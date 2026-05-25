#pragma once

// WiFi
#define WIFI_RECONNECT_INTERVAL_MS  15000UL

// Heartbeat (ESP sendet PING an ATmega)
#define HEARTBEAT_INTERVAL_MS        5000UL

// ATmega gilt als tot wenn N Heartbeats ohne PONG
#define ATMEGA_ALIVE_TIMEOUT_MS  (HEARTBEAT_INTERVAL_MS * 3UL)

// HTTP POST Timeout
#define SERVER_CONNECT_TIMEOUT_MS    5000UL

// Server Port
#define SERVER_PORT                  5000
