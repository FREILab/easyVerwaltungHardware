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
void blinkGreenSubtleSuccess();
void blinkYellowShortWarning();

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
bool auth_check = true;
unsigned long stateChangeTime = 0;
unsigned long lastContinuousServerCheckMs = 0;
int continuousServerCheckFailCount = 0;

//------------------------------------------------------------------------------
// Pin Definitions
//------------------------------------------------------------------------------

#define MACHINE_RELAY_PIN 22
#define BUTTON_RFID 4
#define BUTTON_STOP 13

#define LED_RED_PIN    32
#define LED_YELLOW_PIN 33
#define LED_GREEN_PIN  26

#define BUTTON_PRESSED 0

//------------------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------------------

const int TIME_GLITCH_FILTER_STOP = 100;
const int TIME_GLITCH_FILTER_RFID = 3000;

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
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(BUTTON_RFID, INPUT_PULLUP);
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
    case STANDBY:
      digitalWrite(MACHINE_RELAY_PIN, LOW);
      setLED_ryg(0, 1, 0);
      break;
    case IDENTIFICATION:
      digitalWrite(MACHINE_RELAY_PIN, LOW);
      setLED_ryg(0, 1, 1);
      break;
    case RUNNING:
      digitalWrite(MACHINE_RELAY_PIN, HIGH);
      setLED_ryg(0, 0, 1);
      break;
    case RESET:
      digitalWrite(MACHINE_RELAY_PIN, LOW);
      setLED_ryg(1, 0, 0);
      break;
  }

  next_State();
  delay(100);
}

//------------------------------------------------------------------------------
// State Machine
//------------------------------------------------------------------------------

void next_State() {
  static unsigned long rfidButtonPressTime = 0;
  static bool rfidButtonTimerActive = false;
  static unsigned long stopButtonPressTime = 0;
  static bool stopButtonTimerActive = false;

  switch (currentState) {
    case STANDBY:
      if (digitalRead(BUTTON_RFID) == BUTTON_PRESSED) {
        nextState = IDENTIFICATION;
      }
      break;

    case IDENTIFICATION:
      if (perform_auth_check()) {
        continuousServerCheckFailCount = 0;
        lastContinuousServerCheckMs = millis();
        nextState = RUNNING;
        delay(500);
      } else {
        Log.verbose("[next_State] Identification not successful.\n");
        nextState = RESET;
      }
      break;

    case RUNNING:
      if (RFIDCARD_AUTH_CONST) {
        if (CONTINUOUS_SERVER_CHECK && (millis() - lastContinuousServerCheckMs >= 2000)) {
          lastContinuousServerCheckMs = millis();

          int checkResult = tryLoginID(loggedInID);
          if (checkResult == 1) {
            continuousServerCheckFailCount = 0;
            blinkGreenSubtleSuccess();
          } else {
            continuousServerCheckFailCount++;
            blinkYellowShortWarning();
            Log.warning("[continuous-auth] Server check failed (%d/40).\n", continuousServerCheckFailCount);

            if (continuousServerCheckFailCount >= 40) {
              Log.error("[continuous-auth] 40 failed checks reached. Turning relay off.\n");
              digitalWrite(MACHINE_RELAY_PIN, LOW);
              nextState = RESET;
            }
          }
        }

        if (digitalRead(BUTTON_STOP) == BUTTON_PRESSED) {
          if (!stopButtonTimerActive) { stopButtonPressTime = millis(); stopButtonTimerActive = true; }
          if (millis() - stopButtonPressTime >= TIME_GLITCH_FILTER_STOP) nextState = RESET;
        } else { stopButtonTimerActive = false; }

        if (digitalRead(BUTTON_RFID) != BUTTON_PRESSED) {
          if (!rfidButtonTimerActive) { rfidButtonPressTime = millis(); rfidButtonTimerActive = true; }
          if (millis() - rfidButtonPressTime >= TIME_GLITCH_FILTER_RFID) {
            Log.verbose("[next_State] RFID Card pulled.\n");
            nextState = RESET;
          }
        } else { rfidButtonTimerActive = false; }
      } else {
        if (digitalRead(BUTTON_STOP) == BUTTON_PRESSED) nextState = RESET;
      }
      break;

    case RESET:
      continuousServerCheckFailCount = 0;
      if ((digitalRead(BUTTON_RFID) != BUTTON_PRESSED) && (digitalRead(BUTTON_STOP) != BUTTON_PRESSED)) {
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
  digitalWrite(LED_RED_PIN, led_red);
  digitalWrite(LED_YELLOW_PIN, led_yellow);
  digitalWrite(LED_GREEN_PIN, led_green);
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
  uid = readID();
  if (uid.equals("0")) {
    Log.warning("[auth] No card readable.\n");
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

  for (int attempt = 0; attempt < 3; attempt++) {
    bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uidBytes, &uidLength, 500);
    Log.verbose("[readID] Attempt %d: found=%d\n", attempt + 1, found);
    if (found) {
      String id = "";
      for (uint8_t i = 0; i < uidLength; i++) {
        if (uidBytes[i] < 16) id += "0";
        id += String(uidBytes[i], HEX);
        if (i < uidLength - 1) id += ":";
      }
      return id;
    }
  }
  Log.warning("[readID] All attempts failed.\n");
  return "0";
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
  Log.verbose("[tryLoginID] HTTP response code: %d\n", httpCode);
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

void blinkGreenSubtleSuccess() {
  digitalWrite(LED_GREEN_PIN, LOW);  delay(25);
  digitalWrite(LED_GREEN_PIN, HIGH); delay(25);
  digitalWrite(LED_GREEN_PIN, LOW);  delay(25);
  digitalWrite(LED_GREEN_PIN, HIGH);
}

void blinkYellowShortWarning() {
  digitalWrite(LED_GREEN_PIN, HIGH);
  digitalWrite(LED_YELLOW_PIN, HIGH); delay(60);
  digitalWrite(LED_YELLOW_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, HIGH);
}
