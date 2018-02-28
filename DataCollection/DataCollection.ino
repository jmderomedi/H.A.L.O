/*
   Source code for data collection device

   Last Updated: 2/27/2018
   By: James Deromedi
*/
/*POSSIBLE IDEAS/TO DO
   Have switch that sets 'driving' or 'outside' driving: Some enviromental data, since inside car walking: Enviromental data since outside
   Have LED when GPS has a fix
   Have LED when device takes data
   Have timer set on a turn knob, so you can change it on the fly - DONE
   Setup SD card to create two different files (GPS/Enviro) - DONE
   
*/

#include <SD.h>     //Needed to write to a SD card
#include <Wire.h>   //Needed to communicate between devices with I2C
#include <SPI.h>    //Needed to communicate between device with SPI
#include <Adafruit_BME680.h>
//#include <TimerOne.h>

const int BME680_SCK = 19;    //Pin for enviro sensor clock
const int BME680_MOSI = 18;   //Pin for enviro sensor data transfer
const int CHIPSELECT = 10;    //Pin for slave's chip
const int SEALEVELPRESSURE_HPA = 1028.1;    //Sea level pressure at DCA on 2/27/17 at 4pm
const int POTENTIOMETERINPUT = A9;   //Pin for timer changer
const int pMin = 2;
const int pMax = 1023;

char *eFileName = "eFile.txt";
char *gpsFileName = "GPSFile.txt";
int dataTimer = 0;     //Starting 1 second timer for data taking
int potentiometer = 0;
bool isWalking = true;

int led = 2;
int count = 0;

File gpsFile;
File eFile;

Adafruit_BME680 bme;

void setup() {
  /*Setup Serial to print out debug data
    Comment out when done debugging*/
  Serial.begin (9600);
  while (!Serial) {
    delay(1);
  }
  /*Check if the envriomental card is plugged in*/
  if (!bme.begin()) {
    Serial.println("Initialization of Enviromental card failed");
    //HAVE LED LIGHT UP
    return;
  } else {
    Serial.println("No Error with BME680");
  }
  /*Check if the SD card is plugged in*/
  if (!SD.begin(CHIPSELECT)) {
    Serial.println("Initialization of SD card failed");
    //HAVE LED LIGHT UP
    return;
  } else {
    Serial.println("No Error with SD");
  }

  /*Setup oversampling and filter initialization*/
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  //SETUP PIN MODES
  pinMode(led, OUTPUT);
}//ENDSETUP

//------------------------------------------------------------------------------------------------
void loop() {
  unsigned long currentMillis = millis();

  potentReading();

  if (currentMillis - dataTimer > potentiometer) {
    digitalWrite(led, HIGH);
    delay(1000);
    digitalWrite(led, LOW);
    dataTimer = currentMillis;
    /*Read switch to check if driving or walking */
    if (isWalking) {
      if (count < 3) {
        openSDCard(eFile, eFileName, 0);
        eFile.println(getReadings());
        closeSDCard(eFile);
        count++;
      }
    }
    if (!isWalking) {
      openSDCard(gpsFile, gpsFileName, 1);
      //Take/write gps data
      closeSDCard(gpsFile);
    }
  }
}

/**
   potentiometer; soo tired
*/
void potentReading() {
  potentiometer = analogRead(POTENTIOMETERINPUT);  //Low 2 miliseconds - High 1023 miliseconds
  Serial.println(potentiometer);
  potentiometer = (map(potentiometer, pMin, pMax, 6, 180)) * 10000; //Maps the values from 1 minutes to 30 minutes
  
}

/**
   Opens the SD card so data can be written to it

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
    Serial.println("NOOOOO PLEASE");
  }
  /*Uncomment code when done debugging
    if (file) {
    Serial.println("No error in opening ");
    } else {
    Serial.println("Error opening ");
    }*/
}

/**
   Closes the directed SD card
   @Param: file - File; The file that is wanted to be opened
   @Return: None
*/
void closeSDCard(File file) {
  file.close();
}

/**
   COUNTER TO KNOW WHICH READING WE ARE AT

   Takes the environmental data and places it in a string with commas seperating the data
   Tempature(*C), Barometric Pressure (in/Mg), Humidity(%), Gas/VOC(KOhms), Altitude(m)
   @Param: None
   @Return concatString - String; Contains all the data in the above order
*/
String getReadings() {
  String concatString = "";
  if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
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
  Serial.println(concatString);
  return concatString;
}
/**
   Testing and debugging for the BME680 breakout.
   Prints out the specific data
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
}
