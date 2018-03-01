/*
   Source code for data collection device

   Last Updated: 2/28/2018
   By: James Deromedi
*/
/*POSSIBLE IDEAS/TO DO
   Have switch that sets 'driving' or 'outside' driving: Some enviromental data, since inside car walking: Enviromental data since outside
   Have LED when GPS has a fix
   Have LED when device takes data
   Have timer set on a turn knob, so you can change it on the fly - DONE
   Setup SD card to create two different files (GPS/Enviro) - DONE
   Create a calibration method for the potentiometer (min and max values change sometimes) for map() to be correct
   Have small display to show what the timer is at in minutes
   Implement reset button in case of breakage; Have LED that says so
   Cut +5V trace on back of board to prevent power lose from unused USB port
   Add two 1N5817 Diodes, this will prevent drain on batteries when plugged in from USB
   Power Stuff
      Real Time Clock for wake-up
      Low Power Timer
   Watchdog Timer - incase of system failure
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
const int pMin = 35;     //Min value of potentiometer (Might change with calibration method)
const int pMax = 983;    //Max value of potentiometer(Might change with calibration method)

char *eFileName = "eFile.txt";
char *gpsFileName = "GPSFile.txt";
int dataTimer = 0;
int ledTimer = 0;
bool isWalking = true;    //Will be connected to a switch
bool chipInserted;

int led = 2;//Bound to change
int chipInput = 4;
int count = 0;//Debugging

File gpsFile;
File eFile;

SnoozeDigital digital;
SnoozeTimer timer;
SnoozeBlock config(timer, digital);

Adafruit_BME680 bme;

//------------------------------------------------------------------------------------------------
void setup() {
  /*Setup Serial to print out debug data
    Comment out when done debugging
    Serial.begin (9600);
    while (!Serial) {
    delay(1);
    }*/

  /*Check if the envriomental card is plugged in*/
  if (!bme.begin()) {
    //Serial.println("Initialization of Enviromental card failed");
    //TODO: HAVE LED LIGHT UP
    return;
  }

  /*Check if the SD card is plugged in*/
  if (digitalRead(chipInput)) {
    SD.begin(CHIPSELECT);
    chipInserted = true;
  } else {
    chipInserted = false;
  }

  /*if (!SD.begin(CHIPSELECT)) {
    // Serial.println("Initialization of SD card failed");
    chipInserted = false;
    } else {
    chipInserted = true;
    }*/

  /*Setup oversampling and filter initialization*/
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  /*Pin setup for LEDS*/
  pinMode(led, OUTPUT);
  pinMode(chipInput, INPUT_PULLUP);
}//END setup()

//------------------------------------------------------------------------------------------------
void loop() {

  timer.setTimer(potentReading(POTENTIOMETERINPUT));  //Sets the LPTMR to value of potentiometer
  int who = Snooze.deepSleep(config);   //Sets the sleep conditions and puts the chip into deepSleep

  if (!chipInserted) {
    if (digitalRead(chipInput)) {
      SD.begin(CHIPSELECT);
      chipInserted = true;
    }
  }


  digitalWrite(led, HIGH);//Debugging, see when the chip turn on and off
  delay(500);
  digitalWrite(led, LOW);
  /*Read switch to check if driving or walking */
  /* Uncomment after timer works properly */
  if (isWalking) {
    openSDCard(eFile, eFileName, 0);
    eFile.println(getReadings());
    closeSDCard(eFile);
  }
  if (!isWalking) {
    openSDCard(gpsFile, gpsFileName, 1);
    //Take/write gps data
    closeSDCard(gpsFile);
  }

  //}//END timer loop
}//END loop()

//------------------------------------------------------------------------------------------------
/**
   A quick method that reads the input of the potentiometer than maps those values from 6 - 180 milliseconds.
   The returned number is then multipled by 10000 to give a number between 60000 - 1800000 milliseconds or 1 - 30 minutes.
   @Param: inputPin - int; Pin attached the potentiometer
   @Return: value - int; Mapped value of the potentiometer in milliseconds
*/
int potentReading(int inputPin) {
  int value = 0;
  value = analogRead(inputPin);  //Low 2 miliseconds - High 1023 miliseconds
  Serial.println(value);//Debugging
  value = (map(value, pMin, pMax, 6, 180));
  Serial.print((value * 10000)); Serial.println(" milliseconds");//Debugging
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
  } else {
    // Serial.println("NOOOOO PLEASE");
  }
  //Debugging
  //if (file) {
  //Serial.println("No error in opening ");
  //} else {
  //Serial.println("Error opening ");
  //}
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
    return "Error taking data";
  }
  concatString.concat(bme.temperature);
  concatString.concat(",");
  concatString.concat(bme.pressure / 3386.39);
  concatString.concat(",");
  concatString.concat(bme.humidity);
  concatString.concat(",");
  concatString.concat(bme.gas_resistance / 1000.0);
  concatString.concat(",");
  concatString.concat(bme.readAltitude(SEALEVELPRESSURE_HPA));
  // Serial.println(concatString);//Debugging
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
