#include "Arduino.h"

const bool DEBUG = false;
const long baud = 115200;     
const char sp = ' ';          
const char nl = '\n';         
const int pinT   = 0;          
const int pinQ  = 3;         
const int pinLED = 9;         

// temperature alarm limit
const int limT   = 50;      

// LED levels
const int hiLED   =  60;       // hi LED
const int loLED   = hiLED/16;  // lo LED

char Buffer[64];               
int buffer_index = 0;          
String cmd;                    
float val;                     
int ledStatus;                 // 1: loLED
                               // 2: hiLED
                               // 3: loLED blink
                               // 4: hiLED blink
long ledTimeout = 0;           // when to return LED to normal operation

float LED = 100;               // LED override brightness
float P = 200;                // heater power limit in units of pwm. Range 0 to 255
float Q = 0;                  // last value written to heater in units of percent
int alarmStatus;               // hi temperature alarm status
boolean newData = false;       // boolean flag indicating new command
int n =  10;                   // number of samples for each temperature measurement


void readCommand() {
  while (Serial && (Serial.available() > 0) && (newData == false)) {
    int byte = Serial.read();
    if ((byte != '\r') && (byte != '\n') && (buffer_index < 64)) {
      Buffer[buffer_index] = byte;
      buffer_index++;
    }
    else {
      newData = true;
    }
  }   
}

// for controlling with the serial monitor in Arduino IDE
void echoCommand() {
  if (newData) {
    Serial.write("Received Command: ");
    Serial.write(Buffer, buffer_index);
    Serial.write(nl);
    Serial.flush();
  }
}

// return average  of n reads of thermister temperature in °C
inline float readTemperature(int pin) {
  float degC = 0.0;
  for (int i = 0; i < n; i++) {
    degC += analogRead(pin) * 0.322265625 - 50.0;    // use for 3.3v AREF
  }
  return degC / float(n);
}

void parseCommand(void) {
  if (newData) {
    String read_ = String(Buffer);

    // separate command from associated data
    int idx = read_.indexOf(sp);
    cmd = read_.substring(0, idx);
    cmd.trim();
    cmd.toUpperCase();

    // extract data toFloat() returns 0 on error
    String data = read_.substring(idx + 1);
    data.trim();
    val = data.toFloat();

    // reset parameter for next command
    memset(Buffer, 0, sizeof(Buffer));
    buffer_index = 0;
    newData = false;
  }
}

void sendResponse(String msg) {
  Serial.println(msg);
}

void sendFloatResponse(float val) {
  Serial.println(String(val, 3));
}

void sendBinaryResponse(float val) {
  byte *b = (byte*)&val;
  Serial.write(b, 4);  
}

void dispatchCommand(void) {
  if (cmd == "A") {
    setHeater(0);
    sendResponse("Start");
  }
  else if (cmd == "LED") {
    ledTimeout = millis() + 10000;
    LED = max(0, min(100, val));
    sendResponse(String(LED));
  }
  else if (cmd == "P") {
    P1 = max(0, min(255, val));
    sendResponse(String(P1));
  }
  else if (cmd == "Q") {
    setHeater1(val);
    sendFloatResponse(Q);
  }
  else if (cmd == "Q1B") {
    setHeater1(val);
    sendBinaryResponse(Q);
  }
  else if (cmd == "R") {
    sendFloatResponse(Q);
  }
  else if (cmd == "SCAN") {
    sendFloatResponse(readTemperature(pinT));
    sendFloatResponse(Q);
  }
  else if (cmd == "T") {
    sendFloatResponse(readTemperature(pinT));
  }
  else if (cmd == "T1B") {
    sendBinaryResponse(readTemperature(pinT));
  }
  else if (cmd == "X") {
    setHeater(0);
    sendResponse("Stop");
  }
  else if (cmd.length() > 0) {
    setHeater(0);
    sendResponse(cmd);
  }
  Serial.flush();
  cmd = "";
}

void checkAlarm(void) {
  if (readTemperature(pinT) > limT) {
    alarmStatus = 1;
  }
  else {
    alarmStatus = 0;
  }
}

void updateStatus(void) {
  // determine led status
  ledStatus = 1;
  if (Q1 > 0) {
    ledStatus = 2;
  }
  if (alarmStatus > 0) {
    ledStatus += 2;
  }
  // update led depending on ledStatus
  if (millis() < ledTimeout) {        // override led operation
    analogWrite(pinLED, LED);
  }
  else {
    switch (ledStatus) {
      case 1:  // normal operation, heaters off
        analogWrite(pinLED, loLED);
        break;
      case 2:  // normal operation, heater on
        analogWrite(pinLED, hiLED);
        break;
      case 3:  // high temperature alarm, heater off
        if ((millis() % 2000) > 1000) {
          analogWrite(pinLED, loLED);
        } else {
          analogWrite(pinLED, loLED/4);
        }
        break;
      case 4:  // high temperature alarm, heater on
        if ((millis() % 2000) > 1000) {
          analogWrite(pinLED, hiLED);
        } else {
          analogWrite(pinLED, loLED);
        }
        break;
    }   
  }
}

// set Heater 
void setHeater(float qval) {
  Q = max(0., min(qval, 100.));
  analogWrite(pinQ, (Q*P)/100);
}

// arduino startup
void setup() {
  analogReference(EXTERNAL);
  while (!Serial) {
    ; // wait for serial port to connect.
  }
  Serial.begin(baud);
  Serial.flush();
  setHeater(0);
  ledTimeout = millis() + 1000;
}

// arduino main event loop
void loop() {
  readCommand();
  if (DEBUG) echoCommand();
  parseCommand();
  dispatchCommand();
  checkAlarm();
  updateStatus();
}
