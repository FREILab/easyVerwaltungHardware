#pragma once

// --- WiFi ---
#define WIFI_RECONNECT_INTERVAL_MS  15000UL

// --- Heartbeat ---
#define HEARTBEAT_INTERVAL_MS        5000UL

// --- State Machine ---
#define RESET_DISPLAY_MS             2000UL   // Wie lange RESET-State angezeigt wird
#define BUZZ_DURATION_MS             2000UL   // Buzzer nach erfolgreichem Öffnen

// --- Motor-Wartezeit auf ATmega-Antwort ---
// Muss >= ATmega-Worst-Case sein (runCloseDoor: DOOR_REED_TIMEOUT_MS 30s +
// 2x MOTOR_TIMEOUT_MS 5s = ~42s), sonst läuft ESP weiter (HB/SLOWLOOP/PING
// auf den Bus), während der ATmega noch blockiert ist und seinen 64-Byte
// UART-RX-Puffer nicht leert -> Überlauf, PONG/Befehle gehen verloren.
#define MOTOR_WAIT_TIMEOUT_MS       45000UL

// --- Server ---
#define SERVER_CONNECT_TIMEOUT_MS    5000UL

// --- ATmega Watchdog ---
// Kein PONG nach N Heartbeats → "DEAD" im Log
#define ATMEGA_ALIVE_TIMEOUT_MS  (HEARTBEAT_INTERVAL_MS * 3UL)

// --- Server-Erreichbarkeits-Check (unabhängig von RFID-Scans) ---
// Schließt die Lücke, dass checkServer() nachts nie aufgerufen wird,
// weil dafür ein RFID-Scan nötig ist.
#define SERVER_HEALTHCHECK_INTERVAL_MS     60000UL  // alle 60s prüfen
#define SERVER_HEALTHCHECK_FAIL_THRESHOLD       3   // nach 3x (~3 Min) Restart
