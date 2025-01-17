#include "helper/helper.h"
#include "hmc5883l/hmc5883l.h"
#include "udp/udp.h"
#include "spi/spi.h"
#include "autobot/autobot.h"
#include <wiringSerial.h>
#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>

#define DRIVE 0
#define speed 1000000
#define portW 23907
#define portR 3301

Spi drive(DRIVE,speed);
HMC5883L compass;
Udp gs(portR);
Helper H;
Autobot A;

int dat,isAuto = -1,size;
unsigned char data;
unsigned char *coods;

double heading;

struct waypoints{
  double destlat;
  double destlon;
};

struct waypoints cord[5];

string message = "";

unsigned char* value;

void kill() {
  // cout<<"Killed";
  static unsigned char *controls = (char)0;
  drive.RW(&controls[0],1);
}


void setup() {
    dat = 0,isAuto = -1,size = 0;
   if(!H.gpsdintialise())
    cout<<"ERROR: initialising gpsd! "<<endl;
    if( hmc5883l_init(&compass) != HMC5883L_OKAY )
      cout << "ERROR: initialising compass!" << endl;
   kill();
}

void Compass() {
  hmc5883l_read(&compass);
  heading = compass._data.orientation_deg ;
  heading = H.maps(heading,0,360,360,0) - 90;
  heading = (heading<0)?heading + 360:heading;
  message = "$," + to_string(heading) + ",!";
  value = (unsigned char*)message.c_str();
}

void removeCoods(int check){
  if(check){
    if(size == 1) cord[0].destlat = 0, cord[0].destlon = 0;
    for(int i = 1;i < size;i++){
      cord[i-1].destlat = cord[i].destlat;
      cord[i-1].destlon = cord[i].destlon;
      // printf("\nLoop: Lat: %.6f, Lon: %6f\n",cord[i-1].destlat,cord[i-1].destlon);
    }
   size--;
 }else{
   for(int i=0;i<size;i++){
     cord[i].destlat = 0;
     cord[i].destlon = 0;
   }
   size = 0;
 }
}

int interrupt(){
  coods = gs.read(0,200000);
  if(coods[0] == '$') {
    removeCoods(0);
    kill();
    cout<<"Autonomous Stopped ";
    return 1;
  }
  return 0;
}


void autonomous() {
 while(1){

  if(interrupt()) break;
  Compass();

  A.destlat = cord[0].destlat,A.destlon = cord[0].destlon;
  // printf("\nAUTO: Lat: %.6f, Lon: %6f\n",cord[0].destlat,cord[0].destlon);
  dat = A.update(heading,H);

  if(dat == -1) break;
  else {
    data = (unsigned char)dat;
    drive.RW(&data,1);
  }
 }
}

void getdestlatlon(string point,int i){
  vector<string> token = H.split(point, ',');
  cord[i].destlat = stod(token[0]);
  cord[i].destlon = stod(token[1]);
}

int parseCoods(unsigned char* coods) {
  string data = H.toString(coods);
  data = data.substr(1, data.size() - 2);
  vector<string> tokens = H.split(data, '!');
  int i = 0;
  for (vector<string>::iterator it = tokens.begin() ; it != tokens.end(); ++it,i++){
    getdestlatlon(*it,i);
  }
  return tokens.size();
}

void keyboard(unsigned char *coods) {
  static int i, length;
  Compass();
  static unsigned char *controls;
  for (i = 0; coods[i]!='>'; i++)
    length = (coods[i] == ',') ? i : length;
  controls[0] = H.parse(coods, 1, mid);
  drive.RW(&controls,1);
}

void check(unsigned char* coods) {
  isAuto = (coods[0] == '$')? 2 : isAuto;
  isAuto = (coods[0] == '#')? 1 : isAuto;
  isAuto = (coods[0] == '<')? 0 : isAuto;
  if (isAuto == 2 && size > 0 ) {
    removeCoods(0);
    cout << "Autonomous Stopped" << endl;
  }
  else if(isAuto == 1){
    if(coods[0] == '#') size = parseCoods(coods);
    cout << "Autonomous running" << endl;
    message = "$," + to_string(heading) + ",%,";
    value = (unsigned char*)message.c_str();
    gs.write(value,portW);
    if(coods[0] == '@' && size > 0){
      autonomous();
      message = "$," + to_string(heading) + ",Final destination: " + to_string(cord[0].destlat) + " " + to_string(cord[0].destlon) + "reached," + "~,";
      value = (unsigned char*)message.c_str();
      gs.write(value,portW);
      removeCoods(1);
    }
    else if(coods[0] == '*' && size > 0){
      for(int i=0;i<size;i++){
        autonomous();
        if(size == 1)
          message = "$," + to_string(heading) + ",Final destination: " + to_string(cord[0].destlat) + " " + to_string(cord[0].destlon) + "reached,";
        else
          message = "$," + to_string(heading) + ",Destination: " + to_string(cord[0].destlat) + " " + to_string(cord[0].destlon) + "reached,";
        value = (unsigned char*)message.c_str();
        gs.write(value,portW);
        removeCoods(1);
      }
    }
  }
  else if(isAuto == 0){
        cout << "Keyboard running" << endl;
        keyboard(coods);
  }
}

void loop() {
  coods = gs.read(1,0);
  if(coods[0] != '0') {
     check(coods);
  }
  else kill();
}

int main(){
    setup();
    while (1) loop();
    return 0;
}
