# Deployment (MEGA2560 WiFi R3 Clone: ATmega2560 + ESP8266)

Diese Datei beschreibt die DIP-Switch-Konfiguration und die Flash-Prozedur
fuer Boards vom Typ MEGA2560 WiFi R3 (ATmega2560 + ESP8266 + CH340G, 8-DIP-Block).

## Projektbezug

Dieses Projekt ist bereits AVR-basiert (`board = megaatmega2560`) und passt damit
direkt zum ATmega2560-Teil des Kombi-Boards.

## Build-Ziele (wie bei RFID-Boxen)

Es gibt ein klares Paar aus USB und OTA:

- USB Build/Upload: `lasersaur_usb`
- OTA Build/Upload: `lasersaur_ota`

Beide Environments bauen dieselbe OTA-faehige Firmware.
Der Unterschied ist nur der Uploadweg (USB vs. Netzwerk).

Kommandos:

```bash
pio run -e lasersaur_usb --target upload
pio run -e lasersaur_ota --target upload
```

Fuer `lasersaur_ota` gilt:

Standardziel ist `lasersaur-hmi.local` (siehe `platformio.ini`, `upload_port`).
Falls noetig kannst du beim Upload ein anderes Ziel uebergeben:

```bash
pio run -e lasersaur_ota --target upload --upload-port 10.30.0.x
```

## DIP-Switch Matrix (1..7)

| Modus | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| ATmega2560 <-> ESP8266 | ON | ON | OFF | OFF | OFF | OFF | OFF |
| USB <-> ATmega2560 (Sketch upload) | OFF | OFF | ON | ON | OFF | OFF | OFF |
| USB <-> ESP8266 (Firmware upload) | OFF | OFF | OFF | OFF | ON | ON | ON |
| USB <-> ESP8266 (Kommunikation) | OFF | OFF | OFF | OFF | ON | ON | OFF |
| Alles unabhaengig | OFF | OFF | OFF | OFF | OFF | OFF | OFF |

## Flash-Prozedur (ATmega2560)

1. USB-Verbindung vorbereiten
- CH340G Treiber installiert.
- Stabiles Datenkabel verwenden.

2. DIP setzen fuer MCU-Upload
- `USB <-> ATmega2560`: `3=ON, 4=ON`, Rest `OFF`.

3. Build und Upload

```bash
pio run -e lasersaur_usb --target upload
```

4. Serielle Ausgabe pruefen

```bash
pio device monitor --baud 115200
```

## OTA-Prozedur (Projekt)

1. MCU und ESP wie oben initial per USB testen.
2. DIP fuer normalen Betrieb setzen: `ATmega2560 <-> ESP8266` (`1=ON, 2=ON`, Rest `OFF`).
3. Sicherstellen, dass `upload_port` passt (`lasersaur-hmi.local` oder direkte IP).
4. OTA Upload starten:

```bash
pio run -e lasersaur_ota --target upload
```

## Endbetrieb

1. Fuer Kopplung AVR <-> ESP auf `1=ON, 2=ON`, Rest `OFF`.
2. Nach jeder DIP-Aenderung Board kurz resetten.
3. Vor produktivem Einsatz 24h-Dauerlauf inklusive Netzwerkunterbrechung testen.

## Troubleshooting (Kurz)

- Kein MCU-Upload: DIP 3/4, USB-Kabel, COM-Port pruefen.
- Keine AVR-ESP-Kommunikation: DIP 1/2 und UART-Parameter pruefen.
