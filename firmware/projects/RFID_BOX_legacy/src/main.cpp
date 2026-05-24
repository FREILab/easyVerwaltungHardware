/**
 * @file main.cpp
 * @brief ESP32 RFID Login System (PoC - PlatformIO Migration)
 *
 * Diese Datei initialisiert einen ESP32 für RFID-basierte Login-Operationen.
 * Sie verwaltet die WiFi-Verbindung, initialisiert den MFRC522 Reader, liest
 * RFID-Karten und kommuniziert per HTTP GET mit dem Backend.
 *
 * Kontext:
 * - Migriert von Arduino IDE zu PlatformIO.
 * - Konfiguration und Geheimnisse werden aus "secret.h" geladen.
 * - Teil der neuen Architektur (Proof of Concept).
 * - CI/CD Integration via GitHub Actions für automatisierte Builds.
 * - Tags vorbereitet für zukünftige Releases
 *
 * Hardware: ESP32 Dev1
 * Bibliotheken: MFRC522, ArduinoLog, HTTPClient, WiFi.
 */

#include <Arduino.h>
#include "settings.h" // Lokale Maschinen-Konfiguration (ID, Name, Auth-Logik)

// --- GEHEIMNISSE (Secrets) ---
// 1. Lokale secret.h einbinden, falls vorhanden (für lokales Kompilieren)
#if __has_include("secret.h")
  #include "secret.h"
#endif

// 2. Fallbacks für GitHub-Build-Flags
// Diese Makros werden auf GitHub direkt vom Compiler injiziert.
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

// Hier folgen nun deine weiteren Includes
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoLog.h>
#ifdef OTA_ENABLED
  #include <ArduinoOTA.h>
#endif

// --- Prototypen (Forward Declarations für C++) ---
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

/**
 * @enum State
 * @brief Defines the different states of the RFID login system.
 */
enum State {
  STANDBY,         ///< System is idle, waiting for RFID input.
  IDENTIFICATION,  ///< System is verifying RFID credentials.
  RUNNING,         ///< System is active, machine is running.
  RESET            ///< System is resetting to standby state.
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

#define MACHINE_RELAY_PIN 22  ///< Pin controlling the machine relay
#define BUTTON_RFID 4         ///< Button to stop machine / Taster 1
#define BUTTON_STOP 13        ///< Button to start machine / Taster 2

#define RFID_RST_PIN 5  ///< Reset pin for the RFID module
#define RFID_SS_PIN 21  ///< SDA pin for the RFID module

#define LED_RED_PIN 32     ///< Pin for red LED
#define LED_YELLOW_PIN 33  ///< Pin for yellow LED
#define LED_GREEN_PIN 26   ///< Pin for green LED

#define BUTTON_PRESSED 0   ///< Buttons active low

//------------------------------------------------------------------------------
// Global Variables and Instances
//------------------------------------------------------------------------------

const int TIME_GLITCH_FILTER_STOP = 100;  ///< 0.1s button debounce time
const int TIME_GLITCH_FILTER_RFID = 3000; ///< 3s button debounce time

MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);  ///< Instance of the RFID module

String loggedInID = "0";     ///< Currently logged-in RFID card ID
String uid = "";             ///< UID read from an RFID card
bool isHttpRequestInProgress = false; ///< Flag to indicate an ongoing HTTP request

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
} teeStream;

//------------------------------------------------------------------------------
// Setup Function
//------------------------------------------------------------------------------

/**
 * @brief Initializes hardware, connects to WiFi, and sets up the RFID module.
 */
void setup() {
  Serial.begin(115200);
  Log.begin(LOG_LEVEL_VERBOSE, &teeStream);

  Log.notice("Starting setup ...\n");

  // Relay control output
  pinMode(MACHINE_RELAY_PIN, OUTPUT);

  // LEDs pins
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);

  // RFID card detection button
  pinMode(BUTTON_RFID, INPUT_PULLUP);
  // Logout button
  pinMode(BUTTON_STOP, INPUT_PULLUP);

  // set initial states
  digitalWrite(MACHINE_RELAY_PIN, LOW);  // Ensure machine is off initially
  setLED_ryg(1, 1, 1); // all LEDs on

  delay(1000);  // wait for pullups to get active

  connectToWiFi();
  telnetServer.begin();
  Log.notice("[Telnet] Server started on port 23\n");

#ifdef OTA_ENABLED
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();
  Log.notice("[OTA] Ready. Hostname: %s\n", OTA_HOSTNAME);
#endif

  initRFID();

  // indicate successful startup
  setLED_ryg(0, 0, 0); 
  delay(100);
  setLED_ryg(0, 0, 1);  // green flicker indicator
  delay(100);
  setLED_ryg(0, 0, 0);

  Log.notice("Setup complete.\n");
}

/**
 * @brief Main Loop
 */
void loop() {
  checkWiFiConnection();
  if (telnetServer.hasClient()) {
    if (telnetClient) telnetClient.stop();
    telnetClient = telnetServer.available();
    Log.notice("[Telnet] Client connected.\n");
  }
#ifdef OTA_ENABLED
  ArduinoOTA.handle();
#endif

  // Visual feedback based on state
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

/**
 * @brief Handles state transitions based on system inputs.
 */
void next_State() {
  // Glitch filter variables
  static unsigned long rfidButtonPressTime = 0;
  static bool rfidButtonTimerActive = false;
  static unsigned long stopButtonPressTime = 0;
  static bool stopButtonTimerActive = false;

  switch (currentState) {
    case STANDBY:
      if (digitalRead(BUTTON_RFID) == BUTTON_PRESSED) {
        // when the RFID card is entered, proceed with identification
        nextState = IDENTIFICATION;
      }
      break;

    case IDENTIFICATION:
      if (perform_auth_check()) {
        // when auth check was successful, start machine
        continuousServerCheckFailCount = 0;
        lastContinuousServerCheckMs = millis();
        nextState = RUNNING;
        delay(500); 
      } else {
        // when auth check was not successful, return to reset state
        Log.verbose("[next_State] Identification not successful.\n");
        nextState = RESET;
      }
      break;

    case RUNNING:
      // Differentiate if the machine needs the RFID card connected constantly
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

        // The card has to be connected constantly
        if (digitalRead(BUTTON_STOP) == BUTTON_PRESSED) {
          if (!stopButtonTimerActive) { stopButtonPressTime = millis(); stopButtonTimerActive = true; }
          if (millis() - stopButtonPressTime >= TIME_GLITCH_FILTER_STOP) {
            Log.notice("[next_State] Stop button pressed. Switching to RESET.\n");
            nextState = RESET;
          }
        } else { stopButtonTimerActive = false; }

        if (digitalRead(BUTTON_RFID) != BUTTON_PRESSED) {
          if (!rfidButtonTimerActive) {
            rfidButtonPressTime = millis();
            rfidButtonTimerActive = true;
            Log.notice("[next_State] RFID card removed. Waiting %ds before shutdown.\n", TIME_GLITCH_FILTER_RFID / 1000);
          }
          if (millis() - rfidButtonPressTime >= TIME_GLITCH_FILTER_RFID) {
            Log.notice("[next_State] RFID card timeout. Switching to RESET.\n");
            nextState = RESET;
          }
        } else { rfidButtonTimerActive = false; }
      } else {
        // Only a single sign-on is necessary
        if (digitalRead(BUTTON_STOP) == BUTTON_PRESSED) {
          Log.notice("[next_State] Stop button pressed. Switching to RESET.\n");
          nextState = RESET;
        }
      }
      break;

    case RESET:
      continuousServerCheckFailCount = 0;
      if ((digitalRead(BUTTON_RFID) != BUTTON_PRESSED) && (digitalRead(BUTTON_STOP) != BUTTON_PRESSED)) {
        // when both buttons are inactive, change to standby state
        nextState = STANDBY;
      }
      break;
  }
  currentState = nextState;
}

/**
 * @brief Set external LEDs
 */
void setLED_ryg(bool led_red, bool led_yellow, bool led_green) {
  digitalWrite(LED_RED_PIN, led_red);
  digitalWrite(LED_YELLOW_PIN, led_yellow);
  digitalWrite(LED_GREEN_PIN, led_green);
}

/**
 * @brief Connects the ESP32 to the WiFi network.
 */
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

/**
 * @brief Checks WiFi network connection.
 */
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
}

/**
 * @brief Initializes the RFID module and verifies communication.
 */
void initRFID() {
  SPI.begin();
  mfrc522.PCD_Init();
  byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Log.error("[initRFID] RFID module not responding! Check wiring.\n");
    delay(2000);
    ESP.restart();
  } else {
    Log.notice("[initRFID] Reader initialized. Firmware: 0x%02X\n", version);
  }
}

/**
 * @brief Handles login authentication
 */
bool perform_auth_check() {
  uid = readID();
  if (uid.equals("0")) {
    Log.warning("[auth] No card readable (readID returned 0).\n");
    return false;
  }
  Log.notice("[auth] Card UID: %s\n", uid.c_str());
  int success = tryLoginID(uid);
  if (success == 1) {
    loggedInID = uid;
  }
  return success == 1;
}

/**
 * @brief Subtle green LED blink pattern on successful continuous auth check.
 */
void blinkGreenSubtleSuccess() {
  digitalWrite(LED_GREEN_PIN, LOW);
  delay(25);
  digitalWrite(LED_GREEN_PIN, HIGH);
  delay(25);
  digitalWrite(LED_GREEN_PIN, LOW);
  delay(25);
  digitalWrite(LED_GREEN_PIN, HIGH);
}

/**
 * @brief Short yellow LED blink while green stays on.
 */
void blinkYellowShortWarning() {
  digitalWrite(LED_GREEN_PIN, HIGH);
  digitalWrite(LED_YELLOW_PIN, HIGH);
  delay(60);
  digitalWrite(LED_YELLOW_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, HIGH);
}

/**
 * @brief Attempts to log in using the provided RFID card UID.
 */
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
      Log.warning("[tryLoginID] Login denied (payload does not contain 'true').\n");
    }
  } else {
    Log.error("[tryLoginID] HTTP error: %d\n", httpCode);
  }

  http.end();
  isHttpRequestInProgress = false;
  return success;
}

/**
 * @brief Reads the RFID card UID.
 * @return A String representing the UID or "0" if no card is found.
 */
String readID() {
  for (int attempt = 0; attempt < 3; attempt++) {
    bool isPresent = mfrc522.PICC_IsNewCardPresent();
    Log.verbose("[readID] Attempt %d: PICC_IsNewCardPresent=%d\n", attempt + 1, isPresent);
    if (isPresent) {
      bool readOk = mfrc522.PICC_ReadCardSerial();
      Log.verbose("[readID] Attempt %d: PICC_ReadCardSerial=%d\n", attempt + 1, readOk);
      if (readOk) {
        String id = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
          id += (mfrc522.uid.uidByte[i] < 16 ? "0" : "") + String(mfrc522.uid.uidByte[i], 16);
          if (i < mfrc522.uid.size - 1) id += ":";
        }
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        return id;
      }
    }
    delay(500);
  }
  Log.warning("[readID] All attempts failed.\n");
  return "0";
}