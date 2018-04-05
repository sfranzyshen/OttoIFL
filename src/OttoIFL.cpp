
#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
  #include <pins_arduino.h>
#endif


#include "OttoIFL.h"
#include "Oscillator.h"
#include "US.h"

void Otto::init(int LL, int LR, int FL, int FR, bool load_calibration, int NoiseSensor, 
                int Buzzer, int USTrigger, int USEcho, int AL, int AR) //FIXME: Add SoftwareSerial, RGB LED, Light sensors
{
  servo_pins[0] = LL; 
  servo_pins[1] = LR; // FIXME: setup defines for servos (#define SERVO_LL 0) 
  servo_pins[2] = FL; // and replace where ever numbers are used ...
  servo_pins[3] = FR;
  servo_pins[4] = AL;
  servo_pins[5] = AR;

  attachServos();
  isOttoResting = false;

  if (load_calibration)
  {
    for (int i = 0; i < SERVO_COUNT; i++) 
    {
      // TODO: change EEPROM to read from 0..6
      int servo_trim = EEPROM.read(i);
      if (servo_trim > 128) 
        servo_trim -= 256;
      // TODO: change servo size
      servo[i].SetTrim(servo_trim);
    }
  }

  for (int i = 0; i < SERVO_COUNT; i++) // set everything to 90 (home)
    servo_position[i] = 90;

  //US sensor init with the pins:
  us.init(USTrigger, USEcho);

  //Buzzer & noise sensor pins: 
  pinBuzzer = Buzzer;
  pinNoiseSensor = NoiseSensor;

  pinMode(Buzzer,OUTPUT);
  pinMode(NoiseSensor,INPUT);
  
  ledmatrix.begin(); 
  ledmatrix.setRotated (false);                  // set to rotate the output 90 degrees
  ledmatrix.setFlipRows (true);                 // set to flip the order of the rows, top<>bottom
  ledmatrix.setFlipCols (true);                 // set to flip the order of the columns, left<>right
  ledmatrix.setRowOffset (7);                   // adjust between 0-7 to change the starting row
  ledmatrix.setIntensity(0, MATRIX_BRIGHTNESS); // between 0-15
  ledmatrix.shutdown(0, false);                 // need to start it up i.e. false = start up
  ledmatrix.clearDisplay(0);
  ledmatrix.refresh(0);
  //SoftwareSerial SerialSoft(PIN_SSRx, PIN_SSTx);
  
  // ADCTouch Sensor
  // reference value to account for the capacitance of the pad
  ADCTouch_reference = ADCTouch.read(PIN_ADCTouch, 500);
}

///////////////////////////////////////////////////////////////////
//-- ATTACH & DETACH FUNCTIONS ----------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::attachServos()
{
  // servo[] is an Oscillator object
  for (int i = 0; i < SERVO_COUNT; ++i)
  {
    servo[i].attach(servo_pins[i]);
  }
}

void Otto::detachServos(){
  for (int i = 0; i < SERVO_COUNT; ++i)
  {
    servo[i].detach();
  }
  // servo[0].detach();
  // servo[1].detach();
  // servo[2].detach();
  // servo[3].detach();
}

///////////////////////////////////////////////////////////////////
//-- OSCILLATORS TRIMS ------------------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::setTrims(int LL, int LR, int FL, int FR, int AL, int AR) 
{
  servo[0].SetTrim(LL);
  servo[1].SetTrim(LR);
  servo[2].SetTrim(FL);
  servo[3].SetTrim(FR);
  servo[4].SetTrim(AL);
  servo[5].SetTrim(AR);
}

void Otto::saveTrimsOnEEPROM() 
{  
  for (int i = 0; i < SERVO_COUNT; i++)
  { 
    EEPROM.write(i, servo[i].getTrim());
  } 
      
}


///////////////////////////////////////////////////////////////////
//-- BASIC MOTION FUNCTIONS -------------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::_moveServos(int time, int  servo_target[]) {

  attachServos();
  if (getRestState())
    setRestState(false);

  if(time > 10)
  {
    for (int i = 0; i < 4; i++) 
      increment[i] = ((servo_target[i]) - servo_position[i]) / (time / 10.0);
    final_time =  millis() + time;

    for (int iteration = 1; millis() < final_time; iteration++) 
    {
      partial_time = millis() + 10;
      for (int i = 0; i < 4; i++) servo[i].SetPosition(servo_position[i] + (iteration * increment[i]));
      while (millis() < partial_time); //pause
    }
  }
  else{
    for (int i = 0; i < 4; i++) servo[i].SetPosition(servo_target[i]);
  }
  for (int i = 0; i < 4; i++) servo_position[i] = servo_target[i];
}


void Otto::oscillateServos(int A[4], int O[4], int T, double phase_diff[4], float cycle=1){

  for (int i=0; i<4; i++) {
    servo[i].SetO(O[i]);
    servo[i].SetA(A[i]);
    servo[i].SetT(T);
    servo[i].SetPh(phase_diff[i]);
  }
  double ref=millis();
   for (double x=ref; x<=T*cycle+ref; x=millis()){
     for (int i=0; i<4; i++){
        servo[i].refresh();
     }
  }
}


void Otto::_execute(int A[4], int O[4], int T, double phase_diff[4], float steps = 1.0){

  attachServos();
  if(getRestState()==true){
        setRestState(false);
  }


  int cycles=(int)steps;    

  //-- Execute complete cycles
  if (cycles >= 1) 
    for(int i = 0; i < cycles; i++) 
      oscillateServos(A,O, T, phase_diff);
      
  //-- Execute the final not complete cycle    
  oscillateServos(A,O, T, phase_diff,(float)steps-cycles);
}



///////////////////////////////////////////////////////////////////
//-- HOME = Otto at rest position -------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::home()
{

  if (isOttoResting == false)
  { //Go to rest position only if necessary

    // TODO: 6 servos instead of 4
    int homes[4]={90, 90, 90, 90}; //All the servos at rest position
    _moveServos(500,homes);   //Move the servos in half a second

    detachServos();
    isOttoResting = true;
  }
}

// Getter function for isOttoResting
bool Otto::getRestState()
{
    return isOttoResting;
}

// Setter function for isOttoResting
void Otto::setRestState(bool state)
{
    isOttoResting = state;
}


///////////////////////////////////////////////////////////////////
//-- PREDETERMINED MOTION SEQUENCES -----------------------------//
///////////////////////////////////////////////////////////////////

//---------------------------------------------------------
//-- Otto movement: Jump
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//---------------------------------------------------------
void Otto::jump(float steps, int T){

  int up[]={90,90,150,30};
  _moveServos(T,up);
  int down[]={90,90,90,90};
  _moveServos(T,down);
}


//---------------------------------------------------------
//-- Otto gait: Walking  (forward or backward)    
//--  Parameters:
//--    * steps:  Number of steps
//--    * T : Period
//--    * Dir: Direction: FORWARD / BACKWARD
//---------------------------------------------------------
void Otto::walk(float steps, int T, int dir){

  //-- Oscillator parameters for walking
  //-- Hip sevos are in phase
  //-- Feet servos are in phase
  //-- Hip and feet are 90 degrees out of phase
  //--      -90 : Walk forward
  //--       90 : Walk backward
  //-- Feet servos also have the same offset (for tiptoe a little bit)
  int A[4]= {30, 30, 20, 20};
  int O[4] = {0, 0, 4, -4};
  double phase_diff[4] = {0, 0, DEG2RAD(dir * -90), DEG2RAD(dir * -90)};

  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps);  
}


//---------------------------------------------------------
//-- Otto gait: Turning (left or right)
//--  Parameters:
//--   * Steps: Number of steps
//--   * T: Period
//--   * Dir: Direction: LEFT / RIGHT
//---------------------------------------------------------
void Otto::turn(float steps, int T, int dir){

  //-- Same coordination than for walking (see Otto::walk)
  //-- The Amplitudes of the hip's oscillators are not igual
  //-- When the right hip servo amplitude is higher, the steps taken by
  //--   the right leg are bigger than the left. So, the robot describes an 
  //--   left arc
  int A[4]= {30, 30, 20, 20};
  int O[4] = {0, 0, 4, -4};
  double phase_diff[4] = {0, 0, DEG2RAD(-90), DEG2RAD(-90)}; 
    
  if (dir == LEFT) {  
    A[0] = 30; //-- Left hip servo
    A[1] = 10; //-- Right hip servo
  }
  else {
    A[0] = 10;
    A[1] = 30;
  }
    
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Otto gait: Lateral bend
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    dir: RIGHT=Right bend LEFT=Left bend
//---------------------------------------------------------
void Otto::bend (int steps, int T, int dir){

  //Parameters of all the movements. Default: Left bend
  int bend1[4]={90, 90, 62, 35}; 
  int bend2[4]={90, 90, 62, 105};
  int homes[4]={90, 90, 90, 90};

  //Time of one bend, constrained in order to avoid movements too fast.
  //T=max(T, 600);

  //Changes in the parameters if right direction is chosen 
  if(dir==-1)
  {
    bend1[2]=180-35;
    bend1[3]=180-60;  //Not 65. Otto is unbalanced
    bend2[2]=180-105;
    bend2[3]=180-60;
  }

  //Time of the bend movement. Fixed parameter to avoid falls
  int T2=800; 

  //Bend movement
  for (int i=0;i<steps;i++)
  {
    _moveServos(T2/2,bend1);
    _moveServos(T2/2,bend2);
    delay(T*0.8);
    _moveServos(500,homes);
  }

}


//---------------------------------------------------------
//-- Otto gait: Shake a leg
//--  Parameters:
//--    steps: Number of shakes
//--    T: Period of one shake
//--    dir: RIGHT=Right leg LEFT=Left leg
//---------------------------------------------------------
void Otto::shakeLeg (int steps,int T,int dir){

  //This variable change the amount of shakes
  int numberLegMoves=2;

  //Parameters of all the movements. Default: Right leg
  int shake_leg1[4]={90, 90, 58, 35};   
  int shake_leg2[4]={90, 90, 58, 120};
  int shake_leg3[4]={90, 90, 58, 60};
  int homes[4]={90, 90, 90, 90};

  //Changes in the parameters if left leg is chosen
  if(dir==-1)      
  {
    shake_leg1[2]=180-35;
    shake_leg1[3]=180-58;
    shake_leg2[2]=180-120;
    shake_leg2[3]=180-58;
    shake_leg3[2]=180-60;
    shake_leg3[3]=180-58;
  }
  
  //Time of the bend movement. Fixed parameter to avoid falls
  int T2=1000;    
  //Time of one shake, constrained in order to avoid movements too fast.            
  T=T-T2;
  T=max(T,200*numberLegMoves);  

  for (int j=0; j<steps;j++)
  {
  //Bend movement
  _moveServos(T2/2,shake_leg1);
  _moveServos(T2/2,shake_leg2);
  
    //Shake movement
    for (int i=0;i<numberLegMoves;i++)
    {
    _moveServos(T/(2*numberLegMoves),shake_leg3);
    _moveServos(T/(2*numberLegMoves),shake_leg2);
    }
    _moveServos(500,homes); //Return to home position
  }
  
  delay(T);
}


//---------------------------------------------------------
//-- Otto movement: up & down
//--  Parameters:
//--    * steps: Number of jumps
//--    * T: Period
//--    * h: Jump height: SMALL / MEDIUM / BIG 
//--              (or a number in degrees 0 - 90)
//---------------------------------------------------------
void Otto::updown(float steps, int T, int h){

  //-- Both feet are 180 degrees out of phase
  //-- Feet amplitude and offset are the same
  //-- Initial phase for the right foot is -90, so that it starts
  //--   in one extreme position (not in the middle)
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h, -h};
  double phase_diff[4] = {0, 0, DEG2RAD(-90), DEG2RAD(90)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Otto movement: swinging side to side
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//---------------------------------------------------------
void Otto::swing(float steps, int T, int h){

  //-- Both feets are in phase. The offset is half the amplitude
  //-- It causes the robot to swing from side to side
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h/2, -h/2};
  double phase_diff[4] = {0, 0, DEG2RAD(0), DEG2RAD(0)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Otto movement: swinging side to side without touching the floor with the heel
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//---------------------------------------------------------
void Otto::tiptoeSwing(float steps, int T, int h){

  //-- Both feets are in phase. The offset is not half the amplitude in order to tiptoe
  //-- It causes the robot to swing from side to side
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h, -h};
  double phase_diff[4] = {0, 0, 0, 0};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Otto gait: Jitter 
//--  Parameters:
//--    steps: Number of jitters
//--    T: Period of one jitter 
//--    h: height (Values between 5 - 25)   
//---------------------------------------------------------
void Otto::jitter(float steps, int T, int h){

  //-- Both feet are 180 degrees out of phase
  //-- Feet amplitude and offset are the same
  //-- Initial phase for the right foot is -90, so that it starts
  //--   in one extreme position (not in the middle)
  //-- h is constrained to avoid hit the feets
  h=min(25,h);
  int A[4]= {h, h, 0, 0};
  int O[4] = {0, 0, 0, 0};
  double phase_diff[4] = {DEG2RAD(-90), DEG2RAD(90), 0, 0};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Otto gait: Ascending & turn (Jitter while up&down)
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    h: height (Values between 5 - 15) 
//---------------------------------------------------------
void Otto::ascendingTurn(float steps, int T, int h){

  //-- Both feet and legs are 180 degrees out of phase
  //-- Initial phase for the right foot is -90, so that it starts
  //--   in one extreme position (not in the middle)
  //-- h is constrained to avoid hit the feets
  h=min(13,h);
  int A[4]= {h, h, h, h};
  int O[4] = {0, 0, h+4, -h+4};
  double phase_diff[4] = {DEG2RAD(-90), DEG2RAD(90), DEG2RAD(-90), DEG2RAD(90)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Otto gait: Moonwalker. Otto moves like Michael Jackson
//--  Parameters:
//--    Steps: Number of steps
//--    T: Period
//--    h: Height. Typical valures between 15 and 40
//--    dir: Direction: LEFT / RIGHT
//---------------------------------------------------------
void Otto::moonwalker(float steps, int T, int h, int dir){

  //-- This motion is similar to that of the caterpillar robots: A travelling
  //-- wave moving from one side to another
  //-- The two Otto's feet are equivalent to a minimal configuration. It is known
  //-- that 2 servos can move like a worm if they are 120 degrees out of phase
  //-- In the example of Otto, the two feet are mirrored so that we have:
  //--    180 - 120 = 60 degrees. The actual phase difference given to the oscillators
  //--  is 60 degrees.
  //--  Both amplitudes are equal. The offset is half the amplitud plus a little bit of
  //-   offset so that the robot tiptoe lightly
 
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h/2+2, -h/2 -2};
  int phi = -dir * 90;
  double phase_diff[4] = {0, 0, DEG2RAD(phi), DEG2RAD(-60 * dir + phi)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//----------------------------------------------------------
//-- Otto gait: Crusaito. A mixture between moonwalker and walk
//--   Parameters:
//--     steps: Number of steps
//--     T: Period
//--     h: height (Values between 20 - 50)
//--     dir:  Direction: LEFT / RIGHT
//-----------------------------------------------------------
void Otto::crusaito(float steps, int T, int h, int dir){

  int A[4]= {25, 25, h, h};
  int O[4] = {0, 0, h/2+ 4, -h/2 - 4};
  double phase_diff[4] = {90, 90, DEG2RAD(0), DEG2RAD(-60 * dir)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//---------------------------------------------------------
//-- Otto gait: Flapping
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//--    h: height (Values between 10 - 30)
//--    dir: direction: FOREWARD, BACKWARD
//---------------------------------------------------------
void Otto::flapping(float steps, int T, int h, int dir){

  int A[4]= {12, 12, h, h};
  int O[4] = {0, 0, h - 10, -h + 10};
  double phase_diff[4] = {DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90 * dir), DEG2RAD(90 * dir)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


///////////////////////////////////////////////////////////////////
//-- MOUTHS & ANIMATIONS ----------------------------------------//
///////////////////////////////////////////////////////////////////

unsigned long int Otto::getMouthShape(int number){
  unsigned long int types []={zero_code,one_code,two_code,three_code,four_code,five_code,six_code,seven_code,eight_code,
  nine_code,smile_code,happyOpen_code,happyClosed_code,heart_code,bigSurprise_code,smallSurprise_code,tongueOut_code,
  vamp1_code,vamp2_code,lineMouth_code,confused_code,diagonal_code,sad_code,sadOpen_code,sadClosed_code,
  okMouth_code, xMouth_code,interrogation_code,thunder_code,culito_code,angry_code};

  return types[number];
}


unsigned long int Otto::getAnimShape(int anim, int index){

  unsigned long int littleUuh_code[]={
     0b00000000000000001100001100000000,
     0b00000000000000000110000110000000,
     0b00000000000000000011000011000000,
     0b00000000000000000110000110000000,
     0b00000000000000001100001100000000,
     0b00000000000000011000011000000000,
     0b00000000000000110000110000000000,
     0b00000000000000011000011000000000  
  };

  unsigned long int dreamMouth_code[]={
     0b00000000000000000000110000110000,
     0b00000000000000010000101000010000,  
     0b00000000011000100100100100011000,
     0b00000000000000010000101000010000           
  };

  unsigned long int adivinawi_code[]={
     0b00100001000000000000000000100001,
     0b00010010100001000000100001010010,
     0b00001100010010100001010010001100,
     0b00000000001100010010001100000000,
     0b00000000000000001100000000000000,
     0b00000000000000000000000000000000
  };

  unsigned long int wave_code[]={
     0b00001100010010100001000000000000,
     0b00000110001001010000100000000000,
     0b00000011000100001000010000100000,
     0b00000001000010000100001000110000,
     0b00000000000001000010100100011000,
     0b00000000000000100001010010001100,
     0b00000000100000010000001001000110,
     0b00100000010000001000000100000011,
     0b00110000001000000100000010000001,
     0b00011000100100000010000001000000    
  };

  switch  (anim){

    case littleUuh:
        return littleUuh_code[index];
        break;
    case dreamMouth:
        return dreamMouth_code[index];
        break;
    case adivinawi:
        return adivinawi_code[index];
        break;
    case wave:
        return wave_code[index];
        break;    
  }   
}


void Otto::putAnimationMouth(unsigned long int aniMouth, int index){

      writeFull(getAnimShape(aniMouth,index));
}

void Otto::putMouth(unsigned long int mouth, bool predefined){

  if (predefined){
    writeFull(getMouthShape(mouth));
  }
  else{
    writeFull(mouth);
  }
}

void Otto::writeFull(unsigned long value) {
  ledmatrix.clearDisplay(0); // clear display?
  for (int r=0; r<5;r++) { // 0 - 4 (5)
    for (int c=0; c<6; c++) { // 0 - 5 (6)
      ledmatrix.setLed(0, 7-r, 6-c, (1L & (value >> r*6+c)));
    }
  }
  ledmatrix.refresh(0);  // write the changes we just made to the displa
}

void Otto::clearMouth() {
  ledmatrix.clearDisplay(0);
  ledmatrix.refresh(0);  // write the changes we just made to the displa
}

///////////////////////////////////////////////////////////////////
//-- SENSORS FUNCTIONS  -----------------------------------------//
///////////////////////////////////////////////////////////////////

//---------------------------------------------------------
//-- Otto getDistance: return Otto's ultrasonic sensor measure
//---------------------------------------------------------
float Otto::getDistance(){

  return us.read();
}


//---------------------------------------------------------
//-- Otto getNoise: return Otto's noise sensor measure
//---------------------------------------------------------
int Otto::getNoise(){

  int noiseLevel = 0;
  int noiseReadings = 0;
  int numReadings = 2;  

    noiseLevel = analogRead(pinNoiseSensor);

    for(int i=0; i<numReadings; i++){
        noiseReadings += analogRead(pinNoiseSensor);
        delay(4); // delay in between reads for stability
    }

    noiseLevel = noiseReadings / numReadings;

    return noiseLevel;
}


//---------------------------------------------------------
//-- Otto getBatteryLevel: return battery voltage percent
//---------------------------------------------------------
double Otto::getBatteryLevel(){

  //The first read of the batery is often a wrong reading, so we will discard this value. 
    double batteryLevel = battery.readBatPercent();
    double batteryReadings = 0;
    int numReadings = 10;

    for(int i=0; i<numReadings; i++){
        batteryReadings += battery.readBatPercent();
        delay(1); // delay in between reads for stability
    }

    batteryLevel = batteryReadings / numReadings;

    return batteryLevel;
}


double Otto::getBatteryVoltage(){

  //The first read of the batery is often a wrong reading, so we will discard this value. 
    double batteryLevel = battery.readBatVoltage();
    double batteryReadings = 0;
    int numReadings = 10;

    for(int i=0; i<numReadings; i++){
        batteryReadings += battery.readBatVoltage();
        delay(1); // delay in between reads for stability
    }

    batteryLevel = batteryReadings / numReadings;

    return batteryLevel;
}

///////////////////////////////////////////////////////////////////
//-- SOUNDS -----------------------------------------------------//
///////////////////////////////////////////////////////////////////

void Otto::_tone (float noteFrequency, long noteDuration, int silentDuration){

    // tone(10,261,500);
    // delay(500);

      if(silentDuration==0){silentDuration=1;}

      tone(Otto::pinBuzzer, noteFrequency, noteDuration);
      delay(noteDuration);       //milliseconds to microseconds
      //noTone(PIN_Buzzer);
      delay(silentDuration);     
}
///////
void Otto::_playNote(float noteFrequency, long noteDuration)
{
  tone(Otto::pinBuzzer, noteFrequency, noteDuration);
  int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    noTone(Otto::pinBuzzer);
}

void Otto::bendTones (float initFrequency, float finalFrequency, float prop, long noteDuration, int silentDuration){

  //Examples:
  //  bendTones (880, 2093, 1.02, 18, 1);
  //  bendTones (note_A5, note_C7, 1.02, 18, 0);

  if(silentDuration==0){silentDuration=1;}

  if(initFrequency < finalFrequency)
  {
      for (int i=initFrequency; i<finalFrequency; i=i*prop) {
          _tone(i, noteDuration, silentDuration);
      }

  } else{

      for (int i=initFrequency; i>finalFrequency; i=i/prop) {
          _tone(i, noteDuration, silentDuration);
      }
  }
}


void Otto::sing(int songName){
  switch(songName){

    case S_connection:
      _tone(note_E5,50,30);
      _tone(note_E6,55,25);
      _tone(note_A6,60,10);
    break;

    case S_disconnection:
      _tone(note_E5,50,30);
      _tone(note_A6,55,25);
      _tone(note_E6,50,10);
    break;

    case S_buttonPushed:
      bendTones (note_E6, note_G6, 1.03, 20, 2);
      delay(30);
      bendTones (note_E6, note_D7, 1.04, 10, 2);
    break;

    case S_mode1:
      bendTones (note_E6, note_A6, 1.02, 30, 10);  //1318.51 to 1760
    break;

    case S_mode2:
      bendTones (note_G6, note_D7, 1.03, 30, 10);  //1567.98 to 2349.32
    break;

    case S_mode3:
      _tone(note_E6,50,100); //D6
      _tone(note_G6,50,80);  //E6
      _tone(note_D7,300,0);  //G6
    break;

    case S_surprise:
      bendTones(800, 2150, 1.02, 10, 1);
      bendTones(2149, 800, 1.03, 7, 1);
    break;

    case S_OhOoh:
      bendTones(880, 2000, 1.04, 8, 3); //A5 = 880
      delay(200);

      for (int i=880; i<2000; i=i*1.04) {
           _tone(note_B5,5,10);
      }
    break;

    case S_OhOoh2:
      bendTones(1880, 3000, 1.03, 8, 3);
      delay(200);

      for (int i=1880; i<3000; i=i*1.03) {
          _tone(note_C6,10,10);
      }
    break;

    case S_cuddly:
      bendTones(700, 900, 1.03, 16, 4);
      bendTones(899, 650, 1.01, 18, 7);
    break;

    case S_sleeping:
      bendTones(100, 500, 1.04, 10, 10);
      delay(500);
      bendTones(400, 100, 1.04, 10, 1);
    break;

    case S_happy:
      bendTones(1500, 2500, 1.05, 20, 8);
      bendTones(2499, 1500, 1.05, 25, 8);
    break;

    case S_superHappy:
      bendTones(2000, 6000, 1.05, 8, 3);
      delay(50);
      bendTones(5999, 2000, 1.05, 13, 2);
    break;

    case S_happy_short:
      bendTones(1500, 2000, 1.05, 15, 8);
      delay(100);
      bendTones(1900, 2500, 1.05, 10, 8);
    break;

    case S_sad:
      bendTones(880, 669, 1.02, 20, 200);
    break;

    case S_confused:
      bendTones(1000, 1700, 1.03, 8, 2); 
      bendTones(1699, 500, 1.04, 8, 3);
      bendTones(1000, 1700, 1.05, 9, 10);
    break;

    case S_fart1:
      bendTones(1600, 3000, 1.02, 2, 15);
    break;

    case S_fart2:
      bendTones(2000, 6000, 1.02, 2, 20);
    break;

    case S_fart3:
      bendTones(1600, 4000, 1.02, 2, 20);
      bendTones(4000, 3000, 1.02, 2, 20);
    break;

  }
}
///////////////////////////////////////////////////////////////////
void Otto::move(int moveID,int time, int _moveSize){
  int T = time;
  int moveSize;
  moveSize = _moveSize;
  switch (moveID) {
    case 0:
      home();
      break;
    case 1: //M 1 1000 
      walk(1,T,1);
      break;
    case 2: //M 2 1000 
      walk(1,T,-1);
      break;
    case 3: //M 3 1000 
     turn(1,T,1);
      break;
    case 4: //M 4 1000 
      turn(1,T,-1);
      break;
    case 5: //M 5 1000 30 
      updown(1,T,moveSize);
      break;
    case 6: //M 6 1000 30
   
      moonwalker(1,T,moveSize,1);
      break;
    case 7: //M 7 1000 30
     
      moonwalker(1,T,moveSize,-1);
      break;
    case 8: //M 8 1000 30
    
      swing(1,T,moveSize);
      break;
    case 9: //M 9 1000 30 
   
      crusaito(1,T,moveSize,1);
      break;
    case 10: //M 10 1000 30
     
      crusaito(1,T,moveSize,-1);
      break;
    case 11: //M 11 1000 
      jump(1,T);
      break;
    case 12: //M 12 1000 30
    
      flapping(1,T,moveSize,1);
      break;
    case 13: //M 13 1000 30
    
      flapping(1,T,moveSize,-1);
      break;
    case 14: //M 14 1000 20
    
      tiptoeSwing(1,T,moveSize);
      break;
    case 15: //M 15 500 
      bend(1,T,1);
      break;
    case 16: //M 16 500 
      bend(1,T,-1);
      break;
    case 17: //M 17 500 
      shakeLeg(1,T,1);
      break;
    case 18: //M 18 500 
      shakeLeg(1,T,-1);
      break;
    case 19: //M 19 500 20
    
      jitter(1,T,moveSize);
      break;
    case 20: //M 20 500 15
    
      ascendingTurn(1,T,moveSize);
      break;
    default:
      break;
  }

  
}



///////////////////////////////////////////////////////////////////
//-- GESTURES ---------------------------------------------------//
///////////////////////////////////////////////////////////////////

void Otto::playGesture(int gesture){

  int sadPos[4]=      {110, 70, 20, 160};
  int bedPos[4]=      {100, 80, 60, 120};
  int fartPos_1[4]=   {90, 90, 145, 122}; //rightBend
  int fartPos_2[4]=   {90, 90, 80, 122};
  int fartPos_3[4]=   {90, 90, 145, 80};
  int confusedPos[4]= {110, 70, 90, 90};
  int angryPos[4]=    {90, 90, 70, 110};
  int headLeft[4]=    {110, 110, 90, 90};
  int headRight[4]=   {70, 70, 90, 90};
  int fretfulPos[4]=  {90, 90, 90, 110};
  int bendPos_1[4]=   {90, 90, 70, 35};
  int bendPos_2[4]=   {90, 90, 55, 35};
  int bendPos_3[4]=   {90, 90, 42, 35};
  int bendPos_4[4]=   {90, 90, 34, 35};
  
  switch(gesture){

    case OttoHappy: 
        _tone(note_E5,50,30);
        putMouth(smile);
        sing(S_happy_short);
        swing(1,800,20); 
        sing(S_happy_short);

        home();
       putMouth(happyOpen);
    break;


    case OttoSuperHappy:
       putMouth(happyOpen);
        sing(S_happy);
       putMouth(happyClosed);
        tiptoeSwing(1,500,20);
       putMouth(happyOpen);
        sing(S_superHappy);
        putMouth(happyClosed);
        tiptoeSwing(1,500,20); 

        home();  
        putMouth(happyOpen);
    break;


    case OttoSad: 
       putMouth(sad);
        _moveServos(700, sadPos);     
        bendTones(880, 830, 1.02, 20, 200);
       putMouth(sadClosed);
        bendTones(830, 790, 1.02, 20, 200);  
       putMouth(sadOpen);
        bendTones(790, 740, 1.02, 20, 200);
        putMouth(sadClosed);
        bendTones(740, 700, 1.02, 20, 200);
       putMouth(sadOpen);
        bendTones(700, 669, 1.02, 20, 200);
        putMouth(sad);
        delay(500);

        home();
        delay(300);
        putMouth(happyOpen);
    break;


    case OttoSleeping:
        _moveServos(700, bedPos);     

        for(int i=0; i<4;i++){
          putAnimationMouth(dreamMouth,0);
          bendTones (100, 200, 1.04, 10, 10);
         putAnimationMouth(dreamMouth,1);
          bendTones (200, 300, 1.04, 10, 10);  
          putAnimationMouth(dreamMouth,2);
          bendTones (300, 500, 1.04, 10, 10);   
          delay(500);
        putAnimationMouth(dreamMouth,1);
          bendTones (400, 250, 1.04, 10, 1); 
         putAnimationMouth(dreamMouth,0);
          bendTones (250, 100, 1.04, 10, 1); 
          delay(500);
        } 

       putMouth(lineMouth);
        sing(S_cuddly);

        home();  
        putMouth(happyOpen);
    break;


    case OttoFart:
        _moveServos(500,fartPos_1);
        delay(300);     
        putMouth(lineMouth);
        sing(S_fart1);  
       putMouth(tongueOut);
        delay(250);
        _moveServos(500,fartPos_2);
        delay(300);
        putMouth(lineMouth);
        sing(S_fart2); 
        putMouth(tongueOut);
        delay(250);
        _moveServos(500,fartPos_3);
        delay(300);
        putMouth(lineMouth);
        sing(S_fart3);
        putMouth(tongueOut);    
        delay(300);

        home(); 
        delay(500); 
       putMouth(happyOpen);
    break;


    case OttoConfused:
        _moveServos(300, confusedPos); 
        putMouth(confused);
        sing(S_confused);
        delay(500);

        home();  
        putMouth(happyOpen);
    break;


    case OttoLove:
        putMouth(heart);
        sing(S_cuddly);
        crusaito(2,1500,15,1);

        home(); 
        sing(S_happy_short);  
       putMouth(happyOpen);
    break;


    case OttoAngry: 
        _moveServos(300, angryPos); 
        putMouth(angry);

        _tone(note_A5,100,30);
        bendTones(note_A5, note_D6, 1.02, 7, 4);
        bendTones(note_D6, note_G6, 1.02, 10, 1);
        bendTones(note_G6, note_A5, 1.02, 10, 1);
        delay(15);
        bendTones(note_A5, note_E5, 1.02, 20, 4);
        delay(400);
        _moveServos(200, headLeft); 
        bendTones(note_A5, note_D6, 1.02, 20, 4);
        _moveServos(200, headRight); 
        bendTones(note_A5, note_E5, 1.02, 20, 4);

        home();  
        putMouth(happyOpen);
    break;


    case OttoFretful: 
        putMouth(angry);
        bendTones(note_A5, note_D6, 1.02, 20, 4);
        bendTones(note_A5, note_E5, 1.02, 20, 4);
        delay(300);
        putMouth(lineMouth);

        for(int i=0; i<4; i++){
          _moveServos(100, fretfulPos);   
          home();
        }

        putMouth(angry);
        delay(500);

        home();  
        putMouth(happyOpen);
    break;


    case OttoMagic:

        //Initial note frecuency = 400
        //Final note frecuency = 1000
        
        // Reproduce the animation four times
        for(int i = 0; i<4; i++){ 

          int noteM = 400; 

            for(int index = 0; index<6; index++){
              putAnimationMouth(adivinawi,index);
              bendTones(noteM, noteM+100, 1.04, 10, 10);    //400 -> 1000 
              noteM+=100;
            }

            clearMouth();
            bendTones(noteM-100, noteM+100, 1.04, 10, 10);  //900 -> 1100

            for(int index = 0; index<6; index++){
              putAnimationMouth(adivinawi,index);
              bendTones(noteM, noteM+100, 1.04, 10, 10);    //1000 -> 400 
              noteM-=100;
            }
        } 
 
        delay(300);
        putMouth(happyOpen);
    break;


    case OttoWave:
        
        // Reproduce the animation four times
        for(int i = 0; i<2; i++){ 

            int noteW = 500; 

            for(int index = 0; index<10; index++){
              putAnimationMouth(wave,index);
              bendTones(noteW, noteW+100, 1.02, 10, 10); 
              noteW+=101;
            }
            for(int index = 0; index<10; index++){
              putAnimationMouth(wave,index);
              bendTones(noteW, noteW+100, 1.02, 10, 10); 
              noteW+=101;
            }
            for(int index = 0; index<10; index++){
              putAnimationMouth(wave,index);
              bendTones(noteW, noteW-100, 1.02, 10, 10); 
              noteW-=101;
            }
            for(int index = 0; index<10; index++){
             putAnimationMouth(wave,index);
              bendTones(noteW, noteW-100, 1.02, 10, 10); 
              noteW-=101;
            }
        }    

       clearMouth();
        delay(100);
        putMouth(happyOpen);
    break;

    case OttoVictory:
        
        putMouth(smallSurprise);
      //  final pos   = {90,90,150,30}
        for (int i = 0; i < 60; ++i){
          int pos[]={90,90,90+i,90-i};  
          _moveServos(10,pos);
          _tone(1600+i*20,15,1);
        }

        putMouth(bigSurprise);
     //   final pos   = {90,90,90,90}
        for (int i = 0; i < 60; ++i){
          int pos[]={90,90,150-i,30+i};  
          _moveServos(10,pos);
          _tone(2800+i*20,15,1);
        }

        putMouth(happyOpen);
        //SUPER HAPPY
        //-----
        tiptoeSwing(1,500,20);
        sing(S_superHappy);
        putMouth(happyClosed);
        tiptoeSwing(1,500,20); 
        //-----

        home();
        clearMouth();
        putMouth(happyOpen);

    break;

    case OttoFail:

        putMouth(sadOpen);
        _moveServos(300,bendPos_1);
        _tone(900,200,1);
        putMouth(sadClosed);
        _moveServos(300,bendPos_2);
        _tone(600,200,1);
        putMouth(confused);
        _moveServos(300,bendPos_3);
        _tone(300,200,1);
        _moveServos(300,bendPos_4);
       putMouth(xMouth);

        detachServos();
        _tone(150,2200,1);
        
        delay(600);
        clearMouth();
        putMouth(happyOpen);
        home();

    break;

  }
}

// Light Sensors
int Otto::getLightLValue()
{
  return (analogRead(PIN_LightL));
}

int Otto::getLightRValue()
{
  return (analogRead(PIN_LightR));
}


/**
 * Checks if the ADCTouch sensor is being touched. If the value
 * returned by the sensor is greater than 40 the sensor is being
 * touched. The ADC touch reference must be subtracted to get
 * accurate reading to account for the capacitance of the pad
 * @return true - sensor is touched
 *         false - sensor is not touched 
 */
bool Otto::isADCTouched()
{
  return ((ADCTouch.read(PIN_ADCTouch) - ADCTouch_reference) > 40);
}