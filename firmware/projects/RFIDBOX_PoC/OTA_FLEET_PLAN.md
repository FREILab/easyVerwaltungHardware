# OTA + Fleet Plan

Quelle: Session-Plan fuer RFIDBOX OTA/Fleet Management (ESP32 + Pico W).
Diese Datei liegt im Projektordner und ist damit in Git sichtbar.

## Workspace-Bezug
- Workspace-Root: /Users/marius/Github/easyVerwaltungHardware/firmware/projects/RFIDBOX_PoC
- Hauptdatei derzeit: src/main.cpp
- Zweck: Plan gilt fuer dieses Firmware-Projekt und dessen Multi-Node-Weiterentwicklung (ESP32 0.1, Pico W 1.0+).

## Kernziele
- OTA fuer Mischbetrieb ESP32 + Pico W
- Onboarding ueber RFID/QR + AP-Webinterface
- Persistente Config im Onboard-Flash (keine SD-Karte)
- Discovery + Remote-Verwaltung im lokalen Netzwerk
- Selektive Updates (einzelne Box auf Beta) + Flottenupdates
- Direkte Entwicklung aus VS Code ohne Gehaeuse oeffnen

## Hardware-Generationen (JSON relevant)
- Node 0.1: ESP32 (bestehend)
- Node 1.0: Pico W 230V (in Entwicklung)
- Node 1.1: Pico W 230V Fixes (zukuenftig)
- Node 2.0/3.0+: zukuenftig

## Config-Schema (geraeteseitig)
Pflichtfelder in der lokalen Konfiguration:
- device_id
- machine_id
- hardware_type: esp32 | picow
- hardware_version: 0.1 | 1.0 | 1.1 | 2.0 ...
- wifi.ssid / wifi.password
- server.url / server.token
- firmware.channel (stable/beta)
- firmware.auto_update (bool)

## Server/Manifest-Modell
Firmware wird nach Plattform + Hardware-Version + Kanal aufgeloest:
- /firmware/esp32/0.1/stable/latest.bin
- /firmware/picow/1.0/stable/latest.bin.gz
- /firmware/picow/1.1/beta/latest.bin.gz

Manifest liefert pro Ziel:
- version
- url
- checksum
- groesse
- compressed flag
- release_date

Per-Device Overrides:
- channel override pro device
- optional hardware binding pro device

## OTA-Strategie
ESP32:
- .bin unkomprimiert
- A/B-Fallback Schema

Pico W:
- .bin.gz (GZIP)
- Streaming-Dekompression waehrend Download
- Staging + Recovery

## API-Richtung
- GET /api/firmware/manifest?hardware_type=...&hardware_version=...
- GET /api/firmware/download/{platform}/{hardware_version}/{channel}/{filename}
- POST /api/firmware/upload/{platform}/{hardware_version}

## VS Code Entwicklung
ESP32:
- PlatformIO OTA (mDNS Hostname)

Pico W:
- GCC/CMake Build
- Initial einmalig USB UF2 Drag&Drop
- danach OTA per Python Upload-Script

## Discovery-App
- mDNS discovery: rfidbox-*.local
- Status inkl. hardware_type + hardware_version
- Dashboard-Filter nach Generation (0.1, 1.0, 1.1, ...)
- Aktionen: einzelnes Update, Kanalwechsel, Flottenupdate

## Entscheidungen
- Keine SD-Karte
- Onboard-Flash fuer Config
- Externer Flash hoechstens spaeter optional
- GZIP fuer Pico W OTA fix gesetzt
- Multi-Node-Versionierung fix gesetzt
