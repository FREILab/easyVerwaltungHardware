# LASERSAUR HMI — easyVerwaltung

Arduino-Firmware für das HMI-Board des Lasersaur-Laserplotters mit easyVerwaltung-Backend-Integration.

> **Status: In Entwicklung.** Basis ist `LASERSAUR_HMI_legacy` — Ethernet-Shield entfällt, WiFi (Giga R1 onboard), API wird auf den easyVerwaltung-Contract migriert.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | Arduino Giga R1 (onboard WiFi) |
| Temperatursensoren | MCP9808 (I2C, 4×) |
| Drucksensoren | MPL3115A2 (I2C) |
| Display | LCD 20×4 via I2C-Mux (LiquidCrystal_I2C) |
| Netzwerk | WiFi (onboard, kein Shield) |

## Abgrenzung

Dieses Projekt ist die Nachfolge von `LASERSAUR_HMI_legacy` (Mega + Ethernet-Shield, statische IP). Wesentliche Änderungen:

- Ethernet → WiFi (onboard Giga R1)
- Statische IP → DHCP + mDNS (`lasersaur-hmi.local`)
- Alte HTTP-POST-API → easyVerwaltung-Contract
- ArduinoJson v5 → v7

## Konfiguration

Credentials kommen aus `platformio.secrets.ini` (nicht in Git):

```ini
[secrets]
server_host   = dashboard.intern
service_token = ...
wifi_ssid     = ...
wifi_password = ...
```

Vorlage: `platformio.secrets.ini.example`

## Flashen

```bash
# Erstmaliges Flashen per USB
pio run -e lasersaur_hmi_usb --target upload

# OTA (danach immer so)
pio run -e lasersaur_hmi_ota --target upload
```

OTA läuft über mDNS-Hostname `lasersaur-hmi.local`.

## Debugging

```bash
pio device monitor -e lasersaur_hmi_usb   # Serial 115200 Baud
```

## Abhängigkeiten

- `adafruit/Adafruit MPL3115A2 Library`
- `adafruit/Adafruit MCP9808 Library`
- `marcoschwartz/LiquidCrystal_I2C`
- `bblanchon/ArduinoJson @ ^7`
