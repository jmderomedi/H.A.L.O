/*
   Source code for data collection device

   Last Updated: 3/7/2018
   By: James Deromedi
*/
/*POSSIBLE IDEAS/TO DO
   Have switch that sets 'driving' or 'outside' driving: Some enviromental data, since inside car walking: Enviromental data since outside
   Have LED when GPS has a fix -DONE
   Have LED when device takes data - DONE
   Have timer set on a turn knob, so you can change it on the fly - DONE
   Setup SD card to create two different files (GPS/Enviro) - DONE
   Create a calibration method for the potentiometer (min and max values change sometimes) for map() to be correct -DONE
   Have small display to show what the timer is at in minutes
   Cut +5V trace on back of board to prevent power lose from unused USB port
   Add two 1N5817 Diodes, this will prevent drain on batteries when plugged in from USB
   Power Stuff - DONE
      Low Power Timer -DONE
   Watchdog Timer, incase of system failure
   Clean up code
      -Capital letters
      -Check comments-DONE
      -Clear out unused items-DONE
   Have SD be able to be pulled out and put back in without power cycling or error writing - DONE
*/

#include <Wire.h>                         //Library for teensy to communicate to a device with I2C
#include <SPI.h>                          //Library for teensy to communicate to a device with SPI
#include <Snooze.h>                       //Library to put teensy chip asleep
#include <Adafruit_GPS.h>                 //Library for the environmental sensor 
#include <Adafruit_BME680.h>              //Library for GPS breakout
#include <SD.h>                           //Library for SD card

#define GPSSerial Serial3                 //Pins needed to communicate with GPS; RX1 and TX1; Pin 0 and Pin 1
#define wait4Me "$GPRMC"                  //What is compared against to tell if a NMEA sentence has come in

const int DATACOLLECTEDLED = 4;
const int CALIBRATELED = 5;
const int NOCHIPLED = 6;
const int CHIPINPUT = 9;
const int CHIPSELECT = 10;                //Pin for  SD slave's chip
const int BME680_MOSI = 18;               //Pin for enviro sensor data transfer
const int BME680_SCK = 19;                //Pin for enviro sensor clock
const int POTENTIOMETERINPUT = A9;        //Input Analog Pin for potentiometer
const int SEALEVELPRESSURE_HPA = 1028.1;  //Sea level pressure at DCA on 2/27/17 at 4pm

int pMin = 5000;                          //Setting original Min high so it can be brought down
int pMax = 0;                             //Settng original Max low so it can be brought up

char *eFileName = "eFile.txt";            //List of chars for environmental file name
char *gpsFileName = "GPSFile.txt";        //List of chars for gps file name

bool isWalking = true;                    //TODO: connect to a switch;
bool chipInitialized;                     //Flag for if the SD card is initialized or not
bool previousChipState = false;           //Flag for last initialization state the SD card was in
bool calibrateFlag = false;               //Flag for calibration of potentiometer
bool gpsInitialized;                      //Flag for resetting GPSSerial

File gpsFile;                             //Where all gps data will be written to
File eFile;                               //Where all environmental data will be written to

int fixled = 22;

SnoozeDigital digital;                    //Driver for the teensy chip to be put asleep
SnoozeTimer timer;                        //Wakeup driver method for the teensy chip
SnoozeBlock config(timer, digital);       //Thingy

Adafruit_GPS GPS(&GPSSerial);             //Instantiation of GPS device through its pins
Adafruit_BME680 bme;                      //Instantiation of the environmental data

//------------------------------------------------------------------------------------------------
void setup() {
  //Comment out when done debugging
  //  while (!Serial) {
  //    delay(1);
  //  }
  //  Serial.begin (115200);

  /*Pin setup for LEDS*/
  pinMode(fixled, OUTPUT);
  pinMode(DATACOLLECTEDLED, OUTPUT);
  pinMode(CALIBRATELED, OUTPUT);
  pinMode(CHIPINPUT, INPUT_PULLUP);   //Used to know when a SD in plugged into the device
  pinMode(NOCHIPLED, OUTPUT);
  pinMode(CHIPSELECT, OUTPUT);

  /*Starts GPS serial and set GPS boundries*/
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);    //Setting gps to give GMRMC NMEA lines
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);    //Set how quickly the GPS takes data
  delay(1000);    //Needed for GPS module to set itself up
  GPSSerial.println(PMTK_Q_RELEASE);    //NEEDED?; Ask for firmware version

  /*Check if the envriomental card is plugged in*/
  if (!bme.begin()) {
    //Nothing needed here
    return;
  }

  /*Check if the SD card is plugged in, if not set flag to false, if it is set initalize and set flag*/
  if (!SD.begin(CHIPSELECT)) {
    digitalWrite(NOCHIPLED, HIGH);
    chipInitialized = false;
  } else {
    chipInitialized = true;
    digitalWrite(NOCHIPLED, LOW);
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
  if (!calibrateFlag) {
    calibration();
  }

  /* Checks if the SD card is removed or newly added*/
  sdChipInit();

  openSDCard(eFile, eFileName, 0);    //Open enviromental file
  openSDCard(gpsFile, gpsFileName, 1);    //Open gps file

  /* Start of data collection */
  digitalWrite(DATACOLLECTEDLED, HIGH);

  if (chipInitialized) {
    /*GPS Data
      FUCK, this next little bit, these three lines gave me nightmares!
    */
    uint8_t maxWait = 2;
    if (GPS.waitForSentence(wait4Me, maxWait)) {    //Return true when sentence with wait4Me in it, checks twice
      gpsFile.println(GPS.lastNMEA());
    }
    /*Environmental Data*/
    eFile.println(getReadings());   //Write data taken

  }//END if(chipInitialized)

  closeSDCard(eFile);   //Close enviromental file
  closeSDCard(gpsFile);   //close gps file

  digitalWrite(DATACOLLECTEDLED, LOW);
  /* End of data collection */

  /* Gets time between readings/wake ups and puts the device asleep for that time
       uses Low Power Timer to keep track of milliseconds that have passed */
  timer.setTimer(potentReading(POTENTIOMETERINPUT));
  int who = Snooze.deepSleep(config);

}//END loop()

//------------------------------------------------------------------------------------------------
/**
   SD card Initalization
   This will allow the SD card to be taken out and plugged back in without stopping the program or having errors
   with multiple intializations
   Currently if the card is not plugged in, the program will run as normal but not be able to write anything
   Once the card is replugged in, the card is seen as a 'new' card that needs to be reintialized to be written to.
   @Param: NONE
   @Return: NONE
*/
void sdChipInit() {
  if (digitalRead(CHIPINPUT) == HIGH && chipInitialized == false) {   //If chip is plugged in and it not initialized
    if (!SD.begin(CHIPSELECT)) { //Initalize the 'new' chip
      Serial.println("Failed SD.begin, inside sdChipInit()");
    }
    chipInitialized = true;   //Set chipInitialized to now true
    digitalWrite(NOCHIPLED, LOW);   //Turns off LED
  }
  else if (digitalRead(CHIPINPUT) == LOW) {   //If the card is not plugged in, doesn't matter if initalized
    chipInitialized = false;    //Sets chipInitialized to false;
    digitalWrite(NOCHIPLED, HIGH);    //Turns on LED
  }
  else {    //Chip is plugged in and is initialized
    digitalWrite(NOCHIPLED, LOW);   //Turns off LED
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
  int calibrationTime = 5000;    //Have 5 seconds to calibrate once the light appears
  int temp = 0;
  unsigned long currentMillis = millis();
  while (millis() < currentMillis + calibrationTime) {    //While millis() is below timer
    digitalWrite(CALIBRATELED, HIGH);     //Turn on LED for user to know to calibrate
    temp = analogRead(POTENTIOMETERINPUT);    //Reads in potentiometer reading

    /*Compares the input to current max and min, reassigns as directed*/
    if (temp > pMax) {
      pMax = temp;
    }
    if (temp < pMin) {
      pMin = temp;
    }

  }//END while

  digitalWrite(CALIBRATELED, LOW);    //Turns off led; Signaling calibration is complete
  calibrateFlag = true;   //Set flag so calibration() can only happen once

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
  value = analogRead(inputPin);  //Raw value of potentiometer reading
  value = (map(value, pMin, pMax, 6, 180));   //Remaps the raw data to min and max of calibration
  value = value * 100;    //TODO: Change back to '10000' when done;
  return value;

}//END potentReading()

//------------------------------------------------------------------------------------------------
/**
   Opens or creates a file with the given name for writing.
   Assigns that file to the directed File variable (eFile or gpsFile), depending on the parameter.
   Since SD.open returns pointer of the file opened, have to assign it to a temp File than assign it to the appropriate File
   @Param: file - File; Which file is going to be opened
   @Param: fileName - char*; The name of the file to be opened
   @Param: fileChoice - int; Since open() returns a File variable which will be lost at the end of the method
   @Return: None
*/
void openSDCard(File file, char *fileName, int fileChoice) {
  file = SD.open(fileName, FILE_WRITE);   //Save to temp File first

  /*Assign temp pointer to appropriate File*/
  if (fileChoice == 0) {    //Enviromental data file
    eFile = file;
  } else if (fileChoice == 1) {   //GPS data file
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
    //TODO: HAVE LED TURN-ON
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

/**
   Test Method for SD breakout, Potentiometer min value, Potentiometer max value, potentiometer raw data
   @Param: NONE
   @Return: NONE
*/
void test() {
  delay(500);
  Serial.print("Sd card plugged in: "); Serial.println(digitalRead(CHIPINPUT));
  Serial.print("Sd card initalized: "); Serial.println(chipInitialized);
  Serial.print("Pmax of potentiometer: "); Serial.println(pMax);
  Serial.print("Pmin of potentiometer: "); Serial.println(pMin);
  Serial.print("Value of potentiometer: "); Serial.println(potentReading(POTENTIOMETERINPUT));
  Serial.println("");
}

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
