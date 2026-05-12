// SecuritySystem.ino
// entry point - setup and loop only
// all the actual logic is in SecuritySystem.cpp
// the Arduino IDE compiles both files together automatically

#include "SecuritySystem.h"

// these are defined in SecuritySystem.cpp but used here too
//extern windowLock  window1;
extern IntruderLog intruderLog;
extern PinManager  pinManager;
extern bool        systemArmed;
extern bool        systemStarted;

Controller controller;

void setup() {
  Serial.begin(9600);

  // pin numbers are passed into each component's setup()
  // which calls setPin() from the Peripheral base class
  controller.init();
  //window1.setup(solenoidPin); this is old
  pinManager.init();
  intruderLog.init();

  Serial.println("System ready.");

  // give python time to connect before we start processing
  delay(3000);
}

void loop() {
  controller.handleSerial();
  controller.armedCheck();
  /* replaced with armedCheck so we can have contained functions as private
  if (systemStarted && systemArmed) {
    controller.checkSensors();
  }

  // handleDelays runs even before fully armed so the exit countdown works
  if (systemStarted) {
    controller.handleDelays();
  }*/

  delay(100);
}
