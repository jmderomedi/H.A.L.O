### TechSafari2018 ###
For AU Tech Safari 2018, we are tasked to create a data visualization website/presentation. The first task at hand is to create
the hardware that will collect the data. After some discussions we deciced upon GPS data and environmental data. The device is able
to take data at set increments that is choosen by potentiometer. The data will than be saved to an SD card and parsed. The parsing will clean the data for the map API that we used. The map API will display an interactive map of every data point and locations of interest that we visited.

This repo only contains the hardware/firmware of the project. See oneyanshi for the front end and complete website.

### Hardware Side ###
The data collection device G-PATH (Gps, Pressure, Altitude, Tempature, Humidity).\n
The device should be Set-and-Forget.\n
Make sure the battery is charged, once plugged in the device will run until the battery dies.

### Built With ###
	-Teensyduino, A plug in for Arduino
	-Adafruit Ultimite GPS Breakout
	-Adafruit SD Breakout
	-Adafruit BME680 (Environmental Sensor)
### Authors ###
	James Deromedi
	
### License ###
This is a free-ware at the moment. Please just reconize the author(s) if you use anything posted here

### Acknowledgements ###
Kristof for being our advisor
John for being the amazing sponsor and setting up this whole trip and event
