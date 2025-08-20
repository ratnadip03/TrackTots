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