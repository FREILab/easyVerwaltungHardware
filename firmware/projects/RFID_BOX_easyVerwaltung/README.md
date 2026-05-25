# RFID_BOX — easyVerwaltung

ESP32-Firmware für RFID-basierte Maschinenfreigabe mit easyVerwaltung-Backend-Integration.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | ESP32 Dev1 |
| RFID-Leser | MFRC522 (SPI) |
| Ausgabe | Relais (Maschinenfreigabe), 3 LEDs (rot/gelb/grün) |
| Eingabe | RFID-Karten-Button, Stop-Taster |

## Funktion

Das Gerät liest RFID-Karten und fragt beim easyVerwaltung-Server an, ob der Nutzer die Maschine benutzen darf. Die Freigabe erfolgt über ein Relais. Zwei Betriebsmodi:

- **Heartbeat-Modus** (`auth_permanent`): Karte muss dauerhaft aufliegen, Server wird alle N Sekunden (vom Server vorgegeben) erneut gefragt.
- **Einmal-Modus** (`auth_onetime`): Einmalige Anmeldung, danach läuft die Maschine bis zum manuellen Stop.

## Konfiguration

Credentials kommen aus `platformio.secrets.ini` (nicht in Git):

```ini
[secrets]
wifi_ssid     = ...
wifi_password = ...
server_ip     = dashboard.intern
auth_token    = ...
```

Maschinen-ID und -Name werden pro Environment in `platformio.ini` gesetzt.

## Flashen

```bash
# Erstmaliges Flashen per USB
pio run -e xtool_usb --target upload

# OTA-Update (kein USB nötig)
pio run -e xtool_ota --target upload
```

OTA läuft via mDNS-Hostname (z.B. `xtool-01.local`). Für andere Geräte: `<machine>_usb` / `<machine>_ota`.

## Debugging

```bash
# Serial Monitor (USB, 115200 Baud)
pio device monitor -e xtool_usb
```

Kein Telnet — Serial Monitor nur per USB.

## API

Kommuniziert über `shared/easyAPI` mit dem easyVerwaltung-Backend:

- `POST /api/service/health` — Startup-Check
- `POST /api/service/machine/login` — Kartenanmeldung
- `POST /api/service/machine/heartbeat` — Session-Verlängerung

## Abhängigkeiten

- `miguelbalboa/MFRC522`
- `thijse/ArduinoLog`
- `shared/easyAPI` (lokal, transport-agnostisch)
