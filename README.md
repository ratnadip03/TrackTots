# TrackTots
🚨 TrackTots – A multipurpose IoT-based safety gadget for women, night travelers, miners, and border patrolling. Monitors health, detects emergencies, and sends live GPS location via SMS.
# 📦 TrackTots — Multipurpose Safety Gadget (AIoT)

> **Use-cases:** Women’s safety • Night travelers • Miners’ safety • Border patrolling • Children/Elderly safety

TrackTots is a portable **IoT safety device** built on **ESP32** that can **auto-detect emergencies** (abnormal heart-rate/SpO₂, scream-level sound) or accept a **manual SOS button**, then send an **SMS alert with live GPS location** to preset contacts via **SIM800L GSM**.

---

## ✨ Key Features

* **Manual SOS** via push‑button
* **Auto SOS** triggers:

  * **Loud sound** (scream/shout) using **KY‑038**
  * **Abnormal vitals** using **MAX30100** (HR/SpO₂ changes)
* **Live GPS** link in SMS (Google Maps URL)
* **Vital stats** appended in alert SMS (HR, SpO₂, sound level)
* **Offline ready** — works on **SMS** (no internet)
* **Configurable thresholds & contacts** in code

---

## 🧩 Hardware

* **ESP32** Dev Board (WROOM/DevKit)
* **NEO‑6M GPS** module
* **SIM800L GSM** module (+ stable 4V supply, capable of \~2A burst)
* **MAX30100** Pulse Oximeter (I²C)
* **KY‑038 (HW‑484 V0.2)** sound sensor (analog)
* **Push button** (with pull‑up)
* **Rechargeable battery pack** (+ **optional step‑up** dedicated for SIM800L)

---

## 📌 Pinout (ESP32)

| Module                  | Signal         | ESP32 Pin                    |
| ----------------------- | -------------- | ---------------------------- |
| **MAX30100**            | SDA            | **GPIO 21**                  |
|                         | SCL            | **GPIO 22**                  |
| **KY‑038 (Analog Out)** | AO             | **GPIO 34**                  |
| **Push Button**         | IN             | **GPIO 35** *(with pull-up)* |
| **SIM800L**             | TX → ESP32 RX1 | **GPIO 27**                  |
|                         | RX ← ESP32 TX1 | **GPIO 26**                  |
| **NEO‑6M GPS**          | TX → ESP32 RX2 | **GPIO 16**                  |
|                         | RX ← ESP32 TX2 | **GPIO 17**                  |

---

## 📚 Arduino IDE Setup

1. **Board:** `ESP32 Dev Module` (install ESP32 core via Boards Manager)
2. **Libraries:**

   * `TinyGPSPlus` (by Mikal Hart)
   * `MAX30100_PulseOximeter`

---

## ⚙️ Configure Before Upload

In the code (below), edit:

* **Contact number** in `sendSMS()` → replace with your number (with country code)
* **Thresholds**

  * `soundThreshold` (default `30`)

> Example number format for India: `+91XXXXXXXXXX`

---

## ▶️ How It Works (Logic)

* Device continuously **samples sound**, **updates pulse oximeter** (HR/SpO₂), **reads GPS**
* **Triggers SOS if:**

  * Sound exceeds threshold
  * Manual button pressed
  * BPM changes abruptly (>10 bpm)
* On SOS: sends SMS with **location + vitals + reason**

---

## 🧰 Final Ready-to-Upload Code (\*.ino)

> Save this as `TrackTots.ino` and upload to ESP32

```cpp
#include <HardwareSerial.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <TinyGPSPlus.h>

#define GSM_RX 27
#define GSM_TX 26
#define GPS_RX 16
#define GPS_TX 17
#define SOUND_PIN 34
#define BUTTON_PIN 35

HardwareSerial sim800(1); // UART1
HardwareSerial gpsSerial(2); // UART2
PulseOximeter pox;
TinyGPSPlus gps;

unsigned long lastSendTime = 0;
const unsigned long cooldown = 15000; // 15 seconds
float lastBPM = 0;
int soundThreshold = 30;
bool lastButtonState = HIGH;

void setup() {
  Serial.begin(115200);
  sim800.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Wire.begin();

  pinMode(SOUND_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!pox.begin()) {
    Serial.println("MAX30100 init failed");
  } else {
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  }
}

String getGPSLocation() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
  if (gps.location.isValid()) {
    String mapLink = "https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    return "Lat: " + String(gps.location.lat(), 6) + ", Lon: " + String(gps.location.lng(), 6) + "\n" + mapLink;
  } else {
    return "Location: Not fixed";
  }
}

void sendSMS(String msg) {
  sim800.println("AT+CMGF=1");
  delay(500);
  sim800.println("AT+CMGS=\"+918329291562\""); // Replace with your number
  delay(500);
  sim800.print(msg);
  sim800.write(26); // CTRL+Z
  Serial.println("SMS sent: " + msg);
}

void loop() {
  pox.update();
  float bpm = pox.getHeartRate();
  float spo2 = pox.getSpO2();
  int soundValue = analogRead(SOUND_PIN);
  bool buttonState = digitalRead(BUTTON_PIN);
  String location = getGPSLocation();
  unsigned long now = millis();

  // Condition 1: High Sound
  if (soundValue > soundThreshold && now - lastSendTime > cooldown) {
    String msg = "Alert: High noise!\nSound: " + String(soundValue) +
                 "\nBPM: " + String(bpm, 1) + "\nSpO2: " + String(spo2, 1) +
                 "\n" + location;
    sendSMS(msg);
    lastSendTime = now;
  }

  // Condition 2: Button Press
  if (buttonState == LOW && lastButtonState == HIGH && now - lastSendTime > cooldown) {
    String msg = "🚨 Emergency! Button pressed.\nSound: " + String(soundValue) +
                 "\nBPM: " + String(bpm, 1) + "\nSpO2: " + String(spo2, 1) +
                 "\n" + location;
    sendSMS(msg);
    lastSendTime = now;
  }
  lastButtonState = buttonState;

  // Condition 3: BPM change
  if (abs(bpm - lastBPM) > 10 && bpm > 40 && bpm < 180 && now - lastSendTime > cooldown) {
    String msg = "Alert: BPM changed.\nBPM: " + String(bpm, 1) +
                 "\nSpO2: " + String(spo2, 1) + "\nSound: " + String(soundValue) +
                 "\n" + location;
    sendSMS(msg);
    lastSendTime = now;
  }

  lastBPM = bpm;
}
```
