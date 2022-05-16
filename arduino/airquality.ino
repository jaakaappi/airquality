#include <SPI.h>
#include <Wire.h>
#include <SparkFunBME280.h> // Click here to get the library: http://librarymanager/All#SparkFun_BME280
#include <SparkFunCCS811.h> // Click here to get the library: http://librarymanager/All#SparkFun_CCS811
#include <hd44780.h> // main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c expander i/o class header
#include <ESP8266WiFi.h>

#define CCS811_ADDR 0x5B // Default I2C Address

#define RED_PIN 14
#define YELLOW_PIN 12
#define GREEN_PIN 13

#define MEASUREMENT_INTERVAL 60*1000

CCS811 myCCS811(CCS811_ADDR);
BME280 myBME280;

hd44780_I2Cexp lcd(0x20);

boolean sensorsReady = false; // CCS811 and BME280 require 20 minutes to start showing accurate results.
int previousMeasurementMillis = 0;
int co2 = 0;
int tvoc = 0;
float temperatureC = 0.0;
float relativeHumidity = 0.0;

void setup()
{
  WiFi.mode(WIFI_OFF);

  lcd.begin(16, 2);

  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Apply BME280 data to CCS811 for compensation.");

  CCS811Core::CCS811_Status_e returnCode = myCCS811.beginWithStatus();
  Serial.print("CCS811 begin exited with: ");
  Serial.println(myCCS811.statusString(returnCode));

  myBME280.settings.commInterface = I2C_MODE;
  myBME280.settings.I2CAddress = 0x77;

  myBME280.settings.commInterface = I2C_MODE;
  myBME280.settings.I2CAddress = 0x77;
  myBME280.settings.runMode = MODE_FORCED;
  myBME280.settings.tStandby = 60000;
  myBME280.settings.filter = 0;
  myBME280.settings.tempOverSample = 1;
  myBME280.settings.pressOverSample = 1;
  myBME280.settings.humidOverSample = 1;

  delay(10);
  myBME280.begin();
  delay(10);
  readMeasurements();
}

void loop()
{
  if (sensorsReady) {
    if (sensorsReady && myCCS811.dataAvailable())
    {
      printInfoSerial();
      printInfoLcd();

      if (millis() - previousMeasurementMillis >= MEASUREMENT_INTERVAL) {
        readMeasurements();
        if (co2 < 500) {
          digitalWrite(GREEN_PIN, HIGH);
          digitalWrite(YELLOW_PIN, LOW);
          digitalWrite(RED_PIN, LOW);
        } else if (co2 > 500 && co2 < 1000) {
          digitalWrite(GREEN_PIN, LOW);
          digitalWrite(YELLOW_PIN, HIGH);
          digitalWrite(RED_PIN, LOW);
        } else if (co2 > 1000) {
          digitalWrite(GREEN_PIN, LOW);
          digitalWrite(YELLOW_PIN, LOW);
          digitalWrite(RED_PIN, HIGH);
        }
        previousMeasurementMillis = millis();
      }
    }
    else if (myCCS811.checkForStatusError())
    {
      printSensorError();
    }

    delay(3000);
  } else if (millis() >= 20 * 60 * 1000) {
    sensorsReady = true;
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Warming up");
    lcd.setCursor(0, 1);
    lcd.print(String(millis() / (1000 * 60)) + "/20 minutes");

    Serial.print("Waiting for sensors to stabilize, currently elapsed: ");
    Serial.print(millis() / (1000 * 60));
    Serial.println("/20 minutes");

    digitalWrite(GREEN_PIN, HIGH);
    delay(200);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(YELLOW_PIN, HIGH);
    delay(200);
    digitalWrite(YELLOW_PIN, LOW);
    digitalWrite(RED_PIN, HIGH);
    delay(200);
    digitalWrite(RED_PIN, LOW);

    delay(1000);
  }

}

void readMeasurements() {
  myCCS811.readAlgorithmResults();
  co2 = myCCS811.getCO2();
  tvoc = myCCS811.getTVOC();
  temperatureC = myBME280.readTempC();
  relativeHumidity = myBME280.readFloatHumidity();
  myCCS811.setEnvironmentalData(myBME280.readFloatHumidity(), myBME280.readTempC());
}

void printInfoLcd()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CO2 " + String(co2) + " ppm");
  lcd.setCursor(0, 1);
  lcd.print("TVOC " + String(tvoc) + " ppb");

  delay(3000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp " + String(temperatureC) + " \xDF""C"); // https://forum.arduino.cc/t/solved-how-to-print-the-degree-symbol-extended-ascii/438685/5
  lcd.setCursor(0, 1);
  lcd.print("RelHum " + String(relativeHumidity) + " %");
}

void printInfoSerial()
{
  Serial.println("CCS811 data:");
  Serial.print(" CO2 concentration : ");
  Serial.print(myCCS811.getCO2());
  Serial.println(" ppm");

  Serial.print(" TVOC concentration : ");
  Serial.print(myCCS811.getTVOC());
  Serial.println(" ppb");

  Serial.println("BME280 data:");
  Serial.print(" Temperature: ");
  Serial.print(myBME280.readTempC(), 2);
  Serial.println(" degrees C");

  Serial.print(" Temperature: ");
  Serial.print(myBME280.readTempF(), 2);
  Serial.println(" degrees F");

  Serial.print(" Pressure: ");
  Serial.print(myBME280.readFloatPressure(), 2);
  Serial.println(" Pa");

  Serial.print(" Pressure: ");
  Serial.print((myBME280.readFloatPressure() * 0.0002953), 2);
  Serial.println(" InHg");

  Serial.print(" Altitude: ");
  Serial.print(myBME280.readFloatAltitudeMeters(), 2);
  Serial.println("m");

  Serial.print(" Altitude: ");
  Serial.print(myBME280.readFloatAltitudeFeet(), 2);
  Serial.println("ft");

  Serial.print(" %RH: ");
  Serial.print(myBME280.readFloatHumidity(), 2);
  Serial.println(" %");

  Serial.println();
}

void printSensorError()
{
  uint8_t error = myCCS811.getErrorRegister();

  if (error == 0xFF) //comm error
  {
    Serial.println("Failed to get ERROR_ID register.");
    //lcd.setCursor(0, 0);
    //lcd.print("Failed to get");
    //lcd.setCursor(0, 1);
    //lcd.print("ERROR_ID reg.");
  }
  else
  {
    Serial.print("Error: ");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error:");
    if (error & 1 << 5) {
      Serial.print("HeaterSupply");
      lcd.setCursor(0, 1);
      lcd.print("HeaterSupply");
    }
    if (error & 1 << 4) {
      Serial.print("HeaterFault");
      lcd.setCursor(0, 1);
      lcd.print("HeaterFault");
    }
    if (error & 1 << 3) {
      Serial.print("MaxResistance");
      lcd.setCursor(0, 1);
      lcd.print("MaxResistance");
    }
    if (error & 1 << 2) {
      Serial.print("MeasModeInvalid");
      lcd.setCursor(0, 1);
      lcd.print("MeasModeInvalid");
    }
    if (error & 1 << 1) {
      Serial.print("ReadRegInvalid");
      lcd.setCursor(0, 1);
      lcd.print("ReadRegInvalid");
    }
    if (error & 1 << 0) {
      Serial.print("MsgInvalid");
      lcd.setCursor(0, 1);
      lcd.print("MsgInvalid");
    }
    Serial.println();
  }
}
