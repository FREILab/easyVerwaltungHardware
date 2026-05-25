/**
 * ESP8266 — State Machine + WiFi + OTA für RobotDyn UNO+WiFi R3
 *
 * Verantwortung: Gesamte Türlogik, WiFi, OTA via mDNS (tuer-XX.local),
 * HTTP-Server-Check. Hardware-Steuerung delegiert an ATmega per UART.
 *
 * State Machine:
 *   STANDBY      → LED:010 (Gelb). Wartet auf RFID oder BTN:CLOSE.
 *   IDENTIFICATION → LED:001 (Grün). checkServer() → OPEN/DENY/NONET.
 *   RESET        → LED:100 (Rot). 2s Anzeige, dann zurück zu STANDBY.
 *
 * Build-Flags (aus platformio.ini / platformio.secrets.ini):
 *   SERVER_IP, AUTHENTICATION_TOKEN, OTA_HOSTNAME
 *   WIFI_LOCAL_IP, WIFI_GATEWAY, WIFI_SUBNET
 *
 * WIFI_SSID / WIFI_PASS kommen aus include/secret.h (nicht in Git).
 *
 * DIP für Flash (einmalig): SW5+SW6+SW7 ON, Rest OFF.
 * DIP Normal-Betrieb (ATmega↔ESP): SW1+SW2 ON, Rest OFF.
 * OTA danach: pio run -e door_01_esp_ota --target upload
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <TelnetStream.h>
#include "settings.h"

#define DBG TelnetStream

#if __has_include("secret.h")
  #include "secret.h"
#endif

// --- Secrets via Build-Flags ---
#ifndef SERVER_IP
  #define SERVER_IP "NOT_SET"
#endif
#ifndef AUTHENTICATION_TOKEN
  #define AUTHENTICATION_TOKEN "NoToken"
#endif
#ifndef OTA_HOSTNAME
  #define OTA_HOSTNAME "tuer"
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID "NOT_SET"
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS "NOT_SET"
#endif
#ifndef WIFI_LOCAL_IP
  #define WIFI_LOCAL_IP ""
#endif
#ifndef WIFI_GATEWAY
  #define WIFI_GATEWAY ""
#endif
#ifndef WIFI_SUBNET
  #define WIFI_SUBNET ""
#endif

// ─────────────────────────────────────────────────────────
// State Machine
// ─────────────────────────────────────────────────────────

enum State { STANDBY, IDENTIFICATION, RESET };
State currentState = STANDBY;

unsigned long resetEnteredAt = 0;

static const uint8_t UID_BUF_LEN = 32;
char lastUid[UID_BUF_LEN] = "";
int  retryCount = 0;

// ─────────────────────────────────────────────────────────
// Forward Declarations
// ─────────────────────────────────────────────────────────

void sendLED(bool r, bool y, bool g);
char checkServer(const char* rfid);
bool waitMotorOK();
void processSerial();
void handleRFID(const char* uid);
bool connectWiFi(unsigned long timeoutMs);
bool configureStaticIp();
void wifiReconnect();

bool useStaticIp = false;

// ─────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────

void setup() {
  // Serial0 = UART zum ATmega (Normal-Betrieb DIP 7+8)
  // Bei DIP SW5+SW6 ON (ESP Serial Monitor): Serial-Output hier sichtbar
  Serial.begin(9600);
  Serial.println(F("[ESP] Boot"));

  useStaticIp = configureStaticIp();
  Serial.print(F("[WiFi] Verbinde mit: "));
  Serial.println(WIFI_SSID);

  WiFi.hostname(OTA_HOSTNAME);
  connectWiFi(20000);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[WiFi] OK, IP: "));
    Serial.println(WiFi.localIP());

    MDNS.begin(OTA_HOSTNAME);

    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.onStart([]()  { ESP.wdtFeed(); });
    ArduinoOTA.onEnd([]()    { DBG.println(F("[OTA] Fertig")); });
    ArduinoOTA.onError([](ota_error_t e) {
      DBG.print(F("[OTA] Fehler ")); DBG.println(e);
    });
    ArduinoOTA.begin();

    TelnetStream.begin(23);

    DBG.print(F("[ESP] IP: "));
    DBG.println(WiFi.localIP());
    DBG.print(F("[OTA] Bereit: "));
    DBG.println(OTA_HOSTNAME);
  } else {
    Serial.println(F("[WiFi] FEHLGESCHLAGEN"));
  }

  sendLED(false, true, false);  // Gelb = bereit
  DBG.println(F("[ESP] Bereit"));
}

// ─────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────

void loop() {
  ESP.wdtFeed();
  ArduinoOTA.handle();
  MDNS.update();
  wifiReconnect();
  processSerial();

  if (currentState == RESET && millis() - resetEnteredAt >= RESET_DISPLAY_MS) {
    currentState = STANDBY;
    sendLED(false, true, false);
  }
}

// ─────────────────────────────────────────────────────────
// Eingehende ATmega-Events parsen (nicht-blockierend)
// ─────────────────────────────────────────────────────────

void processSerial() {
  static char buf[40];
  static uint8_t pos = 0;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      buf[pos] = '\0';
      pos = 0;

      if (strncmp(buf, "RFID:", 5) == 0 && currentState == STANDBY) {
        handleRFID(buf + 5);

      } else if (strcmp(buf, "BTN:CLOSE") == 0 && currentState == STANDBY) {
        DBG.println(F("[ESP] Schließen"));
        sendLED(true, true, true);   // alle LEDs = Closing
        Serial.println(F("MOTOR:CLOSE"));
        waitMotorOK();
        retryCount = 0;
        lastUid[0] = '\0';
        sendLED(false, true, false);  // zurück zu Gelb
      }
      // REED-Events ignorieren wir vorerst (ATmega verwaltet das intern)

    } else if (c != '\r' && pos < sizeof(buf) - 1) {
      buf[pos++] = c;
    }
  }
}

// ─────────────────────────────────────────────────────────
// RFID-Event verarbeiten
// ─────────────────────────────────────────────────────────

void handleRFID(const char* uid) {
  DBG.print(F("[ESP] RFID: "));
  DBG.println(uid);

  if (strcmp(uid, lastUid) == 0) {
    retryCount++;
  } else {
    retryCount = 0;
  }

  currentState = IDENTIFICATION;
  sendLED(false, false, true);   // Grün = Identifikation läuft

  char result = checkServer(uid);

  if (result == 1) {
    DBG.println(F("[ESP] Zugang gewährt"));
    strncpy(lastUid, uid, UID_BUF_LEN - 1);
    lastUid[UID_BUF_LEN - 1] = '\0';

    if (retryCount >= 2) {
      Serial.println(F("MOTOR:FORCE_OPEN"));
    } else {
      Serial.println(F("MOTOR:OPEN"));
    }
    retryCount = 0;

    waitMotorOK();

    // Buzzer + Grün für 2s
    Serial.println(F("BUZZ:1"));
    sendLED(false, false, true);

    unsigned long t = millis();
    while (millis() - t < BUZZ_DURATION_MS) {
      ESP.wdtFeed();
      ArduinoOTA.handle();
      MDNS.update();
      delay(20);
    }

    Serial.println(F("BUZZ:0"));
    currentState = STANDBY;
    sendLED(false, true, false);

  } else {
    if (result == 0) {
      DBG.println(F("[ESP] Karte abgelehnt"));
      retryCount = 0;
      lastUid[0] = '\0';
    } else {
      DBG.println(F("[ESP] Server nicht erreichbar"));
    }
    resetEnteredAt = millis();
    currentState = RESET;
    sendLED(true, false, false);   // Rot = abgelehnt / Fehler
  }
}

// ─────────────────────────────────────────────────────────
// Warten auf MOTOR:OK vom ATmega (nicht länger als Timeout)
// ArduinoOTA und mDNS laufen weiter.
// ─────────────────────────────────────────────────────────

bool waitMotorOK() {
  static char buf[20];
  static uint8_t pos = 0;

  unsigned long deadline = millis() + MOTOR_WAIT_TIMEOUT_MS;

  while (millis() < deadline) {
    ESP.wdtFeed();
    ArduinoOTA.handle();
    MDNS.update();

    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        buf[pos] = '\0';
        pos = 0;
        if (strcmp(buf, "MOTOR:OK") == 0)      return true;
        if (strcmp(buf, "MOTOR:TIMEOUT") == 0) return false;
      } else if (c != '\r' && pos < sizeof(buf) - 1) {
        buf[pos++] = c;
      }
    }
    delay(10);
  }
  return false;
}

// ─────────────────────────────────────────────────────────
// LED-Befehl an ATmega senden
// ─────────────────────────────────────────────────────────

void sendLED(bool r, bool y, bool g) {
  char cmd[9];
  snprintf(cmd, sizeof(cmd), "LED:%c%c%c", r ? '1' : '0', y ? '1' : '0', g ? '1' : '0');
  Serial.println(cmd);
}

// ─────────────────────────────────────────────────────────
// Server-Anfrage (HTTP GET)
// ─────────────────────────────────────────────────────────

char checkServer(const char* rfid) {
  if (WiFi.status() != WL_CONNECTED) return -1;

  WiFiClient client;
  IPAddress serverAddr;
  if (WiFi.hostByName(SERVER_IP, serverAddr) != 1) {
    DBG.println(F("[HTTP] DNS fehlgeschlagen"));
    return -1;
  }

  if (!client.connect(serverAddr, 80)) {
    DBG.println(F("[HTTP] Verbindung fehlgeschlagen"));
    return -1;
  }

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

  while (client.connected() && millis() - t < SERVER_CONNECT_TIMEOUT_MS) {
    ESP.wdtFeed();
    while (client.available()) {
      char c = client.read();
      t = millis();
      if (!inBody) {
        if (c == '\n') {
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

  DBG.print(F("[HTTP] Antwort: "));
  DBG.println(message);

  return strstr(message, "true") != nullptr ? 1 : 0;
}

// ─────────────────────────────────────────────────────────
// WiFi verbinden (blockierend mit Timeout)
// ─────────────────────────────────────────────────────────

bool connectWiFi(unsigned long timeoutMs) {
  WiFi.disconnect();
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    ESP.wdtFeed();
    delay(300);
  }
  return WiFi.status() == WL_CONNECTED;
}

// ─────────────────────────────────────────────────────────
// WiFi Reconnect (non-blocking, wird im Loop aufgerufen)
// ─────────────────────────────────────────────────────────

void wifiReconnect() {
  static unsigned long lastAttempt = 0;
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastAttempt < WIFI_RECONNECT_INTERVAL_MS) return;

  lastAttempt = millis();
  DBG.println(F("[WiFi] Reconnect..."));
  connectWiFi(5000);
  if (WiFi.status() == WL_CONNECTED) {
    DBG.print(F("[WiFi] Online: "));
    DBG.println(WiFi.localIP());
  }
}

// ─────────────────────────────────────────────────────────
// Statische IP konfigurieren (falls in Build-Flags gesetzt)
// ─────────────────────────────────────────────────────────

bool configureStaticIp() {
  IPAddress localIp, gateway, subnet;
  if (!localIp.fromString(WIFI_LOCAL_IP) ||
      !gateway.fromString(WIFI_GATEWAY)  ||
      !subnet.fromString(WIFI_SUBNET))   {
    DBG.println(F("[WiFi] DHCP"));
    return false;
  }
  WiFi.config(localIp, gateway, subnet);
  DBG.print(F("[WiFi] Statisch: "));
  DBG.println(localIp);
  return true;
}
