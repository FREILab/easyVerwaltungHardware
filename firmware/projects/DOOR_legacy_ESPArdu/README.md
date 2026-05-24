# DOOR — Legacy

Arduino-Firmware für RFID-gesteuerte Türverriegelung mit Schrittmotor und Legacy-API.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | Arduino UNO R4 WiFi |
| RFID-Leser | MFRC522 (SPI) |
| Antrieb | Schrittmotor via A4988/DRV8825 (AccelStepper) |
| Shift-Register | 74HC595 — steuert LEDs, Buzzer, Motor-Enable |
| Sensoren | Reed-Kontakt (Tür offen/zu), Endschalter (Heimposition) |
| Taster | Schließ-Taster, Terrassen-Taster |

## Funktion

Das Gerät liest RFID-Karten, fragt die Legacy-API an und öffnet bei Freigabe die Tür per Schrittmotor. Der Reed-Kontakt meldet den Türzustand zurück, der Endschalter dient als Referenzpunkt beim Homing.

## Abgrenzung

Dieses Projekt nutzt die **alte API**. Die Migration zur easyVerwaltung-API ist in `DOOR_easyVerwaltung` in Arbeit.

## Konfiguration

Credentials kommen aus `platformio.secrets.ini` (nicht in Git):

```ini
[secrets]
wifi_ssid     = ...
wifi_password = ...
server_ip     = dashboard.intern
auth_token    = ...
```

## Flashen

```bash
# USB
pio run -t upload

# OTA
pio run -e <env_ota> -t upload
```

## Abhängigkeiten

- `miguelbalboa/MFRC522`
- `waspinator/AccelStepper`
- `thijse/ArduinoLog`
