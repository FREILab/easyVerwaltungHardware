# LASERSAUR HMI — easyVerwaltung

Arduino-Firmware für das HMI-Board des Lasersaur-Laserplotters mit easyVerwaltung-Backend-Integration.

> **Status: In Entwicklung.** Basis ist `LASERSAUR_HMI_legacy` — Sensordaten-Reporting und API-Anbindung werden auf den easyVerwaltung-Contract migriert.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | Arduino Mega |
| Sensoren | MCP9808 (Temperatur, I2C), MPL3115A2 (Druck, I2C) |
| Display | LCD via I2C-Mux |

## Konfiguration

Credentials kommen aus `platformio.secrets.ini` (nicht in Git):

```ini
[secrets]
wifi_ssid     = ...
wifi_password = ...
server_ip     = dashboard.intern
auth_token    = ...
```
