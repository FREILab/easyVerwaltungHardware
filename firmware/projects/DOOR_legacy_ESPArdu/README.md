# DOOR — Legacy

Arduino-Firmware für RFID-gesteuerte Türverriegelung mit Schrittmotor und Legacy-API.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | RobotDyn UNO+WiFi R3 (ATmega328P + ESP8266) |
| RFID-Leser | MFRC522 (SPI) |
| Antrieb | Schrittmotor via A4988/DRV8825 (AccelStepper) |
| Shift-Register | 74HC595 — steuert LEDs, Buzzer, Motor-Enable |
| Sensoren | Reed-Kontakt (Tür offen/zu), Endschalter (Heimposition) |
| Taster | Schließ-Taster |

## Architektur

Zwei Firmwares auf einem Board:

```
[Backend] ←HTTP→ [ESP8266] ←UART 9600→ [ATmega328P]
                     ↑OTA                     ↓
              tuer-01.local        RFID, Motor, LEDs, Taster
```

**ATmega328P** (`src_atmega/`) — I/O-Expander: liest RFID-Karten, steuert Schrittmotor und
Schieberegister, meldet Taster/Reed per UART an den ESP8266. Firmware ist stabil und wird
nach dem ersten Flash nicht mehr geändert.

**ESP8266** (`src_esp/`) — State Machine + WiFi + OTA: verbindet sich mit dem Backend,
entscheidet über Türöffnung, sendet Befehle an den ATmega. Alle Ablaufänderungen kommen
per OTA, kein USB-Zugriff nötig.

## DIP-Schalter (RobotDyn)

| Aktion | SW1 | SW2 | SW3 | SW4 | SW5 | SW6 | SW7 | SW8 |
|---|---|---|---|---|---|---|---|---|
| ATmega flashen (1× einmalig) | — | — | ON | ON | — | — | — | — |
| ESP8266 flashen (1× einmalig) | — | — | — | — | ON | ON | ON | — |
| ESP8266 Serial Monitor | — | — | — | — | ON | ON | — | — |
| **Normal-Betrieb** (ATmega↔ESP) | **ON** | **ON** | — | — | — | — | — | — |

## Konfiguration

WLAN-Zugangsdaten kommen aus `include/secret.h` (nicht in Git):

```cpp
#define WIFI_SSID "..."
#define WIFI_PASS "..."
```

Weitere Credentials in `platformio.secrets.ini` (nicht in Git):

```ini
[secrets]
server_ip  = dashboard.intern
auth_token = ...
```

IP-Adresse kommt per DHCP. OTA und Monitor laufen via mDNS-Hostname (`tuer-01.local`).

## Flashen

### Ersteinrichtung (einmalig)

```bash
# 1. ATmega flashen — DIP: SW3+SW4 ON
# Hinweis: RobotDyn liefert Optiboot mit 57600 Baud aus (upload_speed in platformio.ini gesetzt)
pio run -e door_01_atmega_usb --target upload

# 2. ESP8266 flashen — DIP: SW5+SW6+SW7 ON
pio run -e door_01_esp_usb --target upload

# 3. DIP auf Normal-Betrieb: SW1+SW2 ON
```

### ESP8266 OTA (danach immer so)

```bash
# Kein USB, kein DIP-Wechsel nötig
pio run -e door_01_esp_ota --target upload
```

Für Tür 02 entsprechend `door_02_*` verwenden.

## Serielles Protokoll (ATmega ↔ ESP8266)

| Richtung | Nachricht | Bedeutung |
|---|---|---|
| ATmega → ESP | `RFID:aa:bb:cc:dd` | Karte erkannt |
| ATmega → ESP | `BTN:CLOSE` | Schließ-Taster gedrückt |
| ATmega → ESP | `REED:1` / `REED:0` | Tür physisch offen/zu |
| ATmega → ESP | `MOTOR:OK` / `MOTOR:TIMEOUT` | Motor-Status |
| ESP → ATmega | `LED:RYG` | LEDs setzen (z.B. `LED:010` = Gelb) |
| ESP → ATmega | `BUZZ:1` / `BUZZ:0` | Buzzer |
| ESP → ATmega | `MOTOR:OPEN` / `MOTOR:CLOSE` | Motor-Befehl |
| ESP → ATmega | `MOTOR:FORCE_OPEN` | Öffnen mit Force (Retry) |

## Abhängigkeiten

- `miguelbalboa/MFRC522` (ATmega)
- `waspinator/AccelStepper` (ATmega)
- `ESP8266WiFi`, `ESP8266mDNS`, `ArduinoOTA` (ESP8266, im Core enthalten)

## Abgrenzung

Dieses Projekt nutzt die **alte API**. Die Migration zur easyVerwaltung-API ist in `DOOR_easyVerwaltung` in Arbeit.
