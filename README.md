# 🌫️ ESP32 Air Quality Monitor

A Wi-Fi enabled ESP32-based air quality monitoring system that measures environmental conditions, calculates AQI, displays data locally, hosts a web dashboard, and sends remote alerts.

---

## 🔧 Features

- Real-time temperature, humidity, and pressure monitoring (BME280)
- Dust concentration measurement and AQI calculation
- Raw air quality sensing using MQ135
- OLED display with multi-page interface
- Touch-based display navigation
- ESP32-hosted web dashboard with auto-refresh
- Telegram alerts for periodic AQI updates
- Battery voltage and percentage monitoring
- OTA firmware updates using ArduinoOTA

---

## 🧠 Hardware Used

- ESP32 Dev Module  
- BME280 environmental sensor  
- MQ135 gas sensor  
- Sharp dust sensor  
- SSD1306 OLED display  
- Li-ion battery with voltage divider  

---

## 🔌 Pin Configuration (Overview)

| Component | ESP32 Pin |
|--------|----------|
| BME280 | I²C (SDA/SCL) |
| MQ135 | ADC (GPIO 36) |
| Dust Sensor | ADC (GPIO 39), LED (GPIO 25) |
| OLED | I²C |
| Touch Input | GPIO 15 |
| Battery Sense | ADC (GPIO 35) |

---

## 📐 AQI & Comfort Logic

- Dust sensor voltage is converted to µg/m³
- AQI is calculated using breakpoint-based mapping
- Heat index is computed from temperature and humidity
- Pressure correction is applied to derive a “Real Feel” temperature

---

## 🌐 Web Dashboard

The ESP32 runs a lightweight HTTP server displaying:

- Temperature
- Humidity
- Pressure
- Dust concentration
- Calculated AQI

The dashboard is accessible via the ESP32’s local IP address and refreshes automatically.

---

## 🔐 Security Note

Sensitive data such as Wi-Fi credentials and Telegram bot tokens are stored locally in a `secrets.h` file and are excluded from version control using `.gitignore`.

---

## 🚀 Why This Project

This project demonstrates:

- Embedded C++ development on ESP32
- Multi-sensor data acquisition
- Signal processing and AQI computation
- ESP32 networking and OTA updates
- Real-world IoT system design
- Clean project structuring and documentation

---

## 📌 Future Improvements

- Historical data logging
- Cloud dashboard integration
- Power optimization using deep sleep
- Custom PCB design
- GPS integration for location-based AQI mapping

---

## 🧑‍💻 Author

**Fozan Manzoor**  
Embedded Systems • IoT • AI Evaluation
