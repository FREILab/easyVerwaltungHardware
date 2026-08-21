#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// Deine Pin-Belegung
#define PN532_SS   (18)
#define PN532_SCK  (13)
#define PN532_MISO (33)
#define PN532_MOSI (32)

// PN532-Instanz
Adafruit_PN532 nfc(PN532_SS);

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("Initialisiere Hardware-SPI...");
  
  // Hardware-SPI mit deinen Custom-Pins starten
  SPI.begin(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("FEHLER: Kein PN532 Board gefunden! Verkabelung & Schalter prüfen.");
    while (1);
  }

  nfc.SAMConfig();
  Serial.println("\nBereit! Halte das Handy an die Antenne...");
}

void loop(void) {
  uint8_t success;
  uint8_t uid[7];
  uint8_t uidLength;

  // Warten auf Tag / Smartphone
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 500);

  if (success) {
    Serial.println("\n--- NFC Signal erkannt! ---");
    
    // Wir lesen 12 Pages (48 Bytes), um NDEF-Header + Text komplett zu erfassen
    uint8_t buffer[48];
    bool readError = false;

    for (uint8_t page = 4; page < 16; page++) {
      if (!nfc.mifareultralight_ReadPage(page, &buffer[(page - 4) * 4])) {
        readError = true;
        break;
      }
    }

    if (!readError) {
      // Suche nach dem NDEF Text-Record Identifier (Typ 'T' / 0x54)
      int textStart = -1;
      for (int i = 0; i < 40; i++) {
        if (buffer[i] == 0x54) { // 0x54 = ASCII 'T' (Text Record)
          textStart = i;
          break;
        }
      }

      if (textStart != -1 && textStart + 2 < 48) {
        // Das Byte nach 'T' enthält die Statuslänge/Sprachcodelänge (z.B. 2 für "de")
        uint8_t statusByte = buffer[textStart + 1];
        uint8_t langLength = statusByte & 0x3F; 
        
        // Der echte Text beginnt genau nach dem Sprachcode
        int payloadStart = textStart + 2 + langLength;

        String parsedText = "";
        for (int i = payloadStart; i < 48; i++) {
          // Stoppen, wenn Steuerzeichen/End-Bytes auftreten
          if (buffer[i] == 0xFE || buffer[i] == 0x00) break; 
          if (buffer[i] >= 32 && buffer[i] <= 126) {
            parsedText += (char)buffer[i];
          }
        }

        Serial.print("Gelesener Text: ");
        Serial.println(parsedText);
      } else {
        Serial.println("Kein NDEF-Text-Record gefunden. Erneuten Scan versuchen!");
      }
    } else {
      Serial.println("Fehler beim Auslesen des Speichers.");
    }

    delay(2000); // 2 Sekunden Pause bis zum nächsten Scan
  }
}