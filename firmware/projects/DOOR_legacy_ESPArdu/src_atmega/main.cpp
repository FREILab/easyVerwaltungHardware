/**
 * ATmega328P — I/O-Expander für RobotDyn UNO+WiFi R3
 *
 * Verantwortung: Hardware-Steuerung (RFID, Motor, LEDs, Taster).
 * Die gesamte Logik (State Machine, WiFi, OTA) läuft auf dem ESP8266.
 * Kommunikation über UART 9600 Baud (zeilenbasiertes Protokoll).
 *
 * ATmega → ESP Events:
 *   RFID:aa:bb:cc:dd   RFID-Karte erkannt
 *   BTN:CLOSE          Schließ-Taster (Flanke)
 *   REED:1 / REED:0    Tür physisch offen / zu
 *   MOTOR:OK           Motor-Befehl abgeschlossen
 *   MOTOR:TIMEOUT      Motor hat Timeout erreicht
 *
 * ESP → ATmega Befehle:
 *   LED:RYG            LEDs setzen (je '0'/'1'), z.B. LED:010 = nur Gelb
 *   BUZZ:1 / BUZZ:0    Buzzer ein/aus
 *   MOTOR:OPEN         Öffnen (Force=false)
 *   MOTOR:FORCE_OPEN   Öffnen (Force=true, Retry-Fall)
 *   MOTOR:CLOSE        Schließen
 *
 * DIP für Flash: SW3+SW4 ON, Rest OFF.
 * DIP Normal-Betrieb (ATmega↔ESP): SW1+SW2 ON, Rest OFF.
 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <AccelStepper.h>
#include <avr/wdt.h>
#include "settings.h"

// Debug-Nachrichten an ESP senden (werden dort per busDbg() an Telnet weitergeleitet)
#define atmDbg(msg) Serial.println(F("ATMDBG:" msg))

// --- Pin-Definitionen (identisch zum Original) ---
#define PIN_STEP          2
#define PIN_DIR           3
#define RFID_RST_PIN      5
#define RFID_SS_PIN       6
#define PIN_SR_SER        7
#define PIN_SR_RCLK       8
#define PIN_SR_SRCLK      9
#define PIN_ENDSCHALTER   A0
#define PIN_REED          A1
#define PIN_ABSCHLIESSEN  A2

// --- Globale Instanzen ---
MFRC522      mfrc522(RFID_SS_PIN, RFID_RST_PIN);
AccelStepper stepper(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);

// --- Schieberegister-Zustand (aktiv LOW) ---
bool srRed         = false;
bool srYellow      = true;   // Gelb beim Start
bool srGreen       = false;
bool srSummer      = false;
bool srMotorEnable = false;

// --- Button/Reed Edge-Detection ---
bool lastBtnClose = HIGH;
bool lastReed     = HIGH;

static const uint8_t UID_BUF_LEN = 32;

// --- Forward Declarations ---
void updateSR();
bool readUID(char* buf, uint8_t bufSize);
void runOpenDoor(bool forceOpen);
void runCloseDoor();
void processIncoming();
void pollRFID();
void pollButtons();
void pollReed();

// ─────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────

void setup() {
  // MCUSR vor wdt_enable() sichern und direkt löschen (AVR-Pflicht)
  uint8_t mcusr = MCUSR;
  MCUSR = 0;

  pinMode(PIN_SR_SER,       OUTPUT);
  pinMode(PIN_SR_RCLK,      OUTPUT);
  pinMode(PIN_SR_SRCLK,     OUTPUT);
  pinMode(PIN_ENDSCHALTER,  INPUT_PULLUP);
  pinMode(PIN_REED,         INPUT_PULLUP);
  pinMode(PIN_ABSCHLIESSEN, INPUT_PULLUP);

  updateSR();  // Gelb an während Boot

  stepper.setMaxSpeed(STEPPER_SPEED);
  stepper.setAcceleration(STEPPER_ACCEL);

  SPI.begin();
  mfrc522.PCD_Init();

  Serial.begin(9600);

  // ESP8266 braucht ~2-3s zum Booten — warten damit keine Events verloren gehen
  delay(3000);

  // Boot-Grund aus MCUSR melden (wurde vor wdt_enable gesichert)
  const char* reason = (mcusr & (1 << WDRF))  ? "WDT"
                     : (mcusr & (1 << EXTRF)) ? "EXT"
                     : (mcusr & (1 << BORF))  ? "BOD"
                                              : "POR";
  Serial.print(F("ATMDBG:BOOT reason="));
  Serial.println(reason);

  srYellow = false;
  updateSR();

  wdt_enable(WDTO_4S);
}

// ─────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────

void loop() {
  wdt_reset();
  processIncoming();
  pollRFID();
  pollButtons();
  pollReed();
}

// ─────────────────────────────────────────────────────────
// Eingehende ESP-Befehle parsen (nicht-blockierend)
// ─────────────────────────────────────────────────────────

void processIncoming() {
  static char buf[32];
  static uint8_t pos = 0;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      buf[pos] = '\0';
      pos = 0;

      if (strcmp(buf, "PING") == 0) {
        Serial.println(F("PONG"));

      } else if (strncmp(buf, "LED:", 4) == 0 && strlen(buf) >= 7) {
        srRed    = buf[4] == '1';
        srYellow = buf[5] == '1';
        srGreen  = buf[6] == '1';
        updateSR();

      } else if (strcmp(buf, "BUZZ:1") == 0) {
        srSummer = true;
        updateSR();

      } else if (strcmp(buf, "BUZZ:0") == 0) {
        srSummer = false;
        updateSR();

      } else if (strcmp(buf, "MOTOR:OPEN") == 0) {
        runOpenDoor(false);

      } else if (strcmp(buf, "MOTOR:FORCE_OPEN") == 0) {
        runOpenDoor(true);

      } else if (strcmp(buf, "MOTOR:CLOSE") == 0) {
        runCloseDoor();
      }

    } else if (c != '\r' && pos < sizeof(buf) - 1) {
      buf[pos++] = c;
    }
  }
}

// ─────────────────────────────────────────────────────────
// RFID lesen und melden
// ─────────────────────────────────────────────────────────

void pollRFID() {
  static unsigned long lastHealthCheck = 0;
  if (millis() - lastHealthCheck >= 5000UL) {
    lastHealthCheck = millis();
    byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    if (v == 0x00 || v == 0xFF) {
      mfrc522.PCD_Init();
      atmDbg("RFID reinit dead");
    }
  }

  char uid[UID_BUF_LEN];
  if (!readUID(uid, sizeof(uid))) return;
  Serial.print(F("ATMDBG:RFID card="));
  Serial.println(uid);
  Serial.print(F("RFID:"));
  Serial.println(uid);
}

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
  mfrc522.PCD_Init();
  return true;
}

// ─────────────────────────────────────────────────────────
// Taster-Flanke erkennen
// ─────────────────────────────────────────────────────────

void pollButtons() {
  bool btn = digitalRead(PIN_ABSCHLIESSEN);
  if (lastBtnClose == HIGH && btn == LOW) {
    atmDbg("BTN:CLOSE");
    Serial.println(F("BTN:CLOSE"));
  }
  lastBtnClose = btn;
}

// ─────────────────────────────────────────────────────────
// Reed-Kontakt Flanke erkennen
// ─────────────────────────────────────────────────────────

void pollReed() {
  bool reed = digitalRead(PIN_REED);
  if (reed != lastReed) {
    Serial.print(F("ATMDBG:REED="));
    Serial.println(reed == LOW ? '1' : '0');
    Serial.print(F("REED:"));
    Serial.println(reed == LOW ? '1' : '0');
    lastReed = reed;
  }
}

// ─────────────────────────────────────────────────────────
// Motor: Tür öffnen (blockierend, WDT wird intern gefüttert)
// ─────────────────────────────────────────────────────────

void runOpenDoor(bool forceOpen) {
  srMotorEnable = true;
  updateSR();

  bool timedOut = false;

  if (digitalRead(PIN_ENDSCHALTER) == LOW || forceOpen) {
    stepper.setSpeed(STEPPER_SPEED);
    unsigned long t = millis();
    while (digitalRead(PIN_ENDSCHALTER) == LOW) {
      wdt_reset();
      stepper.runSpeed();
      if (millis() - t >= MOTOR_TIMEOUT_MS) { timedOut = true; break; }
    }
    if (!timedOut) {
      long pos = stepper.currentPosition();
      t = millis();
      while (stepper.currentPosition() - pos < STEPS_TO_OPEN) {
        wdt_reset();
        stepper.runSpeed();
        if (millis() - t >= MOTOR_TIMEOUT_MS) { timedOut = true; break; }
      }
    }
    stepper.setSpeed(0);
  }

  srMotorEnable = false;
  updateSR();

  Serial.println(timedOut ? F("MOTOR:TIMEOUT") : F("MOTOR:OK"));
}

// ─────────────────────────────────────────────────────────
// Motor: Tür schließen (blockierend, WDT wird intern gefüttert)
// ─────────────────────────────────────────────────────────

void runCloseDoor() {
  srMotorEnable = true;
  updateSR();

  bool timedOut = false;

  // Warten bis Tür physisch offen (Reed)
  unsigned long t = millis();
  while (digitalRead(PIN_REED) == HIGH) {
    wdt_reset();
    if (millis() - t >= DOOR_REED_TIMEOUT_MS) { timedOut = true; break; }
    delay(50);
  }

  if (!timedOut) {
    delay(2000);

    long posStart = stepper.currentPosition();
    stepper.setSpeed(-STEPPER_SPEED);
    t = millis();
    while (digitalRead(PIN_ENDSCHALTER) == HIGH &&
           (posStart - stepper.currentPosition()) < STEPS_MAX) {
      wdt_reset();
      stepper.runSpeed();
      if (millis() - t >= MOTOR_TIMEOUT_MS) { timedOut = true; break; }
    }
    if (!timedOut) {
      long pos = stepper.currentPosition();
      t = millis();
      while ((pos - stepper.currentPosition()) < STEPS_TO_CLOSE) {
        wdt_reset();
        stepper.runSpeed();
        if (millis() - t >= MOTOR_TIMEOUT_MS) { timedOut = true; break; }
      }
    }
    stepper.setSpeed(0);
  }

  srMotorEnable = false;
  updateSR();

  if (!timedOut) {
    for (int i = 0; i < 2; i++) {
      srRed = true; srYellow = true; srGreen = true;
      updateSR(); delay(500);
      srRed = false; srYellow = false; srGreen = false;
      updateSR(); delay(500);
    }
  }

  Serial.println(timedOut ? F("MOTOR:TIMEOUT") : F("MOTOR:OK"));
}

// ─────────────────────────────────────────────────────────
// Schieberegister aktualisieren (74HC595, aktiv LOW)
// Bit-Reihenfolge (zuletzt geshiftet = Q0):
//   [0] MotorEnable  [1] Summer  [2] Grün  [3] Gelb  [4] Rot
// ─────────────────────────────────────────────────────────

void updateSR() {
  digitalWrite(PIN_SR_RCLK, LOW);

  digitalWrite(PIN_SR_SER, srMotorEnable ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  digitalWrite(PIN_SR_SER, srSummer ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  digitalWrite(PIN_SR_SER, srGreen ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  digitalWrite(PIN_SR_SER, srYellow ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  digitalWrite(PIN_SR_SER, srRed ? LOW : HIGH);
  digitalWrite(PIN_SR_SRCLK, HIGH); digitalWrite(PIN_SR_SRCLK, LOW);

  digitalWrite(PIN_SR_RCLK, HIGH);
  digitalWrite(PIN_SR_RCLK, LOW);
}
