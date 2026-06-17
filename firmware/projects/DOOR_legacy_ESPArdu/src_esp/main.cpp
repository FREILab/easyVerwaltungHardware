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
#include <TZ.h>
#include <time.h>
#include <cstdarg>
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
bool waitMotorOK(bool ledR, bool ledY, bool ledG);
void processSerial();
void handleRFID(const char* uid);
bool connectWiFi(unsigned long timeoutMs, bool blinkLed = false);
bool configureStaticIp();
void wifiReconnect();
void startNetServices();
void busDbg(const char* fmt, ...);
void syncTime();
void checkServerHealth();

bool useStaticIp = false;
bool netStarted  = false;
unsigned long lastPongMs = 0;

// ─────────────────────────────────────────────────────────
// Debug-Ausgaben: auf UART-Bus zum ATmega (für angeschlossenen Logger,
// "DBG:"-Prefix wird vom ATmega-Parser ignoriert) UND auf Telnet spiegeln.
// ─────────────────────────────────────────────────────────

void busDbg(const char* fmt, ...) {
  char msg[96];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  char line[116];
  time_t now = time(nullptr);
  if (now > 100000) {
    struct tm tmInfo;
    localtime_r(&now, &tmInfo);
    char ts[9];  // "HH:MM:SS\0"
    strftime(ts, sizeof(ts), "%H:%M:%S", &tmInfo);
    snprintf(line, sizeof(line), "DBG:[%s] %s", ts, msg);
  } else {
    snprintf(line, sizeof(line), "DBG:[+%lus] %s", millis() / 1000, msg);
  }
  Serial.println(line);
  DBG.println(line);
}

// Uhrzeit per NTP holen (Router laut IT-Team mit aktivem NTP-Server).
void syncTime() {
  configTime(TZ_Europe_Berlin, "192.168.178.1", "pool.ntp.org");
}

// ─────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────

void setup() {
  // Serial0 = UART zum ATmega (Normal-Betrieb DIP 7+8)
  // Bei DIP SW5+SW6 ON (ESP Serial Monitor): Serial-Output hier sichtbar
  Serial.begin(9600);
  Serial.println(F("[ESP] Boot"));
  TelnetStream.begin(23);  // früh starten — safe ohne WiFi, kein Client → Output verworfen

  // ESP.getResetReason() — mögliche Werte und Ursachen:
  //   "Power On"           Stromversorgung an/aus (Power-Cycle)
  //   "External System"    Reset-Pin/Taster gezogen
  //   "Hardware Watchdog"  Hardware-WDT: CPU war so eingefroren, dass
  //                         selbst der periodische Software-WDT-Timer
  //                         nicht mehr feuern konnte -> harter Hänger
  //   "Software Watchdog"  Software-WDT: loop()/eine Funktion hat zu lange
  //                         ohne yield()/ESP.wdtFeed() blockiert
  //                         (z.B. ein hängender client.connect())
  //   "Exception"          Fataler Crash (z.B. Stack-Overflow,
  //                         Out-of-Bounds-Zugriff, "Fatal exception")
  //   "Software/System restart"  ESP.restart()/ESP.reset() wurde gezielt
  //                         aufgerufen (z.B. HEALTH-Restart, OTA)
  //   "Deep-Sleep Wake"    Aufwachen aus Deep-Sleep (hier ungenutzt)
  busDbg("BOOT reason=%s heap=%u", ESP.getResetReason().c_str(), ESP.getFreeHeap());

  useStaticIp = configureStaticIp();
  Serial.print(F("[WiFi] Verbinde mit: "));
  Serial.println(WIFI_SSID);

  WiFi.hostname(OTA_HOSTNAME);
  connectWiFi(25000, true);

  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    sendLED(false, true, true);  // gelb+grün = verbunden, warte auf DHCP
    Serial.print(F("[WiFi] Warte auf DHCP"));
    unsigned long t = millis();
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - t < 15000UL) {
      ESP.wdtFeed();
      Serial.print('.');
      delay(500);
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    Serial.print(F("[WiFi] OK, IP: "));
    Serial.println(WiFi.localIP());
    syncTime();
    startNetServices();
    sendLED(false, false, true);  // grün = verbunden
    delay(500);
    sendLED(false, true, false);  // gelb = standby
  } else if (WiFi.status() == WL_CONNECTED) {
    sendLED(false, true, true);   // gelb+grün = kein DHCP, loop übernimmt
    Serial.println(F("[WiFi] Verbunden, kein DHCP – retry im Loop"));
  } else {
    sendLED(true, false, false);  // rot = kein WiFi, bleibt bis Reconnect
    Serial.println(F("[WiFi] FEHLGESCHLAGEN"));
  }
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
  checkServerHealth();

  // Erkennt einzelne Loop-Durchläufe >1s (z.B. hängender client.connect()),
  // auch wenn es noch nicht zum WDT-Reset kommt — "Near-Miss"-Daten.
  static unsigned long lastLoopMs = 0;
  unsigned long nowMs = millis();
  if (lastLoopMs != 0 && nowMs - lastLoopMs > 1000) {
    busDbg("SLOWLOOP dt=%lu", nowMs - lastLoopMs);
  }
  lastLoopMs = nowMs;

  if (currentState == RESET && millis() - resetEnteredAt >= RESET_DISPLAY_MS) {
    currentState = STANDBY;
    sendLED(false, true, false);
  }

  static unsigned long lastHB = 0;
  if (millis() - lastHB >= HEARTBEAT_INTERVAL_MS) {
    lastHB = millis();
    const char* stateStr = currentState == STANDBY        ? "STANDBY"
                         : currentState == IDENTIFICATION ? "IDENT"
                                                          : "RESET";
    bool atmegaAlive = (lastPongMs > 0) &&
                       (millis() - lastPongMs < ATMEGA_ALIVE_TIMEOUT_MS);
    busDbg("HB up=%lu wifi=%s rssi=%d heap=%u frag=%u%% state=%s atmega=%s",
           millis() / 1000,
           WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "--",
           WiFi.RSSI(), ESP.getFreeHeap(), ESP.getHeapFragmentation(),
           stateStr, atmegaAlive ? "OK" : "DEAD");
    Serial.println(F("PING"));
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

      if (strcmp(buf, "PONG") == 0) {
        lastPongMs = millis();

      } else if (strncmp(buf, "ATMDBG:", 7) == 0) {
        busDbg("[ATM] %s", buf + 7);

      } else if (strncmp(buf, "RFID:", 5) == 0 && currentState == STANDBY) {
        handleRFID(buf + 5);

      } else if (strcmp(buf, "BTN:CLOSE") == 0 && currentState == STANDBY) {
        DBG.println(F("[ESP] Schließen"));
        sendLED(true, true, true);   // alle LEDs = Closing
        Serial.println(F("MOTOR:CLOSE"));
        waitMotorOK(true, true, true);
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
  busDbg("[ESP] RFID: %s", uid);

  // Gehaltene Karte wird vom ATmega nach PCD_Init() ~1-2s später erneut
  // erkannt und landet als zweites RFID-Event im ESP-RX-Buffer. Ohne Cooldown
  // würde processSerial() es noch in derselben while-Iteration verarbeiten
  // → zweites MOTOR:OPEN. lastGrantedMs wird nach waitMotorOK() gesetzt.
  static unsigned long lastGrantedMs = 0;
  if (strcmp(uid, lastUid) == 0 && millis() - lastGrantedMs < RFID_GRANT_COOLDOWN_MS) {
    busDbg("[ESP] RFID Duplikat, ignoriert");
    return;
  }

  if (strcmp(uid, lastUid) == 0) {
    retryCount++;
  } else {
    retryCount = 0;
  }

  currentState = IDENTIFICATION;
  sendLED(false, false, true);   // Grün = Identifikation läuft

  char result = checkServer(uid);

  if (result == 1) {
    busDbg("[ESP] Zugang gewährt");
    strncpy(lastUid, uid, UID_BUF_LEN - 1);
    lastUid[UID_BUF_LEN - 1] = '\0';

    if (retryCount >= 2) {
      Serial.println(F("MOTOR:FORCE_OPEN"));
    } else {
      Serial.println(F("MOTOR:OPEN"));
    }
    retryCount = 0;

    waitMotorOK(false, false, true);
    lastGrantedMs = millis();  // Debounce-Fenster ab jetzt (nach Motor, vor Buzzer)

    // Buzzer + Grün blinkend für 2s
    Serial.println(F("BUZZ:1"));
    {
      bool blinkState = true;
      unsigned long blinkAt = millis();
      sendLED(false, false, true);
      unsigned long buzzStart = millis();
      while (millis() - buzzStart < BUZZ_DURATION_MS) {
        ESP.wdtFeed();
        ArduinoOTA.handle();
        MDNS.update();
        if (millis() - blinkAt >= 300) {
          blinkAt = millis();
          blinkState = !blinkState;
          sendLED(false, false, blinkState);
        }
        delay(20);
      }
    }

    Serial.println(F("BUZZ:0"));
    currentState = STANDBY;
    sendLED(false, true, false);

  } else {
    if (result == 0) {
      busDbg("[ESP] Karte abgelehnt");
      retryCount = 0;
      lastUid[0] = '\0';
    } else {
      busDbg("[ESP] Server nicht erreichbar");
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

bool waitMotorOK(bool ledR, bool ledY, bool ledG) {
  static char buf[20];
  static uint8_t pos = 0;

  bool blinkState = true;
  unsigned long blinkAt = millis();
  sendLED(ledR, ledY, ledG);

  unsigned long deadline = millis() + MOTOR_WAIT_TIMEOUT_MS;

  while (millis() < deadline) {
    ESP.wdtFeed();
    ArduinoOTA.handle();
    MDNS.update();

    if (millis() - blinkAt >= 300) {
      blinkAt = millis();
      blinkState = !blinkState;
      sendLED(blinkState ? ledR : false,
              blinkState ? ledY : false,
              blinkState ? ledG : false);
    }

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
  static uint8_t consecutiveFails = 0;

  // WiFi-Stack-Reset nach N aufeinanderfolgenden Netzwerkfehlern.
  // Deckt den Fall "WL_CONNECTED aber TCP tot" ab, der nur per
  // Disconnect+Reconnect heilbar ist.
  auto onNetFail = [&]() {
    if (++consecutiveFails >= 3) {
      consecutiveFails = 0;
      busDbg("[HTTP] WiFi-Stack-Reset nach 3 Fehlern");
      WiFi.disconnect();
    }
  };

  if (WiFi.status() != WL_CONNECTED) return -1;

  WiFiClient client;
  IPAddress serverAddr;

  ESP.wdtFeed();
  if (WiFi.hostByName(SERVER_IP, serverAddr) != 1) {
    busDbg("[HTTP] DNS fehlgeschlagen");
    onNetFail(); return -1;
  }

  ESP.wdtFeed();
  client.setTimeout(3000);
  unsigned long connStart = millis();
  bool connOk = client.connect(serverAddr, 80);
  busDbg("[HTTP] connect dt=%lu ok=%d", millis() - connStart, connOk);
  if (!connOk) {
    onNetFail(); return -1;
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

  busDbg("[HTTP] Antwort: %s", message);

  consecutiveFails = 0;
  return strstr(message, "true") != nullptr ? 1 : 0;
}

// ─────────────────────────────────────────────────────────
// Periodischer Server-Reachability-Check, unabhängig von RFID-Scans.
// checkServer() (oben) wird nur bei einem Scan aufgerufen — nachts passiert
// das nicht, d.h. ohne diesen Check gibt es nachts keine Recovery, falls
// Server/WiFi in einen schlechten Zustand geraten. Nach mehreren
// aufeinanderfolgenden Fehlversuchen: ESP.restart() (voller Reset von
// WiFi-Stack/Heap), nur wenn die Tür gerade nicht bedient wird.
// ─────────────────────────────────────────────────────────

void checkServerHealth() {
  static unsigned long lastCheck = 0;
  static uint8_t fails = 0;

  if (millis() - lastCheck < SERVER_HEALTHCHECK_INTERVAL_MS) return;
  lastCheck = millis();
  if (WiFi.status() != WL_CONNECTED || currentState != STANDBY) return;

  IPAddress serverAddr;
  ESP.wdtFeed();
  bool ok = WiFi.hostByName(SERVER_IP, serverAddr) == 1;
  if (ok) {
    WiFiClient client;
    client.setTimeout(3000);
    unsigned long t = millis();
    ok = client.connect(serverAddr, 80);
    busDbg("HEALTH connect dt=%lu ok=%d", millis() - t, ok);
    client.stop();
  } else {
    busDbg("HEALTH dns fail");
  }

  if (ok) {
    fails = 0;
  } else if (++fails >= SERVER_HEALTHCHECK_FAIL_THRESHOLD) {
    busDbg("HEALTH restart after %u fails", fails);
    Serial.flush();
    delay(100);
    ESP.restart();
  } else {
    busDbg("HEALTH fail x%u", fails);
  }
}

// ─────────────────────────────────────────────────────────
// WiFi verbinden (blockierend mit Timeout)
// ─────────────────────────────────────────────────────────

bool connectWiFi(unsigned long timeoutMs, bool blinkLed) {
  unsigned long deadline = millis() + timeoutMs;
  uint8_t attempt = 0;

  do {
    if (attempt > 0) {
      busDbg("[WiFi] Retry %d", attempt + 1);
      if (blinkLed) { sendLED(true, false, false); delay(300); }  // kurz rot = Retry
    }
    attempt++;

    WiFi.disconnect();
    delay(200);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long tryEnd = millis() + 8000UL;
    if (tryEnd > deadline) tryEnd = deadline;

    bool ledState = false;
    while (millis() < tryEnd) {
      ESP.wdtFeed();
      if (WiFi.status() == WL_CONNECTED) return true;
      if (blinkLed) { ledState = !ledState; sendLED(false, ledState, false); }
      delay(300);
    }
  } while (millis() < deadline);

  return false;
}

// ─────────────────────────────────────────────────────────
// Netzwerk-Dienste starten (MDNS, OTA) — nach jedem (Re-)Connect
// ─────────────────────────────────────────────────────────

void startNetServices() {
  if (!netStarted) {
    // Callbacks nur einmalig registrieren
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.onStart([]()  { ESP.wdtFeed(); });
    ArduinoOTA.onEnd([]()    { busDbg("[OTA] Fertig"); });
    ArduinoOTA.onError([](ota_error_t e) {
      busDbg("[OTA] Fehler %d", e);
    });
    netStarted = true;
  }
  // mDNS und OTA bei jedem (Re-)Connect neu starten, sonst
  // sterben sie nach dem ersten WiFi-Drop dauerhaft.
  MDNS.begin(OTA_HOSTNAME);
  ArduinoOTA.begin();

  busDbg("[OTA] Bereit: %s", WiFi.localIP().toString().c_str());
}

// ─────────────────────────────────────────────────────────
// WiFi Reconnect (non-blocking, wird im Loop aufgerufen)
// ─────────────────────────────────────────────────────────

void wifiReconnect() {
  static unsigned long lastAttempt = 0;
  bool connected = WiFi.status() == WL_CONNECTED;
  bool hasIp     = WiFi.localIP() != IPAddress(0, 0, 0, 0);

  if (connected && hasIp && netStarted) return;
  if (millis() - lastAttempt < WIFI_RECONNECT_INTERVAL_MS) return;
  lastAttempt = millis();

  if (!connected) {
    busDbg("[WiFi] Verbinde mit: %s", WIFI_SSID);
    connectWiFi(8000, true);
    connected = WiFi.status() == WL_CONNECTED;
    if (!connected) {
      sendLED(true, false, false);  // rot = kein WiFi, bleibt bis Reconnect klappt
      busDbg("[WiFi] FEHLGESCHLAGEN");
    }
  }

  if (connected && WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    busDbg("[WiFi] Warte auf DHCP...");
    unsigned long t = millis();
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - t < 5000UL) {
      ESP.wdtFeed();
      delay(300);
    }
    lastAttempt = millis();
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    busDbg("[WiFi] Online: %s", WiFi.localIP().toString().c_str());
    syncTime();
    sendLED(false, false, true);  // grün = online
    delay(500);
    startNetServices();  // immer: mDNS + OTA nach (Re-)Connect neu starten
    if (currentState == STANDBY) sendLED(false, true, false);
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
