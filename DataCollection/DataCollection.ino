/*
   Source code for data collection device

   Last Updated: 2/28/2018
   By: James Deromedi
*/
/*POSSIBLE IDEAS/TO DO
   Have switch that sets 'driving' or 'outside' driving: Some enviromental data, since inside car walking: Enviromental data since outside
   Have LED when GPS has a fix
   Have LED when device takes data - DONE
   Have timer set on a turn knob, so you can change it on the fly - DONE
   Setup SD card to create two different files (GPS/Enviro) - DONE
   Create a calibration method for the potentiometer (min and max values change sometimes) for map() to be correct
   Have small display to show what the timer is at in minutes
   Implement reset button in case of breakage; Have LED that says so
   Cut +5V trace on back of board to prevent power lose from unused USB port
   Add two 1N5817 Diodes, this will prevent drain on batteries when plugged in from USB
   Power Stuff - DONE
      Low Power Timer -DONE
   Watchdog Timer, incase of system failure
   Clean up code
      -Capital letters
      -Check comments
      -Clear out unused items
*/

#include <SD.h>     //Needed to write to a SD card
#include <Wire.h>   //Needed to communicate between devices with I2C
#include <SPI.h>    //Needed to communicate between device with SPI
#include <Adafruit_BME680.h>
#include <Snooze.h>

const int BME680_SCK = 19;    //Pin for enviro sensor clock
const int BME680_MOSI = 18;   //Pin for enviro sensor data transfer
const int CHIPSELECT = 10;    //Pin for slave's chip
const int SEALEVELPRESSURE_HPA = 1028.1;    //Sea level pressure at DCA on 2/27/17 at 4pm
const int POTENTIOMETERINPUT = A9;   //Pin for timer changer

int pMin = 5000;    //Setting original Min high so it can be brought down
int pMax = 0;     //Settng original Max low so it can be brought up

char *eFileName = "eFile.txt";
char *gpsFileName = "GPSFile.txt";

int dataTimer = 0;
int ledTimer = 0;

bool isWalking = true;    //TODO: connect to a switch
bool chipInserted;
bool calibrateFlag = false;

/* Pins for the LEDS
   Bound to change
*/
int dataCollectLed = 2;
int chipInput = 4;
int calibrateLED = 6;
int noChipLed = 6;

File gpsFile;
File eFile;

SnoozeDigital digital;
SnoozeTimer timer;
SnoozeBlock config(timer, digital);

Adafruit_BME680 bme;

//------------------------------------------------------------------------------------------------
void setup() {
    //Comment out when done debugging
    Serial.begin (9600);
    while (!Serial) {
    delay(1);
    }
    
  /*Pin setup for LEDS*/
  pinMode(dataCollectLed, OUTPUT);
  pinMode(calibrateLED, OUTPUT);
  pinMode(chipInput, INPUT_PULLUP); //Used to know when a SD in plugged into the device
  pinMode(noChipLed, OUTPUT);

  /*Check if the envriomental card is plugged in*/
  if (!bme.begin()) {
    //Serial.println("Initialization of Enviromental card failed");
    //TODO: HAVE LED LIGHT UP
    //Reset the program??
    digitalWrite(noChipLed, HIGH);
    delay(500);
    digitalWrite(noChipLed, LOW);
    return;
  }

  /*Check if the SD card is plugged in, if not set flag to false, if it is set initalize and set flag*/
    if(!SD.begin(CHIPSELECT)){
      //Didn't intialize
      //Should restart??
      digitalWrite(noChipLed, HIGH);
      Serial.println("NO SD");
      chipInserted = false;
    } else {
      chipInserted = true;
      digitalWrite(noChipLed, LOW);
    }
    
  /*Setup oversampling and filter initialization*/
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  
}//END setup()

//------------------------------------------------------------------------------------------------
void loop() {
  /*Calibrates the potentiometer to always get accurate mappings and minutes between wake ups */
  if (calibrateFlag) {
    calibration();    
  }
  
  /* Checks if the SD card is removed or newly added*/
  sdChipInit();

  /* Gets time between readings/wake ups and puts the device asleep for that time 
   * uses Low Power Timer to keep track of milliseconds that have passed */
  //timer.setTimer(potentReading(POTENTIOMETERINPUT));  
  //int who = Snooze.deepSleep(config);

  /* Start of data collection */
  digitalWrite(dataCollectLed, HIGH); 

  /* TODO: Implement Hardware Switch and read in its value */
  if (isWalking) {
    openSDCard(eFile, eFileName, 0);
    eFile.println(getReadings());
    //TAKE GPS DATA
    closeSDCard(eFile);
  }
  if (!isWalking) {
    openSDCard(gpsFile, gpsFileName, 1);
    //TAKE GPS DATA
    closeSDCard(gpsFile);
  }

  /* End of data collection */
  digitalWrite(dataCollectLed, LOW); //Turns off when done taking data
  
}//END loop()

//------------------------------------------------------------------------------------------------
/**
 * Chip Initalization
 * This will the chip to be taken out and plugged back in without stopping the program or having errors
 * with multiple intializations
 * Currently if the chip is not plugged in, the program will run as normal but not be able to write anything
 * @PARAM: NONE
 * @RETURN: NONE
 */
 void sdChipInit(){
  if(digitalRead(chipInput) == HIGH && chipInserted == false){ //If chip is plugged in and it not initialized
    SD.begin(CHIPSELECT);   //Initalize the 'new' chip
    chipInserted = true;    //Set chipInserted to now true
    digitalWrite(noChipLed, LOW);
  } 
  else if (digitalRead(chipInput) == HIGH) { //If the chip is not plugged in
    chipInserted = false;     
    digitalWrite(noChipLed, HIGH);
    Serial.println("NO SD");
  } 
  else {    //Chip is plugged in and is initialized
    digitalWrite(noChipLed, LOW);
    
  }
  
 }//END sdChipInit()
 
//------------------------------------------------------------------------------------------------
/**
   Calibration of the potentiometer
   The method holds up the program for the calibration time, this way nothing interfers with the it
   After testing, each time the device is turned on the potentiometer will have different min and max readings
   I do not know why
   @Param
   @Return: None
*/
void calibration() {
  int calibrationTime = 30000;   //Have 4 seconds to calibrate once the light appears
  int temp = 0;
  unsigned long currentMillis = millis();
  while (millis() < currentMillis + calibrationTime) {
    digitalWrite(calibrateLED, HIGH);     //Turn on LED at start of calibration
    temp = analogRead(POTENTIOMETERINPUT);
    if (temp > pMax) {
      pMax = temp;
    }
    if (temp < pMin) {
      pMin = temp;
    }
  }//END while
  digitalWrite(calibrateLED, LOW);
  calibrateFlag = true;   //Set flag so it can only happen once
  
}//END calibration()

//------------------------------------------------------------------------------------------------
/**
   Reads the input of the potentiometer than maps those values from 6 - 180 milliseconds.
   The returned number is then multipled by 10000 to give a number between 60000 - 1800000 milliseconds or 1 - 30 minutes.
   @Param: inputPin - int; Pin attached the potentiometer
   @Return: value - int; Mapped value of the potentiometer in milliseconds
*/
int potentReading(int inputPin) {
  int value = 0;
  value = analogRead(inputPin);  //Low 2 miliseconds - High 1023 miliseconds
  //Serial.println(value);//Debugging
  value = (map(value, pMin, pMax, 6, 180));
  Serial.print((value * 1000)); Serial.println(" milliseconds");//Debugging
  value = value * 1000;
  return value;
  
}//END potentReading()

//------------------------------------------------------------------------------------------------
/**
   Opens or creates a file with the given name for writing.
   Assigns that file to the directed File variable (eFile or gpsFile), depending on the parameter.
   @Param: file - File; Which file is going to be opened
   @Param: fileName - char*; The name of the file to be opened
   @Param: fileChoice - int; Since open() returns a File variable which will be lost at the end of the method
   @Return: None
*/
void openSDCard(File file, char *fileName, int fileChoice) {
  file = SD.open(fileName, FILE_WRITE);     //Opens the file at the end for writing

  if (fileChoice == 0) {  //enviromental data file
    eFile = file;
  } else if (fileChoice == 1) { //GPS data file
    gpsFile = file;
  }
  
}//END openSDCard()

//------------------------------------------------------------------------------------------------
/**
   Closes the the File that is currently open
   @Param: file - File; The file that is wanted to be opened
   @Return: None
*/
void closeSDCard(File file) {
  file.close();
  
}//END closeSDCard

//------------------------------------------------------------------------------------------------
/**
   TODO: COUNTER TO KNOW WHICH READING WE ARE AT
       : Steal GPS' time stamp
       : Have driving data be only pressure and alititude
   Takes the environmental data and places it in a string with commas seperating the data
   Tempature(*C), Barometric Pressure (in/Mg), Humidity(%), Gas/VOC(KOhms), Altitude(m)
   @Param: None
   @Return concatString - String; Contains all the data in the above order
*/
String getReadings() {
  String concatString = "";
  if (! bme.performReading()) {
    //  Serial.println("Failed to perform reading :(");
    //HAVE LED TURN-ON
    return "Error Getting Environmental Data";
  }
  concatString.concat(bme.temperature);
  concatString.concat(", ");
  concatString.concat(bme.pressure / 3386.39);
  concatString.concat(", ");
  concatString.concat(bme.humidity);
  concatString.concat(", ");
  concatString.concat(bme.gas_resistance / 1000.0);
  concatString.concat(", ");
  concatString.concat(bme.readAltitude(SEALEVELPRESSURE_HPA));
  return concatString;
  
}//END getReadings()

//------------------------------------------------------------------------------------------------
/**
   Testing and debugging for the BME680 breakout.
   Prints out the specific data in a nice little format
   @Param: None
   @Return: None
*/
void testBME() {
  if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }
  /*Prints out current tempature in celcius*/
  Serial.print("Temperature = ");
  Serial.print(bme.temperature);
  Serial.println(" *C");
  /*Prints out the current barometric tempature in in/Mg*/
  Serial.print("Pressure = ");
  Serial.print(bme.pressure / 3386.39);
  Serial.println(" hPa");
  /*Prints out the current humidity in percent*/
  Serial.print("Humidity = ");
  Serial.print(bme.humidity);
  Serial.println(" %");
  /*Prints out the Volatile Organic Compounds in Ohms; High VOC means lower resistance*/
  Serial.print("Gas = ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" KOhms");
  /*Prints out the altitudein meters, without current sea level pressure, can be off by 10m*/
  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");
  Serial.println();
  delay(2000);
  
}//END testBME()
