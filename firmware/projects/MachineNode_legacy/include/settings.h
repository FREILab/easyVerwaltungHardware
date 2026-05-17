#ifndef SETTINGS_H
#define SETTINGS_H

// --- Auth behaviour ---
#ifndef RFIDCARD_AUTH_CONST
#define RFIDCARD_AUTH_CONST true    // true = card must stay present while machine runs
#endif

// --- Timing ---
#define CARD_POLL_MS         500    // card poll interval in STANDBY and RUNNING (ms)
#define CARD_ABSENT_RESET     10    // consecutive missed reads before relay off (10 × 500ms = 5s)
#define RESET_DISPLAY_MS    2000    // duration of red LED after reject/logout (ms)
#define SERVER_PING_MS      2000    // server keepalive interval in RUNNING (ms)

// --- Pin mapping ---
#define MACHINE_RELAY_PIN   22
#define BUTTON_STOP         14      // was 13 — conflicts with PN532_MISO
#define LED_RED_PIN         32
#define LED_YELLOW_PIN      33
#define LED_GREEN_PIN       26
#define BUTTON_PRESSED       0      // buttons are active LOW

// --- PN532 SPI pins (ESP32-S3 default SPI — adjust for custom PCB) ---
#define PN532_SCK           12
#define PN532_MISO          13
#define PN532_MOSI          11
#define PN532_SS            10

#endif
