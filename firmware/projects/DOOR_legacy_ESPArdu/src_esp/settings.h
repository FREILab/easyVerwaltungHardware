#pragma once

// --- WiFi ---
#define WIFI_RECONNECT_INTERVAL_MS  15000UL

// --- Heartbeat ---
#define HEARTBEAT_INTERVAL_MS        5000UL

// --- State Machine ---
#define RESET_DISPLAY_MS             2000UL   // Wie lange RESET-State angezeigt wird
#define BUZZ_DURATION_MS             2000UL   // Buzzer nach erfolgreichem Öffnen

// --- Motor-Wartezeit auf ATmega-Antwort ---
#define MOTOR_WAIT_TIMEOUT_MS       15000UL

// --- Server ---
#define SERVER_CONNECT_TIMEOUT_MS    5000UL

// --- ATmega Watchdog ---
// Kein PONG nach N Heartbeats → "DEAD" im Log
#define ATMEGA_ALIVE_TIMEOUT_MS  (HEARTBEAT_INTERVAL_MS * 3UL)
