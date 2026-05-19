# Machine Node — Legacy

ESP32-S3-Firmware für NFC-basierte Maschinenfreigabe mit Legacy-API. Dient gleichzeitig als Dev-Kit-Plattform für neue Geräte-Entwicklung.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | ESP32-S3-WROOM-1-N16R8 (16 MB Flash) |
| NFC-Leser | Adafruit PN532 (SPI) |
| Ausgabe | Relais (Maschinenfreigabe), LEDs |

## Abgrenzung

Dieses Projekt nutzt die **alte API**. Im Unterschied zu `RFID_BOX` verwendet der Machine Node einen **PN532-NFC-Leser** statt MFRC522 und läuft auf dem **ESP32-S3** statt dem klassischen ESP32.

`MachineNode_easyVerwaltung` ist die geplante Nachfolge mit easyVerwaltung-API.

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
# Dev-Kit per USB
pio run -e machine_dev_usb -t upload

# Dev-Kit OTA
pio run -e machine_dev_ota -t upload
```

## Abhängigkeiten

- `adafruit/Adafruit PN532`
- `thijse/ArduinoLog`
