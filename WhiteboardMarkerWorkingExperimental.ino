#include <Servo.h>
#define ENC_K 3840.0 // Edges for one rotation of the encoder
#define trackDist 600
#define wheelRad 64
#define LEFTPWM (150)
#define RIGHTPWM (150)
#define MULT 1

#define lineServoDefault 0
#define lineServoTarget 90
#define dropServoDefault 0
#define dropServoTarget 90

#define loopFrequency 20
#define integralSmall 10
#define integralLarge 20
#define lineDetectThreshold 300

#define XspinTime 1000
//#define yellowStraightTime 1000
#define blackLineTime 1000
#define drawTime 3000
#define afterLineTime 300
#define dropTime 2000
#define finalDriveTime 100

//colour sensor pins please change
int i = 0;
int S0 = 43;
int S1 = 45;
int S2 = 47;
int S3 = 49;
int OUT = 51;
float oldOffsetLeft = 0;
float oldOffsetRight = 0;
int red;
int green;
int blue;

int startTime;
int stateTime;
int timeDifference;
float leftError;
float rightError;
int updateTime;
int updateInterval = 20;
float oldAngle;
int state; // replacing this with enum are we Finlay?
bool servoDirection;
int penServoPin = 12;
int liftServoPin = 13;
Servo penServo;
Servo liftServo;


struct pose{
  float X;
  float Y;
  float theta;
};

class PIDcontroller
{
  private:
    float kP;
    float kI;
    float kD;

    float error;
    float prevError;

    float pTerm;
    float iTerm;
    float dTerm;
    
    float integralLowerLim;
    float integralUpperLim;

    float reference;
    float prevReference;

    float prevTime;
    float deltaT;
  public:
    void resetController(){
      error = 0;
      prevError = 0;
      iTerm = 0;
    }

    void setConstants(float P,float I,float D){
      kP = P;
      kI = I;
      kD = D;
    }

    PIDcontroller(float P, float I, float D, float intLowLim, float intUpLim){
      kP = P;
      kI = I;
      kD = D;

      integralLowerLim = intLowLim;
      integralUpperLim = intUpLim;
      
      error = 0;
      prevError = 0;
      iTerm = 0;
      reference = 0;
      prevReference = 0;
      prevTime = (float)millis()/1000;
    }

    float setReference(float ref){
      prevReference = reference;
      reference = ref;
      prevTime = (float)millis()/1000;
    }

    float updateController(float feedback){
      deltaT = (float)millis()/1000 - prevTime;
      prevTime = (float)millis()/1000;
      if(deltaT == 0){
        return 0;
      }
      if (prevReference != reference){
        prevError = error;
      }
      error = reference - feedback;
      dTerm = kD*(error - prevError)/deltaT;
      pTerm = kP*error;
      iTerm += kI*error*deltaT;

      if (iTerm > integralUpperLim){
        iTerm = integralUpperLim;
      }
      else if (iTerm < integralLowerLim){
        iTerm = integralLowerLim;
      }
      prevError = error;
      return (pTerm + iTerm + dTerm);
    }
};

class encoderMotor{
  private:
    volatile long encCount;
    volatile float encRev;
    volatile float wheelAngle;
    int pinEncA;
    int pinEncB;
    int pinPWM;
    int pinAI1;
    int pinAI2;

    bool AI1;
    bool AI2;

    int pwmValue;
    const int pwmLimit = 255;
    void (*channelAptr)(void);
    void (*channelBptr)(void);

  public:
      void handleChannelA() {
      if(digitalRead(pinEncA) == HIGH) {
        if(digitalRead(pinEncB) == LOW){
          encCount++; // Encoder rotating in one direction, e.g. clockwise
        }
        else {// Encoder rotating in the opposite direction, e.g. counter clockwise
        encCount--; 
        }
      }
      else{
        if(digitalRead(pinEncB) == HIGH){
          encCount++; // Encoder rotating in one direction, e.g. clockwise
        }
        else {// Encoder rotating in the opposite direction, e.g. counter clockwise
        encCount--; 
        }
      }  
    encRev = (float)encCount/ENC_K;
    wheelAngle = encRev*360;
    }
    void handleChannelB() 
    {
      if(digitalRead(pinEncB) == HIGH) {
        if(digitalRead(pinEncA) == HIGH){
          encCount++; // Encoder rotating in one direction, e.g. clockwise
        }
        else {// Encoder rotating in the opposite direction, e.g. counter clockwise
        encCount--; 
        }
      }
      else {
        if (digitalRead(pinEncA) == LOW) {
          encCount++; // Encoder rotating in one direction, e.g. clockwise
        }
        else {// Encoder rotating in the opposite direction, e.g. counter clockwise
        encCount--; 
        }
      }
    encRev = (float)encCount/ENC_K;
    wheelAngle = encRev*360;
    }
    encoderMotor(int A1Pin, int A2Pin, int pwmPin){
      pinAI1 = A1Pin;
      pinAI2 = A2Pin;
      pinPWM = pwmPin;

      pwmValue = 0;
      wheelAngle = 0;
      encCount = 0;
      encRev = 0;

      AI1 = false;
      AI2 = false;

      pinMode(pinAI1, OUTPUT);
      pinMode(pinAI2, OUTPUT);
      pinMode(pinPWM, OUTPUT);      
    }
    encoderMotor(int A1Pin, int A2Pin, int pwmPin, int EncAPin, int EncBPin, void (*isrA)(void), void (*isrB)(void)){
      pinAI1 = A1Pin;
      pinAI2 = A2Pin;
      pinPWM = pwmPin;
      pinEncA = EncAPin;
      pinEncB = EncBPin;

      pwmValue = 0;
      wheelAngle = 0;
      encCount = 0;
      encRev = 0;

      AI1 = false;
      AI2 = false;

      pinMode(pinAI1, OUTPUT);
      pinMode(pinAI2, OUTPUT);
      pinMode(pinPWM, OUTPUT);

      pinMode(pinEncA, INPUT);
      pinMode(pinEncB, INPUT);

      attachInterrupt(digitalPinToInterrupt(pinEncA), isrA, CHANGE);
      attachInterrupt(digitalPinToInterrupt(pinEncB), isrB, CHANGE);  
    };
    void setVoltage(float pwmVal){
      pwmValue = (int)(pwmVal+0.5);
      if(pwmValue > 0){
        AI1 = 0;
        AI2 = 1;
      }
      else if(pwmValue < 0){
        pwmValue = -pwmValue;
        AI1 = 1;
        AI2 = 0;
      }
      else{
        AI1 = 0;
        AI2 = 1;
      }

      if(pwmValue > pwmLimit){
        pwmValue = pwmLimit;
      }
      digitalWrite(pinAI1,AI1);
      digitalWrite(pinAI2,AI2);
      analogWrite(pinPWM,pwmValue);
    }
    void setVoltage(float pwmVal, float bounds){
      pwmValue = (int)(pwmVal+0.5);
      if(pwmValue > 0){
        AI1 = 0;
        AI2 = 1;
      }
      else if(pwmValue < 0){
        pwmValue = -pwmValue;
        AI1 = 1;
        AI2 = 0;
      }
      else{
        AI1 = 0;
        AI2 = 1;
      }

      if(pwmValue > pwmLimit || pwmValue > bounds){
        pwmValue = min(pwmLimit, bounds);
      }
      digitalWrite(pinAI1,AI1);
      digitalWrite(pinAI2,AI2);
      analogWrite(pinPWM,pwmValue);
    }
    int getVoltage(){
      return 1;
    }
    float getAngle(){
      return wheelAngle;
    }
    float getAngleRad(){
      return wheelAngle*PI/180.0;
    }
};

class drivetrain{
  private:
    encoderMotor* leftMotor;
    encoderMotor* rightMotor;
    float width;
    float wheelRadius;
    PIDcontroller* LeftContr;
    PIDcontroller* RightContr;
    PIDcontroller* AngularContr;
    float prevTime;
    float deltaT;

    float leftDist;
    float prevLeftDist;
    float rightDist;
    float prevRightDist;
    float deltaLeft;
    float deltaRight;
    float centerDistance;
    float deltaTheta;
  public:
    pose position;
    drivetrain(encoderMotor* left, encoderMotor* right, PIDcontroller* latLeftController, PIDcontroller* latRightController, PIDcontroller* angularController, float trackWidth, float wheelRadii){
        leftMotor = left;
        rightMotor = right;

        LeftContr = latLeftController;
        RightContr = latRightController;
        AngularContr = angularController;

        prevTime = millis();
        deltaT = 0;
        position.X = 0;
        position.Y = 0;
        position.theta = 0;
        width = trackWidth;
        wheelRadius = wheelRadii;
        prevLeftDist = 0;
        leftDist = 0;
        prevRightDist = 0;
        rightDist = 0;
    };
  void updatePosition(){
      deltaLeft = PI*(leftMotor->getAngleRad())*wheelRadius/180;
      deltaRight = PI*(rightMotor->getAngleRad())*wheelRadius/180;
      centerDistance = (deltaLeft + deltaRight)/2;
      deltaTheta = (deltaLeft - deltaRight) / width;
      position.X += centerDistance * cos(position.theta);
      position.Y += centerDistance * sin(position.theta);
      position.theta += deltaTheta;
  }
};

drivetrain* base = nullptr;

encoderMotor* motorLeft = nullptr;
encoderMotor* motorRight = nullptr;

PIDcontroller* lateralPIDLeft = nullptr;
PIDcontroller* lateralPIDRight = nullptr;
PIDcontroller* drivePIDLeft = nullptr;
PIDcontroller* drivePIDRight = nullptr;
PIDcontroller* anglePID = nullptr;

void ISR_motorLeft_channelA() {
  if (motorLeft) motorLeft->handleChannelA();
}
void ISR_motorLeft_channelB() {
  if (motorLeft) motorLeft->handleChannelB();
}
void ISR_motorRight_channelA() {
  if (motorRight) motorRight->handleChannelA();
}
void ISR_motorRight_channelB() {
  if (motorRight) motorRight->handleChannelB();
}

void setup() {
  motorRight = new encoderMotor(31,30,8,21,20,ISR_motorRight_channelA,ISR_motorRight_channelB);
  motorLeft = new encoderMotor(5,6,3,19,18,ISR_motorLeft_channelA,ISR_motorLeft_channelB);
  motorRight->setVoltage(0);
  motorLeft->setVoltage(0);
  state = 0;
  Serial.begin(9600);

  lateralPIDLeft = new PIDcontroller(1.2,0,0,-999,999);
  lateralPIDRight = new PIDcontroller(1.2,0,0,-999,999);
  lateralPIDLeft->setReference(0);
  lateralPIDRight->setReference(0);

  drivePIDLeft = new PIDcontroller(2,0.005,0,-999,999);
  drivePIDRight = new PIDcontroller(2,0.005,0,-999,999);
  anglePID = new PIDcontroller(3,0,0,-999,999);
  base = new drivetrain(
    motorLeft,
    motorRight,
    drivePIDLeft,
    drivePIDRight,
    anglePID,
    trackDist,
    wheelRad
  );
  // colour sensor set pins as input/output
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(OUT, INPUT);

  // set frequency scaling of the colour sensor
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);
  penServo.attach(penServoPin);
  penServo.writeMicroseconds(550);
  delay(100);
  liftServo.attach(liftServoPin);
  //liftServo.writeMicroseconds(500); min
  liftServo.writeMicroseconds(1350);
  delay(5000);
  //liftServo.writeMicroseconds(1900); max
  delay(200);
  penServo.detach();
  liftServo.detach();
  startTime = millis();
  stateTime = millis();
  updateTime = millis();
  oldOffsetLeft = motorLeft->getAngle();
  oldOffsetRight = motorRight->getAngle();
}

void loop() {
  /*leftError = (lateralPIDLeft->updateController(motorLeft->getAngle()));
  motorLeft->setVoltage(leftError);
  rightError = (lateralPIDRight->updateController(motorRight->getAngle()));
  motorRight->setVoltage(rightError);*/
  if(millis() > stateTime + updateInterval){

    if(state == 0){
      motorLeft->setVoltage(LEFTPWM);
      motorRight->setVoltage(-RIGHTPWM);
      if(abs(motorLeft->getAngle()+oldOffsetLeft) + abs(motorRight->getAngle()+oldOffsetRight) > 3200*MULT){
        motorLeft->setVoltage(0);
        motorRight->setVoltage(0);
        startTime = millis();
        state++;
      }
    }
    if(state == 1){
      if(millis() > startTime + 500){
        oldOffsetLeft = motorLeft->getAngle();
        oldOffsetRight = motorRight->getAngle();
        motorLeft->setVoltage(LEFTPWM);
        motorRight->setVoltage(RIGHTPWM);
        startTime = millis();
        state++;
      }
    }
    if(state == 2){
      if(abs(motorLeft->getAngle()-oldOffsetLeft) + abs(motorRight->getAngle()-oldOffsetRight) > 1950.0*MULT){
        oldOffsetLeft = motorLeft->getAngle();
        oldOffsetRight = motorRight->getAngle();              
        motorLeft->setVoltage(0);
        motorRight->setVoltage(0);
        startTime = millis();
        state++;
      }
    }
    if(state == 3){
      if(millis() > startTime + 500){
        oldOffsetLeft = motorLeft->getAngle();
        oldOffsetRight = motorRight->getAngle();
        motorLeft->setVoltage(-LEFTPWM);
        motorRight->setVoltage(RIGHTPWM);
        startTime = millis();
        state++;
      }
    }
    if(state == 4){
      if(abs(motorLeft->getAngle()-oldOffsetLeft) + abs(motorRight->getAngle()-oldOffsetRight) > 550.0*MULT){
        oldOffsetLeft = motorLeft->getAngle();
        oldOffsetRight = motorRight->getAngle();
        motorLeft->setVoltage(0);
        motorRight->setVoltage(0);
        startTime = millis();
        state++;
      }
    }
    if(state == 5){
      if(millis() > startTime + 500){
        oldOffsetLeft = motorLeft->getAngle();
        oldOffsetRight = motorRight->getAngle();
        motorLeft->setVoltage(LEFTPWM);
        motorRight->setVoltage(RIGHTPWM);
        startTime = millis();
        state++;
      }
    }
    if(state == 6){
      if(abs(motorLeft->getAngle()-oldOffsetLeft) + abs(motorRight->getAngle()-oldOffsetRight) > 2820.0*MULT){
        oldOffsetLeft = motorLeft->getAngle();
        oldOffsetRight = motorRight->getAngle();
        motorLeft->setVoltage(0);
        motorRight->setVoltage(0);
        startTime = millis();
        state++;
      }
    }
    if(state == 7){
      if(millis() > startTime + 500){
        oldOffsetLeft = motorLeft->getAngle();
        oldOffsetRight = motorRight->getAngle();
        liftServo.attach(liftServoPin);
        state++;
      }
    }
    if(state == 8){
      liftServo.writeMicroseconds(1350);
      delay(50);
      liftServo.writeMicroseconds(1300);
      delay(50);
      liftServo.writeMicroseconds(1250);
      delay(50);
      liftServo.writeMicroseconds(1200);
      delay(50);
      liftServo.writeMicroseconds(1150);
      delay(50);
      liftServo.writeMicroseconds(1100);
      delay(50);
      liftServo.writeMicroseconds(1050);
      delay(50);
      liftServo.writeMicroseconds(1000);
      delay(50);
      liftServo.writeMicroseconds(950);
      delay(50);
      liftServo.writeMicroseconds(900);
      delay(50);
      liftServo.writeMicroseconds(850);
      delay(50);
      liftServo.writeMicroseconds(800);
      delay(50);
      liftServo.writeMicroseconds(750);
      delay(50);
      liftServo.writeMicroseconds(700);
      delay(50);
      liftServo.writeMicroseconds(650);
      delay(50);
      liftServo.writeMicroseconds(600);
      delay(50);
      liftServo.writeMicroseconds(550);
      delay(50);
      liftServo.writeMicroseconds(500);
      delay(100);
      liftServo.writeMicroseconds(550);
      delay(50);
      liftServo.writeMicroseconds(500);
      delay(300);
      liftServo.writeMicroseconds(550);
      delay(50);
      liftServo.writeMicroseconds(600);
      delay(50);
      liftServo.writeMicroseconds(650);
      delay(50);
      liftServo.writeMicroseconds(700);
      delay(50);
      liftServo.writeMicroseconds(750);
      delay(50);
      liftServo.writeMicroseconds(800);
      delay(50);
      liftServo.writeMicroseconds(850);
      delay(50);
      liftServo.writeMicroseconds(900);
      delay(50);
      liftServo.writeMicroseconds(950);
      delay(50);
      liftServo.writeMicroseconds(1000);
      delay(50);
      liftServo.writeMicroseconds(1050);
      delay(50);
      liftServo.writeMicroseconds(1100);
      delay(50);
      liftServo.writeMicroseconds(1150);
      delay(50);
      liftServo.writeMicroseconds(1200);
      delay(50);
      liftServo.writeMicroseconds(1250);
      delay(50);
      liftServo.writeMicroseconds(1300);
      delay(50);
      liftServo.writeMicroseconds(1350);
      delay(50);
      liftServo.writeMicroseconds(1400);
      delay(50);
      liftServo.writeMicroseconds(1450);
      delay(50);
      liftServo.writeMicroseconds(1500);
      delay(50);
      liftServo.writeMicroseconds(1550);
      delay(50);
      liftServo.writeMicroseconds(1600);
      delay(50);
      liftServo.writeMicroseconds(1650);
      delay(50);
      liftServo.writeMicroseconds(1700);
      delay(50);
      liftServo.writeMicroseconds(1750);
      delay(50);
      liftServo.writeMicroseconds(1800);
      delay(50);
      liftServo.writeMicroseconds(1850);
      delay(50);
      liftServo.writeMicroseconds(1900);
      delay(50);
      liftServo.writeMicroseconds(1850);
      delay(50);
      liftServo.writeMicroseconds(1800);
      delay(50);
      liftServo.writeMicroseconds(1750);
      delay(50);
      liftServo.writeMicroseconds(1700);
      delay(50);
      liftServo.writeMicroseconds(1650);
      delay(50);
      liftServo.writeMicroseconds(1600);
      delay(50);
      liftServo.writeMicroseconds(1550);
      delay(50);
      liftServo.writeMicroseconds(1500);
      delay(50);
      liftServo.writeMicroseconds(1450);
      delay(50);
      liftServo.writeMicroseconds(1400);
      delay(50);
      liftServo.writeMicroseconds(1350);
      delay(100);
      liftServo.detach();
      startTime = millis();
      oldOffsetLeft = motorLeft->getAngle();
      oldOffsetRight = motorRight->getAngle();
      state++;
    }
     if(state == 9){ // Go backwards
      delay(500);
      oldOffsetLeft = motorLeft->getAngle();
      oldOffsetRight = motorRight->getAngle();
      motorLeft->setVoltage(-LEFTPWM);
      motorRight->setVoltage(-RIGHTPWM);
      state++;
    }

    if(state == 10){ // stop
      delay(500);
      oldOffsetLeft = motorLeft->getAngle();
      oldOffsetRight = motorRight->getAngle();              
      motorLeft->setVoltage(0);
      motorRight->setVoltage(0);
      state++;
    }

    if(state == 11 || state == 15){ // turn left
      delay(500);
      oldOffsetLeft = motorLeft->getAngle();
      oldOffsetRight = motorRight->getAngle();
      motorLeft->setVoltage(-LEFTPWM);
      motorRight->setVoltage(RIGHTPWM);
      state++;
    }

    if(state == 12 || state == 16){ // stop
      delay(1250);
      oldOffsetLeft = motorLeft->getAngle();
      oldOffsetRight = motorRight->getAngle();
      motorLeft->setVoltage(0);
      motorRight->setVoltage(0);
      state++;
    }

    if(state == 13){ // Go forwards
      delay(500);
      oldOffsetLeft = motorLeft->getAngle();
      oldOffsetRight = motorRight->getAngle();
      motorLeft->setVoltage(LEFTPWM);
      motorRight->setVoltage(RIGHTPWM);
      state++;
    }

    if(state == 14){ // stop
      delay(1750);
      oldOffsetLeft = motorLeft->getAngle();
      oldOffsetRight = motorRight->getAngle();              
      motorLeft->setVoltage(0);
      motorRight->setVoltage(0);
      state++;
    }

    if(state == 17){ // Go forwards
      delay(500);
      oldOffsetLeft = motorLeft->getAngle();
      oldOffsetRight = motorRight->getAngle();
      motorLeft->setVoltage(LEFTPWM);
      motorRight->setVoltage(RIGHTPWM);
      state++;

    }

    if(state == 18){ // stop
      delay(1500);
      oldOffsetLeft = motorLeft->getAngle();
      oldOffsetRight = motorRight->getAngle();              
      motorLeft->setVoltage(0);
      motorRight->setVoltage(0);
      state++;
    }
    if (state == 19) {
      penServo.attach(penServoPin);

      penServo.writeMicroseconds(2200);
      delay(1000);
      penServo.writeMicroseconds(550);
      delay(1000);
      penServo.detach();
      state++;
    }
    //leftError = (lateralPIDLeft->updateController(motorLeft->getAngle()));
    //rightError = (lateralPIDRight->updateController(motorRight->getAngle()));
    //motorLeft->setVoltage(leftError*1.15,150*1.15);
    //motorRight->setVoltage(rightError,150);
    base->updatePosition();
    stateTime = millis();
  }
  if(millis() > updateTime + 400){
    Serial.print("X: ");
    Serial.println(base->position.X);
    Serial.print("Y: ");
    Serial.println(base->position.Y);
    Serial.print("Theta: ");
    Serial.println(base->position.theta);
    Serial.print("Left: ");
    Serial.println(motorLeft->getAngle());
    Serial.println(leftError);
    Serial.print("Right: ");
    Serial.println(motorRight->getAngle());
    Serial.println(rightError);
    // Serial.println("State: ");
    // Serial.println(state);
    // Serial.println("Colour: ");
    // Serial.println(green);
    //lateralPIDLeft->setReference(360+motorLeft->getAngle());
    //lateralPIDRight->setReference(360+motorRight->getAngle());
    updateTime = millis();
  }
}
