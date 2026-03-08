/**
 * @file main.cpp
 * @brief ESP32 RFID Machine Access Control System
 * * @details This firmware implements an RFID-based authentication system for industrial 
 * machinery. It manages WiFi connectivity, communicates with a central backend via 
 * HTTP GET requests, and controls machine power through a relay.
 * * Context:
 * - Migrated to PlatformIO for professional dependency management.
 * - Supports local builds (via secret.h) and CI/CD (via GitHub Actions environment variables).
 * - Implements a state machine for robust operation and safety interlocks.
 * * Hardware: ESP32 DevKit V1
 * Peripherals: MFRC522 (SPI), Status LEDs, Mechanical Buttons (Simulated Card Slot).
 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoLog.h>
#include "settings.h" // Local machine configuration (ID, Name, Auth-Logic)

// --- CONFIGURATION & SECRETS ---

/**
 * @details Inclusion of sensitive credentials. 
 * Locally, these are stored in secret.h. In GitHub Actions, 
 * these are injected as build flags (-D WIFI_SSID=...).
 */
#if __has_include("secret.h")
  #include "secret.h"
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID "NotSet"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD "NotSet"
#endif
#ifndef SERVER_IP
  #define SERVER_IP "0.0.0.0"
#endif
#ifndef AUTHENTICATION_TOKEN
  #define AUTHENTICATION_TOKEN "NoToken"
#endif

// --- FUNCTION PROTOTYPES ---

void next_State();
void setLED_ryg(bool red, bool yellow, bool green);
void connectToWiFi();
void checkWiFiConnection();
void initRFID();
bool perform_auth_check();
int tryLoginID(String cardUid);
String readID();

// --- STATE MACHINE DEFINITIONS ---

/**
 * @enum State
 * @brief Logic states for machine operation.
 */
enum State {
  STANDBY,         ///< System is idle; machine is OFF; waiting for card insertion.
  IDENTIFICATION,  ///< Card detected; communicating with backend for authorization.
  RUNNING,         ///< Authorized; machine relay is ON.
  RESET            ///< Error or manual logout; system clears session before returning to STANDBY.
};

State currentState = STANDBY;
State nextState = STANDBY;

// --- PIN DEFINITIONS ---

/** @name Power & Control Pins */
///@{
#define MACHINE_RELAY_PIN 22  ///< GPIO for the machine's power relay (Active High).
#define BUTTON_RFID 4         ///< End-stop switch inside the card slot (Detects physical presence).
#define BUTTON_STOP 13        ///< Manual logout/Stop button.
///@}

/** @name RFID SPI Pins */
///@{
#define RFID_RST_PIN 5        ///< Reset pin for MFRC522.
#define RFID_SS_PIN 21        ///< Slave Select (SDA) for MFRC522.
///@}

/** @name Visual Interface Pins */
///@{
#define LED_RED_PIN 32        ///< Status LED: Error / Reset.
#define LED_YELLOW_PIN 33     ///< Status LED: Standby / Processing.
#define LED_GREEN_PIN 26       ///< Status LED: Authorized / Running.
///@}

#define BUTTON_PRESSED 0      ///< Buttons are configured as INPUT_PULLUP (Active Low).

// --- GLOBAL VARIABLES ---

const int TIME_GLITCH_FILTER_STOP = 100;  ///< Debounce time (ms) for the stop button.
const int TIME_GLITCH_FILTER_RFID = 3000; ///< Tolerance (ms) for card removal to prevent accidental stops.

MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN); ///< RFID Reader instance.

String uid = "";              ///< Stores the current card's UID string.
bool isHttpRequestInProgress = false; ///< Prevents overlapping network requests.

// --- MAIN SETUP ---

/**
 * @brief Hardware and Communication Setup.
 * @details Initializes Serial logging, GPIOs, WiFi, and the SPI RFID module.
 * Visual feedback is provided during the startup sequence.
 */
void setup() {
  Serial.begin(115200);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  Log.notice("Initializing Node ...\n");

  pinMode(MACHINE_RELAY_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);

  pinMode(BUTTON_RFID, INPUT_PULLUP);
  pinMode(BUTTON_STOP, INPUT_PULLUP);

  digitalWrite(MACHINE_RELAY_PIN, LOW); // Safe state: Machine OFF
  setLED_ryg(1, 1, 1);                  // Indicator: Booting

  delay(1000); // Allow pull-ups to stabilize

  connectToWiFi();
  initRFID();

  setLED_ryg(0, 0, 0); 
  delay(100);
  setLED_ryg(0, 0, 1); // Success blink
  delay(100);
  setLED_ryg(0, 0, 0);

  Log.notice("Node Setup Ready.\n");
}

// --- MAIN LOOP ---

/**
 * @brief Standard Arduino Loop.
 * @details Manages WiFi persistence, updates visual status based on current state, 
 * and triggers the state transition logic.
 */
void loop() {
  checkWiFiConnection();

  // Visual status mapping
  switch (currentState) {
    case STANDBY:
      digitalWrite(MACHINE_RELAY_PIN, LOW);
      setLED_ryg(0, 1, 0); // Yellow: Idle
      break;
    case IDENTIFICATION:
      setLED_ryg(0, 1, 1); // Yellow/Green: Processing
      break;
    case RUNNING:
      digitalWrite(MACHINE_RELAY_PIN, HIGH);
      setLED_ryg(0, 0, 1); // Green: Active
      break;
    case RESET:
      digitalWrite(MACHINE_RELAY_PIN, LOW);
      setLED_ryg(1, 0, 0); // Red: Access Denied / Ending Session
      break;
  }

  next_State();
  delay(100); // Main loop frequency (~10Hz)
}

// --- LOGIC IMPLEMENTATION ---

/**
 * @brief Handles State Transitions.
 * @details Implements the core business logic, including:
 * 1. Authentication trigger via physical card detection.
 * 2. Backend validation.
 * 3. Constant presence detection (optional per machine type).
 * 4. Safety-focused reset conditions.
 */
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
        Log.notice("[State] Auth success. Machine starting.\n");
        nextState = RUNNING;
        delay(500); 
      } else {
        Log.warning("[State] Auth failed. Resetting.\n");
        nextState = RESET;
      }
      break;

    case RUNNING:
      // Safety Logic: Constant Presence Detection (Dead-Man Switch)
      if (RFIDCARD_AUTH_CONST) {
        // Handle physical Logout Button
        if (digitalRead(BUTTON_STOP) == BUTTON_PRESSED) {
          if (!stopButtonTimerActive) { stopButtonPressTime = millis(); stopButtonTimerActive = true; }
          if (millis() - stopButtonPressTime >= TIME_GLITCH_FILTER_STOP) nextState = RESET;
        } else { stopButtonTimerActive = false; }

        // Handle Card Removal (End-stop released)
        if (digitalRead(BUTTON_RFID) != BUTTON_PRESSED) {
          if (!rfidButtonTimerActive) { rfidButtonPressTime = millis(); rfidButtonTimerActive = true; }
          if (millis() - rfidButtonPressTime >= TIME_GLITCH_FILTER_RFID) {
            Log.verbose("[State] Safety Interlock: Card removed.\n");
            nextState = RESET;
          }
        } else { rfidButtonTimerActive = false; }
      } 
      else {
        // Single Sign-On logic: Card is only needed for the start trigger
        if (digitalRead(BUTTON_STOP) == BUTTON_PRESSED) nextState = RESET;
      }
      break;

    case RESET:
      // Ensure both inputs are released before allowing a new session
      if ((digitalRead(BUTTON_RFID) != BUTTON_PRESSED) && (digitalRead(BUTTON_STOP) != BUTTON_PRESSED)) {
        nextState = STANDBY;
      }
      break;
  }
  currentState = nextState;
}

/**
 * @brief Updates external RGB or Status LEDs.
 * @param red State of red LED.
 * @param yellow State of yellow LED.
 * @param green State of green LED.
 */
void setLED_ryg(bool red, bool yellow, bool green) {
  digitalWrite(LED_RED_PIN, red);
  digitalWrite(LED_YELLOW_PIN, yellow);
  digitalWrite(LED_GREEN_PIN, green);
}

/**
 * @brief Establishes WiFi Connection.
 * @details Retries 10 times before moving on. Lite OS standard for background handling.
 */
void connectToWiFi() {
  Log.notice("[WiFi] Connecting to: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {
    delay(1000);
    Serial.print(".");
    retries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Log.notice("\n[WiFi] Connection established. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Log.error("\n[WiFi] Connection failed.\n");
  }
}

/**
 * @brief Ensures WiFi connectivity remains active.
 */
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
}

/**
 * @brief Initializes MFRC522 Hardware.
 * @details Performs a register read to verify communication. Restarts ESP if hardware is missing.
 */
void initRFID() {
  SPI.begin();
  mfrc522.PCD_Init();
  byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Log.error("[RFID] Hardware fault: Check wiring!\n");
    delay(2000);
    ESP.restart();
  } else {
    Log.notice("[RFID] Reader initialized. HW Version: 0x%02X\n", version);
  }
}

/**
 * @brief High-level Authorization Wrapper.
 * @return true if card is present AND backend authorized access.
 */
bool perform_auth_check() {
  uid = readID();
  if (uid.equals("0")) return false;
  Log.notice("[Auth] Identified UID: %s\n", uid.c_str());
  return tryLoginID(uid);
}

/**
 * @brief Communicates with the Backend API.
 * @details Executes a RESTful GET request to the authorization endpoint.
 * @param cardUid The hex string representation of the card UID.
 * @return 1 on success, 0 on denial, -1 on network error.
 */
int tryLoginID(String cardUid) {
  if (isHttpRequestInProgress || WiFi.status() != WL_CONNECTED) return -1;
  isHttpRequestInProgress = true;

  HTTPClient http;
  WiFiClient client;
  
  // Dynamic URL construction based on machine settings and UID
  String url = "http://" + String(SERVER_IP) + "/machine_try_login/" + 
               AUTHENTICATION_TOKEN + "/" + MACHINE_NAME + "/" + 
               MACHINE_ID + "/" + cardUid;

  Log.verbose("[HTTP] Querying: %s\n", url.c_str());
  http.begin(client, url);
  int httpCode = http.GET();
  int success = 0;

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    if (payload.indexOf("true") >= 0) {
      success = 1;
    }
  } else {
    Log.error("[HTTP] Error Code: %d\n", httpCode);
  }

  http.end();
  isHttpRequestInProgress = false;
  return success;
}

/**
 * @brief Reads the RFID Card UID.
 * @details Attempts multiple reads to handle physical misalignment.
 * @return Hexadecimal string of the card UID (e.g., "AF:04:E2:01") or "0" if no card.
 */
String readID() {
  for (int attempt = 0; attempt < 3; attempt++) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String id = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        id += (mfrc522.uid.uidByte[i] < 16 ? "0" : "") + String(mfrc522.uid.uidByte[i], 16);
        if (i < mfrc522.uid.size - 1) id += ":";
      }
      id.toUpperCase();
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      return id;
    }
    delay(500);
  }
  return "0";
}