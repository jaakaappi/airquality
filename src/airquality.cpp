#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SoftwareSerial.h>

#include <hd44780.h>                       // main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c expander i/o class header
#include "Adafruit_PM25AQI.h"
// #include "ST25DVSensor.h"
#include "MHZ19.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_BME280.h>

#include <constants.h>

#define RED_PIN 14
#define YELLOW_PIN 12
#define GREEN_PIN 13

#define MEASUREMENT_INTERVAL 60 * 1000 // 1 minute
#define WARMUP_PERIOD 60 * 1000        // 1 minute

#define MH_Z19_RX 18
#define MH_Z19_TX 19

#define GAS_SENSOR_ANALOG_PIN 39

#define DHTPIN 36
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

MHZ19 co2_sensor;
SoftwareSerial co2_serial(MH_Z19_RX, MH_Z19_TX);

Adafruit_BME280 myBME280;

int co2 = 0;
int tvoc = 0;
float temperatureC = 0.0;
float relativeHumidity = 0.0;

hd44780_I2Cexp lcd(0x27);

Adafruit_PM25AQI aqi = Adafruit_PM25AQI();
PM25_AQI_Data pm_data;

boolean sensorsReady = false;
int previousMeasurementMillis = 0;

static int WIFI_RETRY_INTERVAL = 60 * 1000;
int previousWifiRetryMillis = 0;

// #define GPO_PIN 0
// #define LPD_PIN 0
// #define SDA_PIN 22
// #define SCL_PIN 21

// String uri_write_message = NFC_URL;
// String uri_write_protocol = URI_ID_0x03_STRING;

void readMeasurements();
void printInfoSerial();
void printInfoLcd();
void printSensorError();
void sendMeasurements();

void setup()
{
  WiFi.begin(SSID, WIFI_PASSWORD);
  previousWifiRetryMillis = millis();
  delay(100);

  lcd.begin(20, 4);
  delay(100);

  // if (st25dv.begin(GPO_PIN, LPD_PIN) == 0)
  // {
  //   Serial.println("NFC init done!");
  // }
  // else
  // {
  //   Serial.println("NFC init failed!");
  // }

  // if (st25dv.writeURI(uri_write_protocol, uri_write_message, ""))
  // {
  //   Serial.println("NFC write failed!");
  // }

  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  Serial.begin(115200);
  delay(100);

  co2_serial.begin(9600);
  co2_sensor.begin(co2_serial);
  co2_sensor.autoCalibration(false);
  delay(100);

  myBME280.begin();
  myBME280.setSampling(Adafruit_BME280::MODE_FORCED,
                       Adafruit_BME280::SAMPLING_X1, // temperature
                       Adafruit_BME280::SAMPLING_X1, // pressure
                       Adafruit_BME280::SAMPLING_X1, // humidity
                       Adafruit_BME280::FILTER_OFF);
  delay(100);

  if (!aqi.begin_I2C())
  {
    Serial.println("Could not find PM 2.5 sensor!");
  }

  pinMode(GAS_SENSOR_ANALOG_PIN, INPUT);
  delay(100);

  dht.begin();
  delay(100);
}

void loop()
{
  if (sensorsReady)
  {
    printInfoLcd();

    if (millis() - previousMeasurementMillis >= MEASUREMENT_INTERVAL)
    {
      readMeasurements();
      printInfoSerial();
      if (co2 < 500)
      {
        digitalWrite(GREEN_PIN, HIGH);
        digitalWrite(YELLOW_PIN, LOW);
        digitalWrite(RED_PIN, LOW);
      }
      else if (co2 > 500 && co2 < 1000)
      {
        digitalWrite(GREEN_PIN, LOW);
        digitalWrite(YELLOW_PIN, HIGH);
        digitalWrite(RED_PIN, LOW);
      }
      else if (co2 > 1000)
      {
        digitalWrite(GREEN_PIN, LOW);
        digitalWrite(YELLOW_PIN, LOW);
        digitalWrite(RED_PIN, HIGH);
      }
      if (WiFi.status() == WL_CONNECTED)
      {
        sendMeasurements();
      }
      else
      {
        Serial.println("No WiFi");
      }
      previousMeasurementMillis = millis();
    }
    if (WiFi.status() != WL_CONNECTED && millis() - previousWifiRetryMillis > WIFI_RETRY_INTERVAL)
    {
      WiFi.begin(SSID, WIFI_PASSWORD);
      previousWifiRetryMillis = millis();
    }
    delay(5000);
  }
  else if (millis() >= WARMUP_PERIOD)
  {
    sensorsReady = true;
    readMeasurements();
    printInfoSerial();
    printInfoLcd();
    if (WiFi.status() == WL_CONNECTED)
    {
      sendMeasurements();
    }
    else
    {
      Serial.println("No WiFi");
    }
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Warming up ");
    lcd.setCursor(0, 1);
    lcd.print(String(millis() / (1000 * 60)) + "/20 minutes");
    lcd.setCursor(0, 2);
    if (WiFi.status() != WL_CONNECTED)
    {
      lcd.print("No WiFi, retrying");
    }
    else
    {
      lcd.print("WiFi connected");
    }

    Serial.print("Waiting for sensors to stabilize, currently elapsed: ");
    Serial.print(millis() / (1000 * 60));
    Serial.println("/20 minutes");
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("No WiFi");
    }
    else
    {
      Serial.println("WiFi connected, SSID: " + String(SSID));
    }

    digitalWrite(GREEN_PIN, HIGH);
    delay(200);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(YELLOW_PIN, HIGH);
    delay(200);
    digitalWrite(YELLOW_PIN, LOW);
    digitalWrite(RED_PIN, HIGH);
    delay(200);
    digitalWrite(RED_PIN, LOW);

    if (WiFi.status() != WL_CONNECTED && millis() - previousWifiRetryMillis > WIFI_RETRY_INTERVAL)
    {
      WiFi.begin(SSID, WIFI_PASSWORD);
      previousWifiRetryMillis = millis();
    }
    delay(1000);
  }
}

void readMeasurements()
{
  myBME280.takeForcedMeasurement();
  temperatureC = myBME280.readTemperature() - 1.2; // self heating compensation
  relativeHumidity = myBME280.readHumidity();
  co2 = co2_sensor.getCO2();
  tvoc = analogRead(GAS_SENSOR_ANALOG_PIN);
  aqi.read(&pm_data);
}

void printInfoLcd()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CO2   " + String(co2) + " ppm");
  lcd.setCursor(0, 1);
  lcd.print("Gasses  " + String(tvoc) + " ppm");
  lcd.setCursor(0, 2);
  lcd.print("Temp  " + String(temperatureC) + " \xDF");
  lcd.print("C");
  lcd.setCursor(0, 3);
  lcd.print("RelHum " + String(relativeHumidity) + " %");

  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PM2.5 " + String(pm_data.pm25_env));
  lcd.setCursor(0, 1);
  lcd.print("PM10  " + String(pm_data.pm10_env));
  lcd.setCursor(0, 2);
  lcd.print("PM100 " + String(pm_data.pm100_env));

  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PM2.5 mg/0.1L " + String(pm_data.particles_10um));
  lcd.setCursor(0, 1);
  lcd.print("PM10  mg/0.1L " + String(pm_data.particles_25um));
  lcd.setCursor(0, 2);
  lcd.print("PM50  mg/0.1L " + String(pm_data.particles_50um));
  lcd.setCursor(0, 3);
  lcd.print("PM100 mg/0.1L " + String(pm_data.particles_100um));
}

void printInfoSerial()
{
  Serial.print("CO2 concentration : ");
  Serial.print(co2);
  Serial.println(" ppm");

  Serial.print("Gas concentration : ");
  Serial.print(tvoc);
  Serial.println(" ppm");

  Serial.println("BME280 data:");
  Serial.print(" Temperature: ");
  Serial.print(temperatureC, 2);
  Serial.println(" degrees C");

  Serial.print(" Pressure: ");
  Serial.print(myBME280.readPressure() / 100, 2);
  Serial.println(" Pa");

  Serial.print(" Altitude: ");
  Serial.print(myBME280.readAltitude(1013.25), 2);
  Serial.println("m");

  Serial.print(" %RH: ");
  Serial.print(relativeHumidity, 2);
  Serial.println(" %");

  Serial.println();

  Serial.println(F("---------------------------------------"));
  Serial.println(F("Concentration Units (standard)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: "));
  Serial.print(pm_data.pm10_standard);
  Serial.print(F("\t\tPM 2.5: "));
  Serial.print(pm_data.pm25_standard);
  Serial.print(F("\t\tPM 10: "));
  Serial.println(pm_data.pm100_standard);
  Serial.println(F("Concentration Units (environmental)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: "));
  Serial.print(pm_data.pm10_env);
  Serial.print(F("\t\tPM 2.5: "));
  Serial.print(pm_data.pm25_env);
  Serial.print(F("\t\tPM 10: "));
  Serial.println(pm_data.pm100_env);
  Serial.println(F("---------------------------------------"));
  Serial.print(F("Particles > 0.3um / 0.1L air:"));
  Serial.println(pm_data.particles_03um);
  Serial.print(F("Particles > 0.5um / 0.1L air:"));
  Serial.println(pm_data.particles_05um);
  Serial.print(F("Particles > 1.0um / 0.1L air:"));
  Serial.println(pm_data.particles_10um);
  Serial.print(F("Particles > 2.5um / 0.1L air:"));
  Serial.println(pm_data.particles_25um);
  Serial.print(F("Particles > 5.0um / 0.1L air:"));
  Serial.println(pm_data.particles_50um);
  Serial.print(F("Particles > 10 um / 0.1L air:"));
  Serial.println(pm_data.particles_100um);
  Serial.println(F("---------------------------------------"));
}

void sendMeasurements()
{
  WiFiClient client;
  HTTPClient http;
  http.begin(client, SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"temperature\":" + String(temperatureC) + ",";
  json += "\"humidity\":" + String(relativeHumidity) + ",";
  json += "\"co2\":" + String(co2) + ",";
  json += "\"tvoc\":" + String(tvoc) + ",";
  json += "\"pm25\":" + String(pm_data.pm25_env) + ",";
  json += "\"pm10\":" + String(pm_data.pm10_env);
  json += "}";

  Serial.println(json);

  int httpResponseCode = http.POST(json);
  delay(100);
  Serial.println("Response: " + String(httpResponseCode));
  http.end();
}