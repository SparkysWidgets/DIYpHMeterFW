/*
 This is the basic OLED screen DIY pH meter firmware based on the LeoPhi hardware(either version, 2 is preferred).
 Some core features, keeping in mind that LeoPhi uses about 30mA during normal (non-conserving) operation usage can
 be limited even with 1000mAh battery. Battery indicator is shown in upper right corner and the different colors of the OLED
 are used to help give visual cues on both battery life and actual pH readings. This is a great example to build from when
 creating a DIY pH Meter. Just the little touches of color indication and rechargeable battery make the hassle of
 building a meter well worth it!

 All code is by Ryan except portions dealing with OLED (they com from the 4D systems demo firmware for Arduino)
 Sparky's Widgets 2012
 http://www.sparkyswidgets.com/Projects/LeoPhi.aspx

 */

//#include <LiquidCrystal.h>


#include <avr/eeprom.h> 
#ifdef LOG_MESSAGES
  #define HWLOGGING Serial
#else
  #define HWLOGGING if (1) {} else Serial
#endif

#include "Goldelox_Serial_4DLib.h"
#include "GoldeloxBigDemo.h" 
#include "Goldelox_const4D.h"
#define DisplaySerial Serial1
Goldelox_Serial_4DLib Display(&DisplaySerial);
bool shouldClearDisplay = true;

// globals for this program
int fmediatests ;

const int PHIN      =      A0;
const int BATTV      =      A9;
const int GREENLED  =      10; //All Status LEDS on PWM, though they sink not source
const int BLUELED   =      11; 
const int REDLED    =       9; 

//LED fade effects
int brightness =      0;
int fadeAmount =      5;

//State machine base variables, do I really need two separate states, easier to code LED heartbeat and adc sample rate
long previousMillis = 0;
long adcMillis = 0;       
int statusInterval = 1000;           // interval at which to blink or send updates(milliseconds)
int adcReadInterval = 20;           // Our ADC read routine should be cycling at about ~50hz (20ms)

//EEPROM trigger check
#define Write_Check      0x1234 
#define VERSION 0x0002

const float vRef = 5.00; //Set our voltage reference (what is out Voltage at the Vin (+) of the ADC in this case an atmega32u4)
const float opampGain = 5.25; //what is our Op-Amps gain (stage 1)

const int buttonPin = 10;
const int buttonPullUp = 5;
int buttonState = 0;

//Rolling average this should act as a digital filter (IE smoothing)
const int numPasses = 200; //Larger number = slower response but greater smoothing effect

int passes[numPasses];    //Our storage array
int passIndex = 0;          //what pass are we on? this one
long total = 0;              //running total
int pHSmooth = 0;           //Our smoothed pHRaw number

//pH calc globals
float milliVolts, pH, battery; //using floats will transmit as 4 bytes over I2C

//Continuous reading flag
bool continousFlag,statusGFlag;

//Our parameter, for ease of use and eeprom access lets use a struct
struct parameters_T
{
  unsigned int WriteCheck;
  int pH7Cal, pH4Cal,pH10Cal;
  bool continous,statusLEDG;
  float pHStep;
} 
params;

enum statesEnum
{
  systemInitialize,
  normal,
  reset,
  calibrate7,
  calibrate4
};
statesEnum currentState;

void setup()  
{
  pinMode(GREENLED, OUTPUT);
  pinMode(REDLED, OUTPUT);
  pinMode(BLUELED, OUTPUT);
  pinMode(4, OUTPUT);
  digitalWrite(REDLED, HIGH);
  digitalWrite(BLUELED, HIGH);
  digitalWrite(GREENLED, HIGH);
  pinMode(PHIN, INPUT);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT); 
  pinMode(buttonPullUp, OUTPUT);
  digitalWrite(buttonPullUp, HIGH);  
  //Serial1.begin(57600); //Enable basic serial commands in base version
  //Setup for OLED screen

  shouldClearDisplay = true;
  Display.TimeLimit4D   = 5000 ; // 2 second timeout on all commands
  Display.Callback4D = Callback ; // NULL ;
  DisplaySerial.begin(9600) ;
  Display.SSTimeout(0);
  //setting state
  currentState = systemInitialize;
  eeprom_read_block(&params, (void *)0, sizeof(params));
  continousFlag = params.continous;
  statusGFlag = params.statusLEDG;
  if (params.WriteCheck != Write_Check){
    reset_Params();
  }
  sendSerialStatusInfo('I');
  // initialize smoothing variables to 0: 
  for (int thisPass = 0; thisPass < numPasses; thisPass++)
    passes[thisPass] = 0;
}

void loop()
{
  //we made it past our setup routine change into our running(reading) state
  //Our smoothing portion
  //subtract the last pass
  unsigned long currentMillis = millis();

  // read the state of the pushbutton value:
  buttonState = digitalRead(buttonPin);

  // check if the pushbutton is pressed.
  //if it is, the buttonState is HIGH:
  if (buttonState == HIGH) {     
    //We are using a pull up pin so our normal state is HIGH
  } 
  else {
    //Button hit detected, cycle state machine state
    if(currentState == systemInitialize)
    {
      Display.gfx_Cls() ;
      Display.txt_MoveCursor(1, 0) ;
      Display.txt_Height(1) ;
      Display.txt_Width(1) ;
      Display.putstr("Calibration");
      Display.txt_MoveCursor(2, 0) ;
      Display.putstr("Put Probe in");
      Display.txt_MoveCursor(3, 0) ;
      Display.putstr("pH 7 solution");
      Display.txt_MoveCursor(4, 0) ;
      Display.putstr("Press Button");
      Display.txt_MoveCursor(5, 0) ;
      Display.putstr("to Calibrate");
      currentState = calibrate7;
      delay(250);
    }
    else if(currentState == calibrate7)
    {
      Display.gfx_Cls() ;
      Display.txt_MoveCursor(1, 0) ;
      Display.txt_Height(1) ;
      Display.txt_Width(1) ;
      Display.putstr("Put Probe in");
      Display.txt_MoveCursor(2, 0) ;
      Display.putstr("pH 4 solution");
      Display.txt_MoveCursor(3, 0) ;
      Display.putstr("Press Button");
      Display.txt_MoveCursor(4, 0) ;
      Display.putstr("to Calibrate");
      currentState = calibrate4;
      params.pH7Cal = pHSmooth;
      eeprom_write_block(&params, (void *)0, sizeof(params));
      delay(250);
    }
    else if(currentState == calibrate4)
    {
      Display.gfx_Cls() ;
      Display.txt_MoveCursor(1, 0) ;
      Display.txt_Height(1) ;
      Display.txt_Width(1) ;
      Display.putstr("Calibrated");
      Display.txt_MoveCursor(2, 0) ;
      Display.putstr("Press Button");
      Display.txt_MoveCursor(3, 0) ;
      Display.putstr("to resume");
      currentState = reset;
      params.pH4Cal = pHSmooth;
      params.pHStep = ((((vRef*(float)(params.pH7Cal - params.pH4Cal))/1024)*1000)/opampGain)/3;
      eeprom_write_block(&params, (void *)0, sizeof(params));
      delay(250);
    }
    else if(currentState == reset)
    {
      display_pHscreen(true);
      currentState = normal;
      delay(250);
    }
    else if(currentState == normal)
    {
      currentState = systemInitialize;
      delay(250);
    }
  }

if(currentMillis - adcMillis > adcReadInterval)
  {
    // save the last time you blinked the LED 
    adcMillis = currentMillis;

    total = total - passes[passIndex];
    //grab our pHRaw this should pretty much always be updated due to our Oversample ISR
    //and place it in our passes array this mimics an analogRead on a pin
    digitalWrite(4, HIGH); 
    passes[passIndex] = smoothADCRead(PHIN);
    digitalWrite(4, LOW); //these will show our sampling fQ checking with scope!
    total = total + passes[passIndex];
    passIndex = passIndex + 1;
    //Now handle end of array and make our rolling average portion
    if(passIndex >= numPasses)
    {
    passIndex = 0;
    }
    
    pHSmooth = total/numPasses;

    //This is for our status LED heartbeats display however needed
    if(statusGFlag)
      {
        analogWrite(BLUELED, brightness);    
        // change the brightness for next time through the loop:
        brightness = brightness + fadeAmount;
        // reverse the direction of the fading at the ends of the fade: 
        if (brightness == 0 || brightness == 255) {
          fadeAmount = -fadeAmount ; 
        }
      }

  }


  if(Serial.available() ) 
  {
    String msgIN = "";
    int msgLength = 18;
    int msgCount = 0;
    char c;
    while(Serial.available() && msgCount < msgLength)
    {
     c = Serial.read();  
     msgIN += c;
     msgCount++;// just a little bit of string length protection
     }
     processMessage(msgIN);
  }

  calcpH();

  if(currentMillis - previousMillis > statusInterval)
  {
    // save the last time you blinked the LED 
    previousMillis = currentMillis;   
    battery = smoothADCRead(BATTV);

    if(continousFlag)
    {
     digitalWrite(REDLED,LOW);
     sendSerialStatusInfo('S');
    }
    digitalWrite(REDLED,HIGH); //we place here in the odd case if you exit C mode while LED is on
    if(currentState == normal || currentState == systemInitialize)
    {
      display_pHscreen(shouldClearDisplay); //we can pass down a clr screen flag if we need <--
      shouldClearDisplay = false; //we had our first run through which is auto clear scenario
    }
  }
}

void calcpH()
{
 float temp = ((((vRef*(float)params.pH7Cal)/1024)*1000)- calcMilliVolts(pHSmooth))/opampGain;
 pH = 7-(temp/params.pHStep);
}

float calcMilliVolts(int numToCalc)
{
 float calcMilliVolts = (((float)numToCalc/1024)*vRef)*1000; //pH smooth is our rolling average mine as well use it
 return calcMilliVolts;
}

int smoothADCRead(int whichPin)
{
  //lets just take a reading and drop it, this should eliminate any ADC multiplexer issues
  int throwAway = analogRead(whichPin);
  int smoothADCRead = analogRead(whichPin);
  return smoothADCRead;
}

void sendSerialStatusInfo(char charStatusInfo)
{
  if(charStatusInfo == 'S')
  {
    Serial.print("pHRaw: ");
    Serial.print(pHSmooth);
    Serial.print(" | ");
    Serial.print("pH: ");
    Serial.print(pH);
    Serial.print(" | ");
    Serial.print("Millivolts: ");
    Serial.print(calcMilliVolts(pHSmooth));
    Serial.print(" | ");
    Serial.print("Batt: ");
    Serial.println((battery/1024)*5);
  }
  if(charStatusInfo == 'I')
  {
   Serial.print("LeoPhi Info: Firmware Ver ");
   Serial.println(VERSION);
   Serial.print("pH 7 cal: ");
   Serial.print(params.pH7Cal);
   Serial.print(" | ");
   Serial.print("pH 4 cal: ");
   Serial.print(params.pH4Cal);
   Serial.print(" | ");
   Serial.print("pH 10 cal: ");
   Serial.print(params.pH10Cal);
   Serial.print(" | ");
   Serial.print("pH probe slope: ");
   Serial.println(params.pHStep);
  }
}

//FIXME: re factor, and don't use Strings bah, it works for now... no long inputs!
void processMessage(String msg)
{
  if(msg.startsWith("L"))
  {
    if (msg.substring(2,1) ==  "0") 
    {
     //Status led visual indication of a working unit on powerup 0 means off
     statusGFlag = false;
     digitalWrite(BLUELED, HIGH);
     Serial.println("Status led off");
     params.statusLEDG = statusGFlag;
     eeprom_write_block(&params, (void *)0, sizeof(params)); 
    }
    if (msg.substring(2,1) ==  "1") 
    {
     //Status led visual indication of a working unit on powerup 0 means off
     statusGFlag = true;
     Serial.println("Status led on");
     params.statusLEDG = statusGFlag;
     eeprom_write_block(&params, (void *)0, sizeof(params));  
    }
     
  }
  if(msg.startsWith("R"))
  {
    //take a pH reading
   calcpH();
   Serial.println(pH); 
  }
  if(msg.startsWith("C"))
  {
     continousFlag = true;
     Serial.println("Continuous Reading On");
     params.continous = continousFlag;
     eeprom_write_block(&params, (void *)0, sizeof(params));
  }
  if(msg.startsWith("E"))
  {
   //exit continuous reading mode
     continousFlag = false;
     Serial.println("Continuous Reading Off");
     params.continous = continousFlag;
     eeprom_write_block(&params, (void *)0, sizeof(params)); 
  }
  if(msg.startsWith("S"))
  {
   //Calibrate to pH7 solution, center on this for zero
   Serial.println("Calibrate 7");
   params.pH7Cal = pHSmooth;
   eeprom_write_block(&params, (void *)0, sizeof(params));
  }
  if(msg.startsWith("F"))
  {
   //calibrate to pH4 solution, recalculate our slope to account for probe
   Serial.println("Calibrate 4");
   params.pH4Cal = pHSmooth;
   //RefVoltage * our deltaRawpH / 10bit steps *mV in V / OP-Amp gain /pH step difference 7-4
   params.pHStep = ((((vRef*(float)(params.pH7Cal - params.pH4Cal))/1024)*1000)/opampGain)/3;
   eeprom_write_block(&params, (void *)0, sizeof(params));
  }
  if(msg.startsWith("T"))
  {
   //calibrate to pH10 solution, recalculate our slope to account for probe 
   Serial.println("Calibrate 10");
   params.pH10Cal = pHSmooth;
   //RefVoltage * our deltaRawpH / 10bit steps *mV in V / OP-Amp gain /pH step difference 10-7
   params.pHStep = ((((vRef*(float)(params.pH10Cal - params.pH7Cal))/1024)*1000)/opampGain)/3;
   eeprom_write_block(&params, (void *)0, sizeof(params));
  }
    if(msg.startsWith("I"))
  {
   //Lets read in our parameters and spit out the info! 
   eeprom_read_block(&params, (void *)0, sizeof(params));
   sendSerialStatusInfo('I');
  }
    if(msg.startsWith("X"))
  {
    //restore to default settings
    Serial.println("Reseting to default settings");
    reset_Params();
  }
}

void reset_Params(void)
{
  //Restore to default set of parameters!
  params.WriteCheck = Write_Check;
  params.statusLEDG = true;
  params.continous = false; //toggle continuous readings
  params.pH7Cal = 512; //assume ideal probe and amp conditions 1/2 of 4096
  params.pH4Cal = 382; //using ideal probe slope we end up this many 10bit units away on the 4 scale
  params.pH10Cal = 890;//using ideal probe slope we end up this many 10bit units away on the 10 scale
  params.pHStep = 59.16;//ideal probe slope
  eeprom_write_block(&params, (void *)0, sizeof(params)); //write these settings back to eeprom
}

////////////////////////Display Stuff//////////////////////////////

void SetThisBaudrate(int Newrate)
{
  int br ;
  DisplaySerial.flush() ;
  DisplaySerial.end() ;
  switch(Newrate)
  {
    case BAUD_110    : br = 110 ;
      break ;
    case BAUD_300    : br = 300 ;
      break ;
    case BAUD_600    : br = 600 ;
      break ;
    case BAUD_1200   : br = 1200 ;
      break ;
    case BAUD_2400   : br = 2400 ;
      break ;
    case BAUD_4800   : br = 4800 ;
      break ;
    case BAUD_9600   : br = 9600 ;
      break ;
    case BAUD_14400  : br = 14400 ;
      break ;
    case BAUD_19200  : br = 19200 ;
      break ;
    case BAUD_31250  : br = 31250 ;
      break ;
    case BAUD_38400  : br = 38400 ;
      break ;
    case BAUD_56000  : br = 56000 ;
      break ;
    case BAUD_57600  : br = 57600 ;
      break ;
    case BAUD_115200 : br = 115200 ;
      break ;
    case BAUD_128000 : br = 133928 ; // actual rate is not 128000 ;
      break ;
    case BAUD_256000 : br = 281250 ; // actual rate is not  256000 ;
      break ;
    case BAUD_300000 : br = 312500 ; // actual rate is not  300000 ;
      break ;
    case BAUD_375000 : br = 401785 ; // actual rate is not  375000 ;
      break ;
    case BAUD_500000 : br = 562500 ; // actual rate is not  500000 ;
      break ;
    case BAUD_600000 : br = 703125 ; // actual rate is not  600000 ;
      break ;
  }
  DisplaySerial.begin(br) ;
  delay(50) ; // Display sleeps for 100
  DisplaySerial.flush() ;
}

void setbaudWait(word  Newrate)
{
  DisplaySerial.print((char)(F_setbaudWait >> 8));
  DisplaySerial.print((char)(F_setbaudWait));
  DisplaySerial.print((char)(Newrate >> 8));
  DisplaySerial.print((char)(Newrate));
  SetThisBaudrate(Newrate); // change this systems baud rate to match new display rate, ACK is 100ms away
  Display.GetAck() ;
}

int trymount(void)
{
#define retries 15
  int i ;
  int j ;
  i = Display.media_Init() ;
  j = 0 ;
  if (!i)
  {
    Display.putstr("Pls insert uSD crd\n") ;
    while (   (!i)
          && (j < retries) )
    {
      Display.putstr(".") ;
      i = Display.media_Init() ;
      j++ ;
    }
    Display.putstr("\n") ;
  }
  if (j == retries)
    return FALSE ;
  else
    return TRUE ;
}

void display_pHscreen(bool shouldClear)
{
  if(shouldClear) Display.gfx_Cls() ;

  HWLOGGING.println(F("Text Tests")) ;

  //what mode are we in
  Display.putstr("pH meter") ;
  Display.gfx_Line(2,10,94,10,BLUE) ;
  
  //draw battery outline upper right corner
  Display.gfx_Line(82,2,92,2,GREEN) ;
  Display.gfx_Line(82,6,92,6,GREEN) ;
  Display.gfx_Line(80,3,80,6,GREEN) ;
  Display.gfx_Line(81,3,81,6,GREEN) ;
  Display.gfx_Line(92,2,92,7,GREEN) ;
  //now draw battery status
  if(battery >= 850)
  {
  Display.gfx_Line(82,3,92,3,BLUE) ;
  Display.gfx_Line(82,4,92,4,BLUE) ;
  Display.gfx_Line(82,4,92,4,BLUE) ;
  Display.gfx_Line(82,5,92,5,BLUE) ;
  }
  if(battery >= 800 && battery <= 849 )
  {
  Display.gfx_Line(82,3,92,3,GREEN) ;
  Display.gfx_Line(82,4,92,4,GREEN) ;
  Display.gfx_Line(82,4,92,4,GREEN) ;
  Display.gfx_Line(82,5,92,5,GREEN) ;
  }
  if(battery >= 750 && battery <= 799 )
  {
  Display.gfx_Line(82,3,82,6,BLACK) ;
  Display.gfx_Line(83,3,83,6,BLACK) ;
  Display.gfx_Line(85,3,92,3,GREEN) ;
  Display.gfx_Line(85,4,92,4,GREEN) ;
  Display.gfx_Line(85,4,92,4,GREEN) ;
  Display.gfx_Line(85,5,92,5,GREEN) ;
  }
  if(battery >= 700 && battery <= 749 )
  {
  Display.gfx_Line(82,3,82,6,BLACK) ;
  Display.gfx_Line(83,3,83,6,BLACK) ;
  Display.gfx_Line(84,3,84,6,BLACK) ;
  Display.gfx_Line(85,3,85,6,BLACK) ;
  Display.gfx_Line(86,3,92,3,GREEN) ;
  Display.gfx_Line(86,4,92,4,GREEN) ;
  Display.gfx_Line(86,4,92,4,GREEN) ;
  Display.gfx_Line(86,5,92,5,GREEN) ;
  }
  if(battery >= 625 && battery <= 699 )
  {
  Display.gfx_Line(82,3,82,6,BLACK) ;
  Display.gfx_Line(83,3,83,6,BLACK) ;
  Display.gfx_Line(84,3,84,6,BLACK) ;
  Display.gfx_Line(85,3,85,6,BLACK) ;
  Display.gfx_Line(86,3,86,6,BLACK) ;
  Display.gfx_Line(87,3,92,3,YELLOW) ;
  Display.gfx_Line(87,4,92,4,YELLOW) ;
  Display.gfx_Line(87,4,92,4,YELLOW) ;
  Display.gfx_Line(87,5,92,5,YELLOW) ;
  }
  if(battery <= 624 )
  {
  Display.gfx_Line(82,3,82,6,BLACK) ;
  Display.gfx_Line(83,3,83,6,BLACK) ;
  Display.gfx_Line(84,3,84,6,BLACK) ;
  Display.gfx_Line(85,3,85,6,BLACK) ;
  Display.gfx_Line(86,3,86,6,BLACK) ;
  Display.gfx_Line(87,3,87,6,BLACK) ;
  Display.gfx_Line(88,3,92,3,RED) ;
  Display.gfx_Line(88,4,92,4,RED) ;
  Display.gfx_Line(88,4,92,4,RED) ;
  Display.gfx_Line(88,5,92,5,RED) ;
  }
  //Probe slope status
  Display.txt_FGcolour(BLUE) ;
  Display.txt_MoveCursor(2, 0) ;
  //set up a temp buffer to convert steps to string
  char mVperpHBuff[6];
  Display.putstr(dtostrf(params.pHStep,5,2,mVperpHBuff)) ;
  Display.putstr("mV/pH");

  //Position and size for large clear pH number
  Display.txt_MoveCursor(4, 2) ;
  Display.txt_Height(3) ;
  Display.txt_Width(2) ;
  //Set color based on pH Red out of preferred range, green in range, yellow above range
  Display.txt_FGcolour(LIME) ;
  //set out ph putstr expects a 'string' lets use a std function to convert on the fly with temp buffer
  char pHscreenBuff[6];
  Display.putstr(dtostrf(pH,5,2,pHscreenBuff)) ;
  //Switch back to default size, example of 'superscript'
  //Display.txt_Height(1) ;
  //Display.txt_Width(1) ;
  //Display.putCH('z') ;

  HWLOGGING.print(F("Char height=")) ;
  HWLOGGING.print(Display.charheight('w') ) ;
  HWLOGGING.print(F(" Width= ")) ;
  HWLOGGING.print(Display.charwidth('w') ) ;
  HWLOGGING.println(F("")) ;
  //reset to defaults
  Display.txt_BGcolour(BLACK) ;
  Display.txt_FGcolour(LIME) ;
  Display.txt_FontID(SYSTEM) ;
  Display.txt_Height(1) ;
  Display.txt_Width(1) ;
  Display.txt_MoveCursor(0,0) ;
}

void Callback(int ErrCode, unsigned char ErrByte)
{
#ifdef LOG_MESSAGES
  const char *Error4DText[] = {"OK\0", "Timeout\0", "NAK\0", "Length\0", "Invalid\0"} ;
  HWLOGGING.print(F("Serial 4D Library reports error ")) ;
  HWLOGGING.print(Error4DText[ErrCode]) ;

  if (ErrCode == Err4D_NAK)
  {
    HWLOGGING.print(F(" returned data= ")) ;
    HWLOGGING.println(ErrByte) ;
  }
  else
    HWLOGGING.println(F("")) ;
  while(1) ; // you can return here, or you can loop
#else
// Pin 13 has an LED connected on most Arduino boards. Just give it a name
#define led 13
  while(1)
  {
    digitalWrite(led, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(200);               // wait for a second
    digitalWrite(led, LOW);    // turn the LED off by making the voltage LOW
    delay(200);               // wait for a second
  }
#endif
}