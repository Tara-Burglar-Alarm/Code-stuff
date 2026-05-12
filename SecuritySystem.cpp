// SecuritySystem.cpp
// method implementations for all classes in SecuritySystem.h

#include "SecuritySystem.h"

// These should not be global, now private variables in controller defined on setup
/*bool systemArmed      = false;
bool systemStarted    = false;
bool alarmActive      = false;
bool exitDelayActive  = false;
bool entryDelayActive = false;
bool motionHandled    = false;  // stops PIR re-triggering while it stays HIGH
bool prevMotionState  = false;
bool prevDoorState    = false;

unsigned long exitStartTime  = 0;
unsigned long entryStartTime = 0;

const unsigned long EXIT_DELAY  = 60000;  // 1 minute to leave after arming
const unsigned long ENTRY_DELAY = 30000;  // 30 seconds to enter PIN

FaceResult lastFaceResult = FACE_NONE;*/

// global objects needed across multiple methods
IntruderLog intruderLog;
PinManager  pinManager;


// --- Log ---

Log::Log() {
  timestamp[0] = '\0';
  trigger[0]   = '\0';
}

void Log::set(const char* ts, const char* trig) {
  strncpy(timestamp, ts,   sizeof(timestamp) - 1);
  strncpy(trigger,   trig, sizeof(trigger)   - 1);
  timestamp[sizeof(timestamp) - 1] = '\0';
  trigger[sizeof(trigger)   - 1]   = '\0';
}

void Log::print() {
  if (isEmpty()) return;
  Serial.print(timestamp);
  Serial.print(" - ");
  Serial.println(trigger);
}

bool Log::isEmpty() {
  return timestamp[0] == '\0';
}


// --- IntruderLog ---
// circular buffer so we always keep the 10 most recent entries

IntruderLog::IntruderLog() : count(0), head(0) {}

void IntruderLog::init() {
  count = 0;
  head  = 0;
}

void IntruderLog::add(const char* ts, const char* trig) {
  entries[head].set(ts, trig);
  head = (head + 1) % 10;
  if (count < 10) count++;
}

void IntruderLog::printAll() {
  if (count == 0) {
    Serial.println("No intruder logs.");
    return;
  }
  Serial.println("LOG_START");
  int start = (count < 10) ? 0 : head;
  for (int i = 0; i < count; i++) {
    entries[(start + i) % 10].print();
  }
  Serial.println("LOG_END");
}

int IntruderLog::getCount() {
  return count;
}


// --- PinManager ---

void PinManager::loadFromEEPROM() {
  for (int i = 0; i < PIN_LENGTH; i++) {
    storedPin[i] = EEPROM.read(EEPROM_PIN_ADDR + i);
  }
  storedPin[PIN_LENGTH] = '\0';

  // if EEPROM has never been written it returns 255
  // in that case default to 1234
  for (int i = 0; i < PIN_LENGTH; i++) {
    if (storedPin[i] < '0' || storedPin[i] > '9') {
      strcpy(storedPin, "1234");
      saveToEEPROM();
      break;
    }
  }
}

void PinManager::saveToEEPROM() {
  for (int i = 0; i < PIN_LENGTH; i++) {
    EEPROM.write(EEPROM_PIN_ADDR + i, storedPin[i]);
  }
}

void PinManager::init() {
  loadFromEEPROM();
  Serial.println("PIN loaded.");
}

bool PinManager::check(const char* attempt) {
  return strncmp(attempt, storedPin, PIN_LENGTH) == 0;
}

void PinManager::update(const char* newPin) {
  strncpy(storedPin, newPin, PIN_LENGTH);
  storedPin[PIN_LENGTH] = '\0';
  saveToEEPROM();
  Serial.println("PIN_UPDATED");
}


// --- Peripheral ---

void Peripheral::setPin(int p, int mode) {
  pinNum = p;
  pinMode(p, mode);
}


// --- windowLock --- old class replaced by soleniod kept just in case

/*void windowLock::setup(int pin) {
  setPin(pin, OUTPUT);
  digitalWrite(pinNum, LOW);
  lockStatus = UNLOCKED;
}

void windowLock::lockWindow() {
  digitalWrite(pinNum, HIGH);
  lockStatus = LOCKED;
  Serial.println("WINDOW LOCKED");
}

void windowLock::unlockWindow() {
  digitalWrite(pinNum, LOW);
  lockStatus = UNLOCKED;
  Serial.println("WINDOW UNLOCKED");
}

void windowLock::printStatus() {
  Serial.print("Window: ");
  Serial.println(lockStatus == LOCKED ? "LOCKED" : "UNLOCKED");
} */ 


// --- MotionSensor ---

void MotionSensor::setup(int pin) {
  setPin(pin, INPUT);
}

bool MotionSensor::read() {
  return digitalRead(pinNum) == HIGH;
}


// --- DoorSensor ---
// INPUT_PULLUP means LOW = door open

void DoorSensor::setup(int pin) {
  setPin(pin, INPUT_PULLUP);
}

bool DoorSensor::read() {
  return digitalRead(pinNum) == LOW;
}


// --- Buzzer ---

void Buzzer::setup(int pin) {
  setPin(pin, OUTPUT);
  digitalWrite(pinNum, LOW);
}

void Buzzer::on() {
  digitalWrite(pinNum, HIGH);
  //alarmActive = true; no longer global, we gotta call in controller class
  Serial.println("ALARM ON");
  contactSecurity();
}

void Buzzer::off() {
  digitalWrite(pinNum, LOW);
  //alarmActive = false;
  Serial.println("ALARM OFF");
}

void Buzzer::contactSecurity() {
  digitalWrite(securityLedPin, HIGH);
  Serial.println("SECURITY CONTACTED");
}

void Buzzer::clearSecurity() {
  digitalWrite(securityLedPin, LOW);
}


// --- Solenoid ---

void Solenoid::setup(int pin) {
  setPin(pin, OUTPUT);
  digitalWrite(pinNum, LOW);
}

void Solenoid::lock() {
  digitalWrite(pinNum, HIGH);
  Serial.println("LOCKED");
}

void Solenoid::unlock() {
  digitalWrite(pinNum, LOW);
  Serial.println("UNLOCKED");
}


// --- Controller ---

void Controller::init() {
  // pin numbers passed in here - Peripheral base class stores and configures them
  motion.setup(motionSensorPin);
  door.setup(doorSensorPin);
  buzzer.setup(buzzerPin);
  solenoid.setup(solenoidPin);
  systemArmed      = false;
  systemStarted    = true;
  alarmActive      = false;
  exitDelayActive  = false;
  entryDelayActive = false;
  motionHandled    = false;  // stops PIR re-triggering while it stays HIGH
  prevMotionState  = false;
  prevDoorState    = false;

  exitStartTime  = 0;
  entryStartTime = 0;

  EXIT_DELAY  = 60000;  // 1 minute to leave after arming
  ENTRY_DELAY = 30000;  // 30 seconds to enter PIN

  FaceResult lastFaceResult = FACE_NONE;
}

void Controller::armSystem() {
  Serial.println("Arming... 60 seconds to leave.");
  exitDelayActive = true;
  exitStartTime   = millis();
}

void Controller::finishArming() {
  systemArmed     = true;
  exitDelayActive = false;
  solenoid.lock();
  Serial.println("CHECK_FACE"); // testing please delete
  Serial.println("System armed.");
}

void Controller::disarmSystem() {
  systemArmed      = false;
  alarmActive      = false;
  entryDelayActive = false;
  motionHandled    = false;
  lastFaceResult   = FACE_NONE;
  buzzer.off();
  buzzer.clearSecurity();
  solenoid.unlock();
  Serial.println("System disarmed.");
}

void Controller::checkSensors() {
  bool motionNow = motion.read();
  bool doorNow   = door.read();

  // door - only trigger on the moment it opens, not while it stays open
  if (doorNow && !prevDoorState && !alarmActive && !entryDelayActive) {
    Serial.println("Door opened - checking face...");
    Serial.println("CHECK_FACE");
  }

  // PIR stays HIGH for about 8 seconds so we only want the first trigger
  if (motionNow && !prevMotionState && !motionHandled && !alarmActive && !entryDelayActive) {
    Serial.println("Motion detected - checking face...");
    Serial.println("CHECK_FACE");
    motionHandled = true;
  }

  if (!motionNow && prevMotionState) {
    motionHandled = false;
  }

  prevMotionState = motionNow;
  prevDoorState   = doorNow;
}

void Controller::handleDelays() {
  if (exitDelayActive && millis() - exitStartTime >= EXIT_DELAY) {
    finishArming();
  }

  if (entryDelayActive && millis() - entryStartTime >= ENTRY_DELAY) {
    Serial.println("PIN timeout - triggering alarm.");
    buzzer.on();
    alarmActive = true;
    entryDelayActive = false;
  }
}

void Controller::handleSerial() {
  if (!Serial.available()) return;

  String msg = Serial.readStringUntil('\n');
  msg.trim();

  if (msg == "READY")    { Serial.println("Python connected."); return; }
  if (msg == "ARM")      { armSystem();    return; }
  if (msg == "SHUTDOWN") { disarmSystem(); Serial.println("Shutdown."); return; }

  // python sends PIN:xxxx - arduino checks and replies
  if (msg.startsWith("PIN:")) {
    String attempt = msg.substring(4);
    if (pinManager.check(attempt.c_str())) {
      Serial.println("PIN_CORRECT");
      // if system is armed and we're in the entry window, disarm
      if (systemArmed && entryDelayActive) {
        disarmSystem();
      }
    } else {
      Serial.println("PIN_WRONG");
    }
    return;
  }

  // python sends NEWPIN:xxxx - arduino saves it to EEPROM
  if (msg.startsWith("NEWPIN:")) {
    String newPin = msg.substring(7);
    pinManager.update(newPin.c_str());
    return;
  }

  if (msg == "FACE_OK") {
    lastFaceResult   = FACE_KNOWN;
    entryDelayActive = true;
    entryStartTime   = millis();
    Serial.println("Known face - enter PIN within 30 seconds.");
    return;
  }

  if (msg == "FACE_UNKNOWN") {
    lastFaceResult = FACE_UNKNOWN;
    Serial.println("Unknown face - triggering alarm.");
    buzzer.on();
    alarmActive = true;
    return;
  }

  // python sends a timestamp when an intruder is detected
  // we store it in the log array so it can be retrieved later
  if (msg.startsWith("LOG:")) {
    String ts = msg.substring(4);
    intruderLog.add(ts.c_str(), "detected");
    Serial.print("Logged: ");
    Serial.println(ts);
    return;
  }

  if (msg == "FACE_NONE") {
    lastFaceResult = FACE_NONE;
    Serial.println("No face - ignoring.");
    // not setting alarm active (we were before)
    return;
  }

  if (msg == "PIN_TIMEOUT") {
    Serial.println("PIN timeout from python - triggering alarm.");
    buzzer.on();
    entryDelayActive = false;
    alarmActive = true;
    return;
  }

  // python is asking for the stored log entries
  if (msg == "GET_LOGS") {
    intruderLog.printAll();
    return;
  }

  if (msg == "CONTACT_SECURITY") { buzzer.contactSecurity();              return; }
  if (msg == "TRAINING")         { Serial.println("Training...");         return; }
  if (msg == "TRAINING_DONE")    { Serial.println("Training done.");      return; }
  if (msg == "TRAIN_FAILED")     { Serial.println("Training failed.");    return; }
  if (msg == "CAPTURE_READY")    { Serial.println("Capture open on PC."); return; }
  if (msg == "CAPTURE_DONE")     { Serial.println("Capture done.");       return; }
  if (msg == "ERROR_NO_MODEL")   { Serial.println("No model found.");     return; }
  if (msg == "ERROR_NO_DATASET") { Serial.println("No dataset.");         return; }
}

void Controller::armedCheck(){
    if (systemStarted && systemArmed) {
    checkSensors();
  }

  // handleDelays runs even before fully armed so the exit countdown works
  if (systemStarted) {
    handleDelays();
  }
}
