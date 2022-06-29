# airquality

Yet Another Air Quality Meter. Shows CO2, TVOC, temperature, relative humidity, PM and particles by weight in volume readings

Hardware
* NodeMCUv3 ESP8266 (ESP-12E)
* CCS811/BME280 Environmental Combo Breakout (SparkFun)
* PMSA003I PM2.5 Air Quality Sensor (Adafruit)
* 20x04 character I2C LCD
* Three LEDs to display CO2 levels

Generally based on https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library/blob/master/examples/Example2_BME280Compensation/Example2_BME280Compensation.ino by Marshall Taylor @ SparkFun Electronics, April 4, 2017

## TODO
* Check gas sensor burn-in times, it's acting up a bit showing 400ppm readings
* Figure out why DHT22/DHT11 does not work
* Re-attach traffic lights

<img src="https://i.imgur.com/dj4vFpZ.jpg" height="400">
<img src="https://i.imgur.com/Dr5kTJo.jpg" height="400">
<img src="https://i.imgur.com/omIsKV0.jpg" height="400">
<img src="https://i.imgur.com/ZP2DmmP.jpg" height="400">
