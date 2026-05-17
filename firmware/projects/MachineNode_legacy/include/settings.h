#ifndef SETTINGS_H
#define SETTINGS_H

#ifndef RFIDCARD_AUTH_CONST
#define RFIDCARD_AUTH_CONST true
#endif

#ifndef CONTINUOUS_SERVER_CHECK
#define CONTINUOUS_SERVER_CHECK true
#endif

// PN532 SPI pins (ESP32-S3 default SPI — auf Custom PCB anpassbar)
#define PN532_SCK  12
#define PN532_MISO 13
#define PN532_MOSI 11
#define PN532_SS   10

#endif
