#include <Arduino.h>
#include "settings.h"

#ifndef WIFI_SSID
  #define WIFI_SSID "NotSet"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD "NotSet"
#endif
#ifndef SERVER_HOST
  #define SERVER_HOST "localhost"
#endif
#ifndef AUTHENTICATION_TOKEN
  #define AUTHENTICATION_TOKEN "NoToken"
#endif

#include <SPI.h>
#include <Adafruit_PN532.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoLog.h>
#ifdef OTA_ENABLED
  #include <ArduinoOTA.h>
#endif

void next_State();
void setLED_ryg(bool led_red, bool led_yellow, bool led_green);
void connectToWiFi();
void checkWiFiConnection();
void initRFID();
bool perform_auth_check();
int tryLoginID(String uid);
String readID();

//------------------------------------------------------------------------------
// State Definitions
//------------------------------------------------------------------------------

enum State {
  STANDBY,
  IDENTIFICATION,
  RUNNING,
  RESET
};

State currentState = STANDBY;
State nextState = STANDBY;

//------------------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------------------

Adafruit_PN532 nfc(PN532_SS, &SPI);

String loggedInID = "0";
String uid = "";
bool isHttpRequestInProgress = false;

//------------------------------------------------------------------------------
// Setup
//------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  Log.notice("Starting setup ...\n");

  pinMode(MACHINE_RELAY_PIN, OUTPUT);
  pinMode(LED_RED_PIN,    OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN,  OUTPUT);
  pinMode(BUTTON_STOP, INPUT_PULLUP);

  digitalWrite(MACHINE_RELAY_PIN, LOW);
  setLED_ryg(1, 1, 1);

  delay(1000);

  connectToWiFi();

#ifdef OTA_ENABLED
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();
  Log.notice("[OTA] Ready. Hostname: %s\n", OTA_HOSTNAME);
#endif

  initRFID();

  setLED_ryg(0, 0, 0);
  delay(100);
  setLED_ryg(0, 0, 1);
  delay(100);
  setLED_ryg(0, 0, 0);

  Log.notice("Setup complete.\n");
}

//------------------------------------------------------------------------------
// Loop
//------------------------------------------------------------------------------

void loop() {
  checkWiFiConnection();
#ifdef OTA_ENABLED
  ArduinoOTA.handle();
#endif

  switch (currentState) {
    case STANDBY:        digitalWrite(MACHINE_RELAY_PIN, LOW);  setLED_ryg(0, 1, 0); break;
    case IDENTIFICATION: digitalWrite(MACHINE_RELAY_PIN, LOW);  setLED_ryg(0, 1, 1); break;
    case RUNNING:        digitalWrite(MACHINE_RELAY_PIN, HIGH); setLED_ryg(0, 0, 1); break;
    case RESET:          digitalWrite(MACHINE_RELAY_PIN, LOW);  setLED_ryg(1, 0, 0); break;
  }

  next_State();
}

//------------------------------------------------------------------------------
// State Machine
//------------------------------------------------------------------------------

void next_State() {
  static unsigned long lastCardPollMs  = 0;
  static int           cardAbsentCount = 0;
  static unsigned long resetEnteredMs  = 0;

  switch (currentState) {

    case STANDBY:
      if (millis() - lastCardPollMs >= CARD_POLL_MS) {
        lastCardPollMs = millis();
        uid = readID();
        if (!uid.equals("0")) {
          nextState = IDENTIFICATION;
        }
      }
      break;

    case IDENTIFICATION:
      if (perform_auth_check()) {
        cardAbsentCount = 0;
        nextState = RUNNING;
        delay(500);
      } else {
        Log.verbose("[next_State] Identification not successful.\n");
        nextState = RESET;
        resetEnteredMs = millis();
      }
      break;

    case RUNNING:
      if (digitalRead(BUTTON_STOP) == BUTTON_PRESSED) {
        nextState = RESET;
        resetEnteredMs = millis();
        break;
      }
      if (RFIDCARD_AUTH_CONST && millis() - lastCardPollMs >= CARD_POLL_MS) {
        lastCardPollMs = millis();
        if (readID().equals(loggedInID)) {
          cardAbsentCount = 0;
        } else if (++cardAbsentCount >= CARD_ABSENT_RESET) {
          Log.verbose("[next_State] Card absent %d times. Resetting.\n", cardAbsentCount);
          nextState = RESET;
          resetEnteredMs = millis();
        }
      }
      break;

    case RESET:
      uid = "0";
      loggedInID = "0";
      if (millis() - resetEnteredMs >= RESET_DISPLAY_MS) {
        nextState = STANDBY;
      }
      break;
  }
  currentState = nextState;
}

//------------------------------------------------------------------------------
// Hardware Helpers
//------------------------------------------------------------------------------

void setLED_ryg(bool led_red, bool led_yellow, bool led_green) {
  digitalWrite(LED_RED_PIN,    led_red);
  digitalWrite(LED_YELLOW_PIN, led_yellow);
  digitalWrite(LED_GREEN_PIN,  led_green);
}

void connectToWiFi() {
  Log.notice("[WiFi] Connecting to %s ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {
    delay(1000);
    retries++;
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Log.notice("\n[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
}

void initRFID() {
  SPI.begin(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Log.error("[initRFID] PN532 not found! Check wiring.\n");
    delay(2000);
    ESP.restart();
  }
  Log.notice("[initRFID] PN532 found. Firmware: %d.%d\n",
    (versiondata >> 16) & 0xFF,
    (versiondata >>  8) & 0xFF);

  nfc.SAMConfig();
}

bool perform_auth_check() {
  // uid is already set by STANDBY polling
  if (uid.equals("0")) {
    Log.warning("[auth] No card UID available.\n");
    return false;
  }
  Log.notice("[auth] Card UID: %s\n", uid.c_str());
  int success = tryLoginID(uid);
  if (success == 1) {
    loggedInID = uid;
  }
  return success == 1;
}

String readID() {
  uint8_t uidBytes[7];
  uint8_t uidLength;

  // Single attempt with short timeout to keep the loop responsive
  bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uidBytes, &uidLength, 200);
  if (!found) return "0";

  String id = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uidBytes[i] < 16) id += "0";
    id += String(uidBytes[i], HEX);
    if (i < uidLength - 1) id += ":";
  }
  Log.verbose("[readID] UID: %s\n", id.c_str());
  return id;
}

int tryLoginID(String uid) {
  if (isHttpRequestInProgress) {
    Log.warning("[tryLoginID] Skipped: request already in progress.\n");
    return -1;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Log.warning("[tryLoginID] Skipped: WiFi not connected.\n");
    return -1;
  }
  isHttpRequestInProgress = true;

  HTTPClient http;
  WiFiClient client;
  String url = "http://" + String(SERVER_HOST) + "/machine_try_login/" + AUTHENTICATION_TOKEN + "/" + MACHINE_NAME + "/" + MACHINE_ID + "/" + uid;
  Log.verbose("[tryLoginID] GET %s\n", url.c_str());

  http.begin(client, url);
  int httpCode = http.GET();
  int success = 0;

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Log.verbose("[tryLoginID] Payload: %s\n", payload.c_str());
    if (payload.indexOf("true") >= 0) {
      Log.notice("[tryLoginID] Login successful.\n");
      success = 1;
    } else {
      Log.warning("[tryLoginID] Login denied.\n");
    }
  } else {
    Log.error("[tryLoginID] HTTP error: %d\n", httpCode);
  }

  http.end();
  isHttpRequestInProgress = false;
  return success;
}

