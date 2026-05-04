
enum status {
  LOCKED, UNLOCKED 
};

class windowLock
{
  private:
    enum status lockStatus;
    int pinNum;
  public:
    void lockWindow(){
      digitalWrite(pinNum, HIGH);
      enum status lockStatus = LOCKED;
    }
    void unlockWindow(){
      digitalWrite(pinNum, LOW);
      enum status lockStatus = UNLOCKED;
    }
    void pin (int pin){
      pinNum = pin;
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
      enum status lockStatus = UNLOCKED;
    }
    void printStatus(){
      Serial.print("Status: ");
      Serial.print(lockStatus);
    }
};

windowLock window1;
windowLock window2;

void setup() {
  //pinMode(windowPin, OUTPUT)
  //digitalWrite(windowPin, LOW)
  //window1 = new windowLock(UNLOCKED)
  Serial.begin(9600);
  //windowLock* window1;
  window1.pin(9);
  window2.pin(10);
}

void loop() {
  //windowLock window1{};
  //window1.pin(9);
  window1.printStatus();
  window2.printStatus();
  delay(5000);
  window1.lockWindow();
}
