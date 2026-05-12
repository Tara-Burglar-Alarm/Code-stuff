// SecuritySystem.h
// declarations for all classes used in the burglar alarm system

#ifndef SECURITY_SYSTEM_H
#define SECURITY_SYSTEM_H

#include <Arduino.h>
#include <EEPROM.h>

// pin numbers - only used when calling setup() in Controller::init() and SecuritySystem.ino
// kept here as a reference so they're easy to find and change
const int motionSensorPin = 2;
const int doorSensorPin   = 3;
const int buzzerPin       = 6;
const int solenoidPin     = 7;
const int securityLedPin  = 8;

// EEPROM address where the PIN is stored
// PIN is stored as 5 chars (4 digits + null terminator)
const int EEPROM_PIN_ADDR = 0;
const int PIN_LENGTH      = 4;

// lock state for the window solenoid
enum status { LOCKED, UNLOCKED };

// possible outcomes when a face is checked
enum FaceResult { FACE_NONE, FACE_KNOWN, FACE_UNKNOWN };


// base class for anything that needs a pin set up
// Buzzer, windowLock, Solenoid, MotionSensor and DoorSensor all inherit from this
class Peripheral {
  protected:
    int pinNum;  // accessible by child classes
  public:
    void setPin(int p, int mode);  // sets pinNum and calls pinMode
};


// stores a single intruder detection - timestamp and which sensor fired
class Log {
  private:
    char timestamp[20];
    char trigger[10];
  public:
    Log();
    void set(const char* ts, const char* trig);
    void print();
    bool isEmpty();
};


// holds up to 10 Log objects, overwrites oldest when full
class IntruderLog {
  private:
    Log entries[10];
    int count;
    int head;
  public:
    IntruderLog();
    void init();
    void add(const char* ts, const char* trig);
    void printAll();
    int  getCount();
};


// controls the solenoid that locks the window replaced by solenoid
/*class windowLock : public Peripheral {
  private:
    status lockStatus;
  public:
    void setup(int pin);
    void lockWindow();
    void unlockWindow();
    void printStatus();
};*/


class MotionSensor : public Peripheral {
  public:
    void setup(int pin);
    bool read();
};


class DoorSensor : public Peripheral {
  public:
    void setup(int pin);
    bool read();
};


// buzzer and security LED - both get activated on alarm
class Buzzer : public Peripheral {
  public:
    void setup(int pin);
    void on();
    void off();
    void contactSecurity();
    void clearSecurity();
};


class Solenoid : public Peripheral {
  public:
    void setup(int pin);
    void lock();
    void unlock();
};


// handles PIN storage in EEPROM and checking
// Arduino owns the PIN - python just sends attempts and gets back correct/wrong
class PinManager {
  private:
    char storedPin[PIN_LENGTH + 1];
    void loadFromEEPROM();
    void saveToEEPROM();
  public:
    void init();
    bool check(const char* attempt);
    void update(const char* newPin);
};


// main controller - ties everything together
// sensors, buzzer and solenoid are private so they can only
// be accessed through the controller methods
class Controller {
  private:
    MotionSensor motion;
    DoorSensor   door;
    Buzzer       buzzer;
    Solenoid     solenoid;
    bool systemArmed;
    bool systemStarted;
    bool alarmActive;

    bool exitDelayActive;
    bool entryDelayActive;

    bool motionHandled;  // prevents re-triggering while PIR stays HIGH (~8s)
    bool prevMotionState;
    bool prevDoorState;  // edge detection for door sensor

    unsigned long exitStartTime;
    unsigned long entryStartTime;

    unsigned long EXIT_DELAY;  // 60s to leave after arming
    unsigned long ENTRY_DELAY;  // 30s to enter PIN after known face

    FaceResult lastFaceResult = FACE_NONE;
  public:
    void init();
    void armSystem();
    void finishArming();
    void disarmSystem();
    void checkSensors();
    void handleDelays();
    void handleSerial();
    void armedCheck();
    void systemStart();
};

#endif
