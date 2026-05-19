# DOOR — easyVerwaltung

Arduino-Firmware für RFID-gesteuerte Türverriegelung mit easyVerwaltung-Backend-Integration.

> **Status: In Entwicklung.** Basis ist `DOOR_legacy` — die Legacy-API wird durch den easyVerwaltung-Login-Contract ersetzt.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | Arduino UNO R4 WiFi |
| RFID-Leser | MFRC522 (SPI) |
| Antrieb | Schrittmotor via A4988/DRV8825 |
| Shift-Register | 74HC595 — LEDs, Buzzer, Motor-Enable |
| Sensoren | Reed-Kontakt, Endschalter |

## Konfiguration

Credentials kommen aus `platformio.secrets.ini` (nicht in Git):

```ini
[secrets]
wifi_ssid     = ...
wifi_password = ...
server_ip     = dashboard.intern
auth_token    = ...
```

## API

Wird kommunizieren über `shared/easyAPI` mit:

- `POST /api/service/health`
- `POST /api/service/machine/login`
- `POST /api/service/machine/heartbeat`
