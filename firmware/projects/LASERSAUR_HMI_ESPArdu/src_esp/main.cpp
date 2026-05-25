/**
 * ESP8266 — WiFi-Modem für Lasersaur HMI
 *
 * Verantwortung: WiFi-Verbindung halten, HTTP POST für ATmega ausführen,
 * Telnet-Debug auf Port 23, OTA via mDNS (laser-hmi.local).
 *
 * Protokoll (Serial, 9600 Baud, LF-terminiert):
 *   ATmega → ESP:  POST:{json}\n   — HTTP POST ausführen
 *                  PONG\n          — Heartbeat-Antwort
 *   ESP → ATmega:  ACK:200\n       — POST erfolgreich
 *                  NACK:WIFI\n     — kein WiFi
 *                  NACK:DNS\n      — DNS-Auflösung fehlgeschlagen
 *                  NACK:503\n      — Server nicht erreichbar
 *                  NACK:TIMEOUT\n  — Keine Antwort vom Server
 *                  PING\n          — Heartbeat (alle 5 s)
 *
 * Build-Flags (aus platformio.secrets.ini):
 *   SERVER_HOST, AUTH_TOKEN, WIFI_SSID, WIFI_PASS
 *
 * DIP für Flash (einmalig): DIP 5+6+7 ON, Rest OFF.
 * DIP Normal-Betrieb:       DIP 1+2+3+4 ON, Rest OFF.
 * OTA danach: pio run -e laser_hmi_esp_ota --target upload
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <TelnetStream.h>
#include "settings.h"

#if __has_include("secret.h")
  #include "secret.h"
#endif

#define DBG TelnetStream
#define OTA_HOSTNAME "laser-hmi"

// --- Secrets via Build-Flags ---
#ifndef SERVER_HOST
  #define SERVER_HOST "NOT_SET"
#endif
#ifndef AUTH_TOKEN
  #define AUTH_TOKEN "NoToken"
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID "NOT_SET"
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS "NOT_SET"
#endif

// ─────────────────────────────────────────────────────────
// Forward Declarations
// ─────────────────────────────────────────────────────────

void processSerial();
void doHttpPost(const char* json);
bool connectWiFi(unsigned long timeoutMs);
void wifiReconnect();
void startNetServices();

bool netStarted    = false;
unsigned long lastPongMs = 0;

// Puffer groß genug für "POST:" + JSON (~250 Zeichen)
static char serialBuf[280];
static uint8_t serialPos = 0;


// ─────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────

void setup() {
    // Serial0 = UART zum ATmega (Normalbetrieb DIP 1+2)
    Serial.begin(9600);
    TelnetStream.begin(23);

    Serial.println(F("[ESP] Boot"));

    WiFi.hostname(OTA_HOSTNAME);
    Serial.print(F("[WiFi] Verbinde mit: "));
    Serial.println(WIFI_SSID);

    connectWiFi(25000);

    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
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
        startNetServices();
    } else {
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

    static unsigned long lastHB = 0;
    if (millis() - lastHB >= HEARTBEAT_INTERVAL_MS) {
        lastHB = millis();
        bool atmegaAlive = (lastPongMs > 0) && (millis() - lastPongMs < ATMEGA_ALIVE_TIMEOUT_MS);
        DBG.print(F("[HB] WiFi:"));
        DBG.print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "--");
        DBG.print(F(" ATmega:"));
        DBG.println(atmegaAlive ? F("OK") : F("DEAD"));
        Serial.println(F("PING"));
    }
}


// ─────────────────────────────────────────────────────────
// Eingehende ATmega-Nachrichten parsen (nicht-blockierend)
// ─────────────────────────────────────────────────────────

void processSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            serialBuf[serialPos] = '\0';
            serialPos = 0;

            DBG.print(F("[ATmega] "));
            DBG.println(serialBuf);

            if (strcmp(serialBuf, "PONG") == 0) {
                lastPongMs = millis();

            } else if (strncmp(serialBuf, "POST:", 5) == 0) {
                doHttpPost(serialBuf + 5);
            }

        } else if (c != '\r' && serialPos < sizeof(serialBuf) - 1) {
            serialBuf[serialPos++] = c;
        }
    }
}


// ─────────────────────────────────────────────────────────
// HTTP POST ausführen
// ─────────────────────────────────────────────────────────

void doHttpPost(const char* json) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("NACK:WIFI"));
        DBG.println(F("[HTTP] NACK: kein WiFi"));
        return;
    }

    WiFiClient client;
    IPAddress serverAddr;
    if (WiFi.hostByName(SERVER_HOST, serverAddr) != 1) {
        Serial.println(F("NACK:DNS"));
        DBG.println(F("[HTTP] NACK: DNS fehlgeschlagen"));
        return;
    }

    if (!client.connect(serverAddr, SERVER_PORT)) {
        Serial.println(F("NACK:503"));
        DBG.print(F("[HTTP] NACK: Verbindung fehlgeschlagen zu "));
        DBG.println(SERVER_HOST);
        return;
    }

    uint16_t bodyLen = strlen(json);
    client.print(F("POST /log HTTP/1.1\r\n"));
    client.print(F("Host: ")); client.print(SERVER_HOST); client.print(F("\r\n"));
    client.print(F("Content-Type: application/json\r\n"));
    client.print(F("Connection: close\r\n"));
    client.print(F("Content-Length: ")); client.print(bodyLen); client.print(F("\r\n"));
    client.print(F("\r\n"));
    client.print(json);

    unsigned long t = millis();
    while (client.connected() && millis() - t < SERVER_CONNECT_TIMEOUT_MS) {
        ESP.wdtFeed();
        if (client.available()) {
            String line = client.readStringUntil('\n');
            if (line.startsWith("HTTP/")) {
                int code = line.substring(9, 12).toInt();
                client.stop();
                if (code == 200) {
                    Serial.println(F("ACK:200"));
                    DBG.println(F("[HTTP] POST OK 200"));
                } else {
                    char nack[12];
                    snprintf(nack, sizeof(nack), "NACK:%d", code);
                    Serial.println(nack);
                    DBG.print(F("[HTTP] POST Fehler: "));
                    DBG.println(code);
                }
                return;
            }
        }
    }
    client.stop();
    Serial.println(F("NACK:TIMEOUT"));
    DBG.println(F("[HTTP] NACK: Timeout"));
}


// ─────────────────────────────────────────────────────────
// WiFi verbinden (blockierend mit Timeout)
// ─────────────────────────────────────────────────────────

bool connectWiFi(unsigned long timeoutMs) {
    unsigned long deadline = millis() + timeoutMs;
    uint8_t attempt = 0;

    do {
        if (attempt > 0) {
            Serial.print(F("[WiFi] Retry "));
            Serial.println(attempt + 1);
        }
        attempt++;

        WiFi.disconnect();
        delay(200);
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        unsigned long tryEnd = millis() + 8000UL;
        if (tryEnd > deadline) tryEnd = deadline;

        while (millis() < tryEnd) {
            ESP.wdtFeed();
            if (WiFi.status() == WL_CONNECTED) return true;
            delay(300);
        }
    } while (millis() < deadline);

    return false;
}


// ─────────────────────────────────────────────────────────
// Netzwerk-Dienste starten (MDNS, OTA) — einmalig nach WiFi-Connect
// ─────────────────────────────────────────────────────────

void startNetServices() {
    if (netStarted) return;
    netStarted = true;

    MDNS.begin(OTA_HOSTNAME);

    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.onStart([]()  { ESP.wdtFeed(); });
    ArduinoOTA.onEnd([]()    { DBG.println(F("[OTA] Fertig")); });
    ArduinoOTA.onError([](ota_error_t e) {
        DBG.print(F("[OTA] Fehler ")); DBG.println(e);
    });
    ArduinoOTA.begin();

    DBG.print(F("[ESP] IP: "));
    DBG.println(WiFi.localIP());
    DBG.print(F("[OTA] Bereit: "));
    DBG.println(OTA_HOSTNAME);
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
        DBG.print(F("[WiFi] Verbinde: "));    DBG.println(WIFI_SSID);
        Serial.print(F("[WiFi] Verbinde: ")); Serial.println(WIFI_SSID);
        connectWiFi(8000);
        connected = WiFi.status() == WL_CONNECTED;
        if (!connected) {
            DBG.println(F("[WiFi] FEHLGESCHLAGEN"));
            Serial.println(F("[WiFi] FEHLGESCHLAGEN"));
            return;
        }
    }

    if (connected && WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        unsigned long t = millis();
        while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - t < 5000UL) {
            ESP.wdtFeed();
            delay(300);
        }
        lastAttempt = millis();
    }

    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        DBG.print(F("[WiFi] Online: "));    DBG.println(WiFi.localIP());
        Serial.print(F("[WiFi] Online: ")); Serial.println(WiFi.localIP());
        if (!netStarted) startNetServices();
    }
}
