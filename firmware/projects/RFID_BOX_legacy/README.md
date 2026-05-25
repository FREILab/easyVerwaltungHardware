# RFID_BOX — Legacy

ESP32-Firmware für RFID-basierte Maschinenfreigabe mit Legacy-API.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | ESP32 Dev1 |
| RFID-Leser | MFRC522 (SPI) |
| Ausgabe | Relais (Maschinenfreigabe), 3 LEDs (rot/gelb/grün) |
| Eingabe | RFID-Karten-Button, Stop-Taster |

## Abgrenzung

Dieses Projekt nutzt die **alte HTTP-GET-API** des Backends. Die Weiterentwicklung findet in `RFID_BOX_easyVerwaltung` statt, das die neue easyVerwaltung-REST-API mit Login/Heartbeat-Contract verwendet.

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
# Erstmaliges Flashen per USB
pio run -e xtool_usb --target upload

# OTA-Update (kein USB nötig)
pio run -e xtool_ota --target upload
```

OTA läuft via mDNS-Hostname (z.B. `xtool-01.local`). Für andere Geräte: `<machine>_usb` / `<machine>_ota`.

## Debugging

**Serial** (USB, 115200 Baud):
```bash
pio device monitor -e xtool_usb
```

**Telnet** (WiFi, kein USB nötig):
```bash
# pio verwendet monitor_port = socket://xtool-01.local:23
pio device monitor -e xtool_ota
```

Alle OTA-Environments haben `monitor_port` auf den jeweiligen mDNS-Hostnamen vorbelegt.

## Abhängigkeiten

- `miguelbalboa/MFRC522`
- `thijse/ArduinoLog`
