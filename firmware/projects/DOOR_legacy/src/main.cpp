/**
 * @file main.cpp
 * @brief Arduino UNO R4 WiFi RFID Tür-Steuerung
 *
 * Hardware:
 *   Arduino UNO R4 WiFi
 *   MFRC522 RFID-Leser (SPI)
 *   Schrittmotor-Treiber (A4988/DRV8825) via AccelStepper
 *   74HC595 Schieberegister (LEDs, Buzzer, Motor-Enable)
 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <AccelStepper.h>
#include <WiFiS3.h>
#include <ArduinoOTA.h>
#include <WDT.h>
#include "settings.h"

// --- Secrets via Build-Flags (platformio.secrets.ini) ---
#ifndef SERVER_IP
  #define SERVER_IP "NOT_SET"
#endif
#ifndef AUTHENTICATION_TOKEN
  #define AUTHENTICATION_TOKEN "NoToken"
#endif
#ifndef MACHINE_ID
  #define MACHINE_ID "tuer-01"
#endif
#ifndef MACHINE_NAME
  #define MACHINE_NAME "tuer"
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID "NOT_SET"
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS "NOT_SET"
#endif
#ifndef OTA_HOSTNAME
  #define OTA_HOSTNAME "tuer"
#endif

//------------------------------------------------------------------------------
// Pin-Definitionen (unverändert gegenüber Programm.ino)
//------------------------------------------------------------------------------

#define PIN_STEP          2
#define PIN_DIR           3
#define RFID_RST_PIN      5
#define RFID_SS_PIN       6
#define PIN_SR_SER        7   ///< Schieberegister Serial Data
#define PIN_SR_RCLK       8   ///< Schieberegister Latch
#define PIN_SR_SRCLK      9   ///< Schieberegister Clock
#define PIN_ENDSCHALTER   A0  ///< Endschalter (Heimposition)
#define PIN_REED          A1  ///< Reed-Kontakt (Tür offen/zu)
#define PIN_ABSCHLIESSEN  A2  ///< Manueller Schließ-Taster
#define PIN_TERASSE       A3  ///< Terrassen-Taster (zukünftig)

//------------------------------------------------------------------------------
// States
//------------------------------------------------------------------------------

enum State {
  STANDBY,        ///< Wartet auf RFID-Karte
  IDENTIFICATION, ///< Server-Anfrage läuft (LED-Feedback)
  DOOR_OPEN,      ///< Tür offen, wartet auf Schließ-Befehl
  CLOSING,        ///< Motor schließt Tür
  RESET           ///< Abgelehnt/Fehler, 2 s Anzeige
};

State currentState = STANDBY;
unsigned long resetEnteredAt = 0;

//------------------------------------------------------------------------------
// Schieberegister-Zustand (aktiv LOW: true = LED/Motor an)
//------------------------------------------------------------------------------

bool srRed         = false;
bool srYellow      = false;
bool srGreen       = false;
bool srSummer      = false;
bool srMotorEnable = false;

//------------------------------------------------------------------------------
// Globale Instanzen
//------------------------------------------------------------------------------

WiFiClient client;

WiFiServer telnetServer(23);
WiFiClient telnetClient;

class TeeStream : public Print {
  size_t write(uint8_t c) override {
    Serial.write(c);
    if (telnetClient && telnetClient.connected()) telnetClient.write(c);
    return 1;
  }
  size_t write(const uint8_t* buf, size_t size) override {
    Serial.write(buf, size);
    if (telnetClient && telnetClient.connected()) telnetClient.write(buf, size);
    return size;
  }
} ser;

MFRC522      mfrc522(RFID_SS_PIN, RFID_RST_PIN);
AccelStepper stepper(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);

static const uint8_t UID_BUF_LEN = 32;
char lastUid[UID_BUF_LEN] = "";
int  retryCount = 0;

//------------------------------------------------------------------------------
// Forward Declarations
//------------------------------------------------------------------------------

void updateSR();
void openDoor(bool forceOpen);
void closeDoor();
char checkServer(const char* rfid);
bool readUID(char* buf, uint8_t bufSize);

//------------------------------------------------------------------------------
// Setup
//------------------------------------------------------------------------------

void setup() {
  pinMode(PIN_SR_SER,      OUTPUT);
  pinMode(PIN_SR_RCLK,     OUTPUT);
  pinMode(PIN_SR_SRCLK,    OUTPUT);
  pinMode(PIN_ENDSCHALTER, INPUT_PULLUP);
  pinMode(PIN_REED,        INPUT_PULLUP);
  pinMode(PIN_ABSCHLIESSEN,INPUT_PULLUP);

  srYellow = true;
  updateSR();

  stepper.setMaxSpeed(STEPPER_SPEED);
  stepper.setAcceleration(STEPPER_ACCEL);

  Serial.begin(9600);
  { unsigned long t = millis(); while (!Serial && millis() - t < 3000); }

  ser.println(F("Init WiFi"));
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long wifiStart = millis();
  bool ledState = false;
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    ledState = !ledState;
    srRed = srYellow = srGreen = ledState;
    updateSR();
    delay(300);
    ser.print('.');
  }
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - wifiStart < 15000) {
    ledState = !ledState;
    srRed = srYellow = srGreen = ledState;
    updateSR();
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    ser.print(F("\nVerbunden, IP: "));
    ser.println(WiFi.localIP());
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
    ser.print(F("MAC: "));
    ser.println(macStr);
    // WDT während OTA am Leben halten: Staging-Erase (~1.5s) und Apply-Erase (~1.5s)
    // liegen jeweils innerhalb des 4s-Fensters wenn an den richtigen Stellen refreshed wird.
    ArduinoOTA.onStart([]() { WDT.refresh(); });
    ArduinoOTA.beforeApply([]() { WDT.refresh(); });
    ArduinoOTA.begin(WiFi.localIP(), OTA_HOSTNAME, "", InternalStorage);
    ser.print(F("[OTA] Bereit: "));
    ser.println(OTA_HOSTNAME);
    telnetServer.begin();
    ser.println(F("[Telnet] Server auf Port 23."));
  } else {
    ser.println(F("\nWiFi fehlgeschlagen – weiter ohne Netz"));
  }

  ser.println(F("Init RFID"));
  SPI.begin();
  mfrc522.PCD_Init();

  srYellow = false;
  updateSR();

  WDT.begin(WDT_TIMEOUT_MS);
  ser.println(F("Bereit."));
}

//------------------------------------------------------------------------------
// Loop
//------------------------------------------------------------------------------

void loop() {
  WDT.refresh();
  ArduinoOTA.poll();
  { WiFiClient c = telnetServer.available(); if (c) { if (telnetClient) telnetClient.stop(); telnetClient = c; } }

  // WiFi-Reconnect (non-blocking)
  static unsigned long lastWifiAttempt = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
    lastWifiAttempt = millis();
    ser.println(F("WiFi getrennt – Reconnect..."));
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }

  // LED-Ausgabe je nach State
  switch (currentState) {
    case STANDBY:
      srRed = false; srYellow = true;  srGreen = false; break;
    case IDENTIFICATION:
      srRed = false; srYellow = true;  srGreen = true;  break;
    case DOOR_OPEN:
      srRed = false; srYellow = false; srGreen = true;  break;
    case CLOSING:
      srRed = true;  srYellow = true;  srGreen = true;  break;
    case RESET:
      srRed = true;  srYellow = false; srGreen = false;  break;
  }
  updateSR();

  switch (currentState) {

    case STANDBY: {
      if (digitalRead(PIN_ABSCHLIESSEN) == LOW) {
        currentState = CLOSING;
        break;
      }
      char uid[UID_BUF_LEN];
      if (!readUID(uid, sizeof(uid))) {
        delay(50);
        break;
      }
      // Retry-Logik: gleiche Karte zweimal → Tür force-öffnen
      if (strcmp(uid, lastUid) == 0) {
        retryCount++;
      } else {
        retryCount = 0;
      }

      currentState = IDENTIFICATION;
      srRed = false; srYellow = false; srGreen = true;
      updateSR();

      char result = checkServer(uid);
      if (result == 1) {
        openDoor(retryCount >= 2);
        retryCount = 0;
        strncpy(lastUid, uid, UID_BUF_LEN - 1);
        lastUid[UID_BUF_LEN - 1] = '\0';
        currentState = STANDBY;
      } else if (result == 0) {
        ser.println(F("Karte abgelehnt."));
        retryCount = 0;
        lastUid[0] = '\0';
        resetEnteredAt = millis();
        currentState = RESET;
      } else {
        ser.println(F("Server nicht erreichbar."));
        resetEnteredAt = millis();
        currentState = RESET;
      }
      break;
    }

    case DOOR_OPEN:
      if (digitalRead(PIN_ABSCHLIESSEN) == LOW) {
        currentState = CLOSING;
      }
      delay(50);
      break;

    case CLOSING:
      closeDoor();
      lastUid[0] = '\0';
      retryCount = 0;
      currentState = STANDBY;
      break;

    case RESET:
      if (millis() - resetEnteredAt >= 2000) {
        currentState = STANDBY;
      }
      delay(50);
      break;

    default:
      break;
  }
}

//------------------------------------------------------------------------------
// Server-Auth
//------------------------------------------------------------------------------

static void blinkGreen() {
  static unsigned long last = 0;
  static bool state = false;
  if (millis() - last >= 150) {
    state = !state;
    srGreen = state;
    updateSR();
    last = millis();
  }
}

char checkServer(const char* rfid) {
  if (WiFi.status() != WL_CONNECTED) {
    ser.println(F("Kein WiFi – Server-Check übersprungen."));
    return -1;
  }
  ser.println(F("Verbinde mit Server..."));
  IPAddress serverAddr;
  if (WiFi.hostByName(SERVER_IP, serverAddr) != 1) {
    ser.println(F("DNS fehlgeschlagen."));
    return -1;
  }
  blinkGreen();
  if (!client.connect(serverAddr, 80)) {
    client.stop();
    ser.println(F("Verbindung fehlgeschlagen."));
    return -1;
  }

  ser.println(F("Verbunden."));
  char req[256];
  snprintf(req, sizeof(req), "GET /check_key/%s/%s HTTP/1.0", AUTHENTICATION_TOKEN, rfid);
  client.println(req);
  snprintf(req, sizeof(req), "Host: %s", SERVER_IP);
  client.println(req);
  client.println();

  char message[128] = "";
  uint8_t msgLen = 0;
  bool inBody = false;
  char lineBuf[128] = "";
  uint8_t lineLen = 0;
  unsigned long t = millis();

  while (client.connected() && millis() - t < 5000) {
    WDT.refresh();
    blinkGreen();
    while (client.available()) {
      char c = client.read();
      t = millis();
      if (!inBody) {
        if (c == '\n') {
          // Leerzeile = Ende der HTTP-Header
          if (lineLen <= 1) inBody = true;
          lineLen = 0;
        } else if (lineLen < sizeof(lineBuf) - 1) {
          lineBuf[lineLen++] = c;
        }
      } else {
        if (msgLen < sizeof(message) - 1) message[msgLen++] = c;
      }
    }
    if (inBody && msgLen > 0) break;
  }
  client.stop();

  ser.print(F("Antwort: "));
  ser.println(message);
  if (strstr(message, "true") != nullptr) return 1;
  return 0;
}

//------------------------------------------------------------------------------
// RFID-Lesen
//------------------------------------------------------------------------------

bool readUID(char* buf, uint8_t bufSize) {
  if (!mfrc522.PICC_IsNewCardPresent()) return false;
  if (!mfrc522.PICC_ReadCardSerial())   return false;
  buf[0] = '\0';
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    char hex[4];
    snprintf(hex, sizeof(hex), i < mfrc522.uid.size - 1 ? "%02x:" : "%02x",
             mfrc522.uid.uidByte[i]);
    strncat(buf, hex, bufSize - strlen(buf) - 1);
  }
  ser.print(F("Karte: "));
  ser.println(buf);
  mfrc522.PCD_Init();  // Re-init wie im Original
  return true;
}

//------------------------------------------------------------------------------
// Tür öffnen
//------------------------------------------------------------------------------

void openDoor(bool forceOpen) {
  if (digitalRead(PIN_ENDSCHALTER) == LOW || forceOpen) {
    srMotorEnable = true;
    srGreen = true;
    updateSR();

    stepper.setSpeed(STEPPER_SPEED);
    unsigned long motorStart = millis();
    while (digitalRead(PIN_ENDSCHALTER) == LOW) {
      if (millis() - motorStart >= MOTOR_TIMEOUT_MS) {
        ser.println(F("openDoor: Endschalter-Timeout – Motor gestoppt."));
        break;
      }
      WDT.refresh();
      stepper.runSpeed();
    }
    long pos = stepper.currentPosition();
    motorStart = millis();
    while (stepper.currentPosition() - pos < STEPS_TO_OPEN) {
      if (millis() - motorStart >= MOTOR_TIMEOUT_MS) {
        ser.println(F("openDoor: Extra-Schritte-Timeout – Motor gestoppt."));
        break;
      }
      WDT.refresh();
      stepper.runSpeed();
    }
    stepper.setSpeed(0);

    srMotorEnable = false;
    updateSR();
  }

  srGreen  = true;
  srSummer = true;
  updateSR();
  delay(2000);
  srGreen  = false;
  srSummer = false;
  updateSR();
}

//------------------------------------------------------------------------------
// Tür schließen
//------------------------------------------------------------------------------

void closeDoor() {
  // Warten bis Tür physisch offen (Reed gibt frei)
  unsigned long reedWait = millis();
  while (digitalRead(PIN_REED) == HIGH) {
    if (millis() - reedWait >= DOOR_REED_TIMEOUT_MS) {
      ser.println(F("closeDoor: Reed-Timeout – Schließen abgebrochen."));
      return;
    }
    WDT.refresh();
    delay(50);
  }
  delay(2000);

  srMotorEnable = true;
  updateSR();

  long posStart = stepper.currentPosition();
  stepper.setSpeed(-STEPPER_SPEED);
  while (digitalRead(PIN_ENDSCHALTER) == HIGH && (posStart - stepper.currentPosition()) < STEPS_MAX) {
    WDT.refresh();
    stepper.runSpeed();
  }
  long pos = stepper.currentPosition();
  while ((pos - stepper.currentPosition()) < STEPS_TO_CLOSE) {
    WDT.refresh();
    stepper.runSpeed();
  }

  srMotorEnable = false;
  updateSR();

  // Visuelles Feedback: 2× alle LEDs blinken
  for (int i = 0; i < 2; i++) {
    srRed = true; srYellow = true; srGreen = true;
    updateSR(); delay(500);
    srRed = false; srYellow = false; srGreen = false;
    updateSR(); delay(500);
  }
}

//------------------------------------------------------------------------------
// Schieberegister aktualisieren (74HC595, aktiv LOW)
// Bit-Reihenfolge (zuletzt geshiftet = Q0):
//   [0] MotorEnable  [1] Summer  [2] Grün  [3] Gelb  [4] Rot
//------------------------------------------------------------------------------

void updateSR() {
  digitalWrite(PIN_SR_RCLK, LOW);

  // Motor Enable: aktiv LOW (invertiert)
  digitalWrite(PIN_SR_SER, srMotorEnable ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  // Summer: aktiv LOW
  digitalWrite(PIN_SR_SER, srSummer ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  // Grün: aktiv LOW
  digitalWrite(PIN_SR_SER, srGreen ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  // Gelb: aktiv LOW
  digitalWrite(PIN_SR_SER, srYellow ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  // Rot: aktiv LOW
  digitalWrite(PIN_SR_SER, srRed ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  digitalWrite(PIN_SR_RCLK, HIGH);
  digitalWrite(PIN_SR_RCLK, LOW);
}
