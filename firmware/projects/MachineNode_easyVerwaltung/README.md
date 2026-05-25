# Machine Node — easyVerwaltung

ESP32-S3-Firmware für NFC-basierte Maschinenfreigabe mit easyVerwaltung-Backend-Integration.

> **Status: Geplant.** Basis ist `MachineNode_legacy` — Legacy-API wird durch den easyVerwaltung-Login-Contract ersetzt.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | ESP32-S3-WROOM-1-N16R8 (16 MB Flash) |
| NFC-Leser | Adafruit PN532 (SPI) |
| Ausgabe | Relais (Maschinenfreigabe), LEDs |
| Stromsensor (onboard) | Für kleine Lasten (z.B. 3D-Drucker) — direkt auf der Platine |
| Stromsensor (extern) | Interface für Remote-Sensor — für 400V-Geräte (Drehstrom) |

### Stromsensor-Varianten

Der Node erfasst Maschinenstunden über Strommessung. Je nach Maschinentyp kommt eine von zwei Varianten zum Einsatz:

- **Onboard** (kleine Lasten, 230V, z.B. 3D-Drucker): Stromsensor direkt auf der Platine integriert.
- **Extern** (400V-Geräte, Drehstrom): Steckbares Interface für einen externen Stromsensor (z.B. Stromwandler), der an der Maschine montiert wird und per Kabel zum Node führt.

## Funktion

Das Gerät liest NFC-Karten und fragt beim easyVerwaltung-Server an, ob der Nutzer die Maschine benutzen darf. Die Freigabe erfolgt über ein Relais. Zwei Betriebsmodi (pro Gerät zur Build-Zeit festgelegt):

- **Heartbeat-Modus** (`auth_permanent`): Karte muss dauerhaft aufliegen, Session wird periodisch verlängert.
- **Einmal-Modus** (`auth_onetime`): Einmalige Anmeldung, Maschine läuft bis zum manuellen Stop.

## Abgrenzung

Nachfolge von `MachineNode_legacy` (alte API). Im Unterschied zu `RFID_BOX_easyVerwaltung` verwendet der Machine Node einen **PN532-NFC-Leser** statt MFRC522 und läuft auf dem **ESP32-S3** statt dem klassischen ESP32.

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

Kommuniziert über `shared/easyAPI` mit dem easyVerwaltung-Backend:

- `POST /api/service/health` — Startup-Check
- `POST /api/service/machine/login` — Kartenanmeldung
- `POST /api/service/machine/heartbeat` — Session-Verlängerung (nur `auth_permanent`)

## Abhängigkeiten

- `adafruit/Adafruit PN532`
- `thijse/ArduinoLog`
- `shared/easyAPI` (lokal, transport-agnostisch)
