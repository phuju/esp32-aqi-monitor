// -------- ESP32 Web Server Code --------
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include "secrets.h"


int currentIndex = 0;
unsigned long lastDisplaySwitch = 0;
const unsigned long displayInterval = 60*1000;
const int totalPages = 8;

unsigned long previousMillis = 0;
const long interval = 1000;

bool touchDetected = false;
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE = 500;

unsigned long lastAlertTime = 0;
const unsigned long alertInterval = 6*10*60*1000;

WiFiServer server(80);

// Sensor values (global so web handler can access)
float temp = 0.0;
float hum = 0.0;
float press = 0.0;
float realFeel = 0.0;
int mq135raw = 0;
float heatIndex = 0.0;
float pressureEffect = 0.0;
double dustDensity = 0.0;
double aqi = 0.0;
float batteryVolt = 0.0;
int batteryPercent = 0;

double Voc = 0.6;  // <-- Declare here with a default value (optional)
const double K = 0.005;  // <-- Sensitivity factor for Sharp dust sensor

Adafruit_BME280 bme;

#define TOUCH_PIN 15

// OLED Display Configuration
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// MQ135 config
#define MQ135_PIN 36

// Dust Sensor Configuration
#define DUST_LED_PIN 25
#define DUST_ANALOG_PIN 39
#define DELAY_BEFORE_SAMPLING 280
#define DELAY_AFTER_SAMPLING 40
#define DELAY_LED_OFF 9680

#define BATTERY_PIN 35

void setupWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;
  Serial.println("New Client.");
  String currentLine = "";
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') {
        if (currentLine.length() == 0) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();

          // Start of HTML
          client.println("<!DOCTYPE html><html><head>");
          client.println("<meta charset='UTF-8'>");
          client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
          client.println("<meta http-equiv='refresh' content='3'>");  // Auto-refresh every 3 sec
          client.println("<title>ESP32 AQI Monitor</title>");
          
          // Simple CSS styling
          client.println("<style>");
          client.println("body { font-family: Arial; background: #f0f0f0; color: #333; text-align: center; }");
          client.println("h2 { background: #4CAF50; color: white; padding: 10px 0; margin: 0; }");
          client.println(".data { display: inline-block; background: white; padding: 15px 25px; border-radius: 10px; box-shadow: 2px 2px 8px rgba(0,0,0,0.1); margin-top: 20px; }");
          client.println("p { font-size: 18px; margin: 10px 0; }");
          client.println("</style></head><body>");
          
          // Webpage content
          client.println("<h2>ESP32 Air Quality Monitor</h2>");
          client.println("<div class='data'>");
          client.print("<p><strong>Temperature:</strong> "); client.print(temp); client.println(" °C</p>");
          client.print("<p><strong>Humidity:</strong> "); client.print(hum); client.println(" %</p>");
          client.print("<p><strong>Pressure:</strong> "); client.print(press); client.println(" hPa</p>");
          client.print("<p><strong>FeelsLike:</strong> "); client.print(realFeel, 1); client.println(" °C</p>");
          client.print("<p><strong>rAQI:</strong> "); client.print(mq135raw);
          client.print("<p><strong>Dust:</strong> "); client.print(dustDensity, 1); client.println(" µg/m³</p>");
          client.print("<p><strong>AQI:</strong> "); client.print(aqi, 0); client.println("</p>");
          client.println("</div></body></html>");

          break;
        } else {
          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }
  delay(1);
  client.stop();
  Serial.println("Client Disconnected.");
}

void setup() {


  Serial.begin(115200);
  setupWiFi();
  ArduinoOTA.begin();

  pinMode(TOUCH_PIN, INPUT);

  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH);

  // Calibrate Voc (baseline voltage) in clean air

  double vocSum = 0;
  for (int i = 0; i < 100; i++) {
    vocSum += getDustVoltage();  // Reads voltage from dust sensor
    delay(50);
  }
  Voc = vocSum / 100.0;
  Serial.print("Calibrated Voc: ");
  Serial.println(Voc);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed to start");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Starting up sensors...");
  display.display();
  delay(5000); // Warm-up

  if (!bme.begin(0x76)) {
  Serial.println("Could not find BME280 sensor!");
  while (1);
  }
}

double getDustVoltage() {
  digitalWrite(DUST_LED_PIN, LOW);
  delayMicroseconds(DELAY_BEFORE_SAMPLING);
  double analogOutput = analogRead(DUST_ANALOG_PIN);
  delayMicroseconds(DELAY_AFTER_SAMPLING);
  digitalWrite(DUST_LED_PIN, HIGH);
  delayMicroseconds(DELAY_LED_OFF);
  return analogOutput / 4095.0 * 3.3;
}

double getDustDensity(double voltage) {
  const double Voc = 0.6;
  const double K = 0.005;
  double density = (voltage - Voc) / K;
  return (density < 0) ? 0 : density;
}

double getAQI(double ugm3) {
  double aqiL = 0, aqiH = 0, bpL = 0, bpH = 0;

  if (ugm3 <= 35)      { aqiL = 0; aqiH = 50;  bpL = 0; bpH = 35; }
  else if (ugm3 <= 75) { aqiL = 50; aqiH = 100; bpL = 35; bpH = 75; }
  else if (ugm3 <= 115){ aqiL = 100; aqiH = 150; bpL = 75; bpH = 115; }
  else if (ugm3 <= 150){ aqiL = 150; aqiH = 200; bpL = 115; bpH = 150; }
  else if (ugm3 <= 250){ aqiL = 200; aqiH = 300; bpL = 150; bpH = 250; }
  else if (ugm3 <= 350){ aqiL = 300; aqiH = 400; bpL = 250; bpH = 350; }
  else                 { aqiL = 400; aqiH = 500; bpL = 350; bpH = 500; }

  return ((aqiH - aqiL) / (bpH - bpL)) * (ugm3 - bpL) + aqiL;
}

float computeHeatIndex(float tempC, float humidity) {
  float tempF = tempC * 1.8 + 32.0;
  float hiF = -42.379 + 2.04901523 * tempF + 10.14333127 * humidity
              - 0.22475541 * tempF * humidity - 0.00683783 * tempF * tempF
              - 0.05481717 * humidity * humidity
              + 0.00122874 * tempF * tempF * humidity
              + 0.00085282 * tempF * humidity * humidity
              - 0.00000199 * tempF * tempF * humidity * humidity;
  return (hiF - 32) / 1.8;
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ArduinoOTA.handle();
    handleClient();
    
    float batteryVolt = analogRead(BATTERY_PIN) * (3.3 / 4095.0) * 3;
    batteryPercent = map(batteryVolt * 100, 600, 780, 0, 100);
    batteryPercent = constrain(batteryPercent, 0, 100); 
    
    temp = bme.readTemperature();
    hum = bme.readHumidity();
    press = bme.readPressure() / 100.0F;
    
    if (isnan(temp) || isnan(hum) || isnan(press)) {
      Serial.println("Failed to read from BME sensor!");
      temp = 0;
      hum = 0;
      press = 0;
      }
      
    heatIndex = computeHeatIndex(temp, hum);
    float press = bme.readPressure() / 100.0F;
    float pressureEffect = 0;
    if (press < 1000) {
      pressureEffect = (1000 - press) * 0.02;  // amplify discomfort
      } else if (press > 1020) {
        pressureEffect = -((press - 1020) * 0.015); // crisper feel
      }

    realFeel = heatIndex + pressureEffect;
    mq135raw = analogRead(MQ135_PIN);
    
    String airQualityLabel;
    if (mq135raw <= 200) airQualityLabel = "Good";
    else if (mq135raw <= 400) airQualityLabel = "Moderate";
    else if (mq135raw <= 700) airQualityLabel = "Unhealthy";
    else if (mq135raw <= 800) airQualityLabel = "Very Unhealthy";
    else airQualityLabel = "Hazardous";
    
    
    double dustVoltage = getDustVoltage();
    dustDensity = getDustDensity(dustVoltage);
    aqi = getAQI(dustDensity);
    
    unsigned long now = millis();
    
    if (now - lastAlertTime > alertInterval) {
      String message = "📡 AQI Update:\n";
      message += "Temp: " + String(temp) + "°C\n";
      message += "Humidity: " + String(hum) + "%\n";
      message += "Pressure: " + String(press) + " hPa\n";
      message += "Feels Like: " + String(realFeel) + "°C\n";
      message += "rAQI: " + String(mq135raw) + "\n";
      message += "Dust: " + String(dustDensity) + " µg/m³\n";
      message += "AQI: " + String(aqi);
      
      sendMessage(message);
      lastAlertTime = now;
    }
    
    Serial.print("Temp: "); Serial.print(temp); Serial.print("C, ");
    Serial.print("Hum: "); Serial.print(hum); Serial.print("%, ");
    Serial.print("Press: "); Serial.print(press); Serial.print("hPa, ");
    Serial.print("FeelsLike: "); Serial.print(realFeel); Serial.print("C, ");
    Serial.print("mqraw: "); Serial.print(mq135raw); Serial.print(", ");
    Serial.print("Dust: "); Serial.print(dustDensity, 1); Serial.print("ug/m3, ");
    Serial.print("V: "); Serial.print(dustVoltage, 1); Serial.print("v, ");
    Serial.print("AQI: "); Serial.println(aqi, 0);
    Serial.print("Battery Voltage: "); Serial.print(batteryVolt, 2); Serial.print(" V | ");
    Serial.print("Battery: "); Serial.print(batteryPercent); Serial.println(" %");
    
  }
  displayfunc();
  displaycycle();
  display.display();
}

void sendMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + message;
    http.begin(url);
    int httpResponseCode = http.GET();
    http.end();
    Serial.print("Telegram message sent, code: ");
    Serial.println(httpResponseCode);
  }
}

void displayfunc(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  switch (currentIndex) {
    case 0:
      display.setCursor(0, 5);
      display.println("Temp:");
      display.setCursor(0, 30);
      display.print(temp); display.println(" C");
      break;
    case 1:
      display.setCursor(0, 5);
      display.println("Hum:");
      display.setCursor(0, 30);
      display.print(hum); display.println(" %");
      break;
    case 2:
      display.setCursor(0, 5);
      display.println("Press:");
      display.setCursor(0, 30);
      display.print(press); display.println(" hPa");
      break;
    case 3:
      display.setCursor(0, 5);
      display.println("FeelsLike:");
      display.setCursor(0, 30);
      display.print(realFeel); display.println(" C");
      break;
    case 4:
      display.setCursor(0, 5);
      display.println("rAQI:");
      display.setCursor(0, 30);
      display.print(mq135raw);
      display.print(" ");
      if (mq135raw <= 200) display.println("Good");
      else if (mq135raw <= 400) display.println("Moderate");
      else if (mq135raw <= 700) display.println("Unhealthy");
      else if (mq135raw <= 800) display.println("Very Unhealthy");
      else display.println("Hazardous");
      break;
    case 5:
      display.setCursor(0, 5);
      display.println("Dust:");
      display.setCursor(0, 30);
      display.print(dustDensity, 1); display.println(" ug/m3");
      break;
    case 6:
      display.setCursor(0, 5);
      display.println("AQI:");
      display.setCursor(0, 30);
      display.print(aqi, 0);
      display.println(" ");
      if (aqi <= 50) display.println("Good");
      else if (aqi <= 100) display.println("Moderate");
      else if (aqi <= 150) display.println("Unhealthy");
      else if (aqi <= 200) display.println("Unhealthy");
      else if (aqi <= 300) display.println("Very Unhealthy");
      else display.println("Hazardous");
      break;
    case 7:
      display.setCursor(0, 5);
      if (batteryVolt <= 7.0) {
        display.println("Battery:");
        display.setCursor(0, 30);
        display.print(batteryPercent); display.print(" %");
        }
      else if (batteryVolt >= 7.2) {
        display.println("Charging");
        }
      break;
  }
}

void displaycycle(){

  unsigned long currentMillis = millis();

  if (currentMillis - lastDisplaySwitch > displayInterval) {
    lastDisplaySwitch = currentMillis;

    displayfunc();

    display.display();
    currentIndex = (currentIndex + 1) % totalPages;
  }

  if (digitalRead(TOUCH_PIN) == HIGH && millis() - lastTouchTime > TOUCH_DEBOUNCE) {
    touchDetected = true;
    lastTouchTime = millis();
  }
  if (touchDetected) {
    touchDetected = false;
    currentIndex = (currentIndex + 1) % totalPages;

    displayfunc();
    display.display();
  }
}