# OTA + Fleet Plan

Quelle: OTA/Fleet-Konzept fuer RFIDBOX_PoC.

## Workspace-Bezug
- Workspace-Root: /Users/marius/Github/easyVerwaltungHardware/firmware/projects/RFIDBOX_PoC
- Hauptdatei derzeit: src/main.cpp
- Ziel: Einheitliches Fleet-Deployment-Prinzip fuer RFIDBOX-Knoten.

## Kernziele
- Einheitlicher OTA-Workflow fuer alle RFIDBOX-Nodes
- Per-Device-Config persistent speichern (statt Build-spezifischer Firmware)
- Einmalig-Provisioning neuer Geraete ueber AP-Webinterface
- Zentrale Firmware-Ablage mit Versionierung und Kanalmodell (stable/beta)
- Selektive Updates pro Geraet + Flottenupdate
- Direkte Entwicklung aus VS Code weiterhin moeglich

## Hardware-Generationen
- RFIDBOX v0.1: ESP32 (aktuell)
- RFIDBOX v1.x: zukunftige Revisionen

## Aktueller Stand
- Build aktuell als PlatformIO Environment `esp32dev`
- Firmware wird lokal gebaut und klassisch deployed
- Noch kein zentrales Manifest-gesteuertes Pull-OTA aktiv

## Ziel-Config-Schema (geraeteseitig)
Pflichtfelder in lokaler Config (NVS/Preferences):
- device_id (z.B. rfidbox-01)
- device_name
- machine_id
- wifi.ssid / wifi.password
- server.url / server.token
- firmware.channel (stable/beta)
- firmware.auto_update (bool)

Optionale Felder:
- firmware.server_url (Override)
- ota_hostname_prefix

## Migrations-Strategie
Zwei Phasen fuer die schrittweise Umstellung:

Phase 1 (kurzfristig):
- Bestehenden Build-/Upload-Workflow beibehalten
- Server-Ablage und Manifest-Struktur aufsetzen
- Device-Overrides im Manifest vorbereiten

Phase 2 (mittelfristig):
- Generische Firmware fuer alle RFIDBOX-Nodes
- Config aus Build-Parametern in NVS ueberfuehren
- Bei fehlender Config: AP-Provisioning starten
- OTA als Pull-Modell via Server-Manifest

## Server/Manifest-Modell
Firmware wird nach Plattform, Hardware-Version und Kanal aufgeloest:
- /firmware/esp32/0.1/stable/latest.bin
- /firmware/esp32/0.1/beta/latest.bin
- /firmware/esp32/1.0/stable/latest.bin (zukunftig)

Manifest liefert pro Ziel:
- version
- url
- checksum (SHA256)
- groesse
- release_date

Per-Device-Overrides:
- channel override pro device_id
- optional hardware binding pro device_id

## Detaillierte Ablage-Struktur (Server)
Empfohlener Root:
- /srv/rfidbox/

Vollstaendige Struktur:
```text
/srv/rfidbox/
    firmware/
        manifest/
            manifest.json
            manifest.schema.json
            manifest.history/
                2026-05-11T120000Z-manifest.json
        esp32/
            0.1/
                stable/
                    latest.bin
                    latest.sha256
                    metadata.json
                    archive/
                        1.0.0/
                            firmware.bin
                            firmware.sha256
                            metadata.json
                        1.0.1/
                            firmware.bin
                            firmware.sha256
                            metadata.json
                beta/
                    latest.bin
                    latest.sha256
                    metadata.json
                    archive/
                        1.1.0-beta.1/
                        1.1.0-beta.2/
        active/
            esp32-0.1 -> /srv/rfidbox/firmware/esp32/0.1/stable/
        fallback/
            esp32-0.1 -> /srv/rfidbox/firmware/esp32/0.1/archive/1.0.0/
    uploads/
        incoming/
        processing/
        failed/
    logs/
        api/
        download/
    backups/
        daily/
        weekly/
```

Dateirollen pro Release-Ordner:
- firmware.bin: eigentliche Firmware (ESP32 OTA, unkomprimiert)
- firmware.sha256: SHA256 ueber das Firmware-File
- metadata.json: version, build_time, git_commit, size, hardware_version

Beispiel metadata.json:
```json
{
    "platform": "esp32",
    "hardware_version": "0.1",
    "channel": "stable",
    "version": "1.0.1",
    "file": "firmware.bin",
    "sha256": "...",
    "size_bytes": 512000,
    "compressed": false,
    "build_time_utc": "2026-05-11T09:10:11Z",
    "git_commit": "abc1234"
}
```

## Berechtigungsmodell
- rfidbox-device-ro: nur Read auf /srv/rfidbox/firmware
- rfidbox-release-rw: Write auf uploads/incoming und firmware/*/beta
- rfidbox-admin: Write auf stable, active, fallback, manifest

## Retention/Archiv-Regeln
- stable: immer 1x latest + mindestens 2 vorige Versionen im archive/
- beta: latest + die letzten 5 Betas
- manifest.history: jede Aenderung versioniert speichern

## Freigabe-Workflow
- Upload immer zuerst nach beta/archive/{version}
- Test auf einzelnen Devices (per override)
- Danach Promotion auf stable/latest
- fallback-Link zeigt auf letzte stabile Vorgaengerversion

## OTA-Strategie
ESP32:
- .bin unkomprimiert
- A/B-Fallback mit OTA-Partitionen (ota_0 / ota_1)

Betriebsmodi (parallel moeglich):
- Direktes Dev-OTA aus VS Code (schnelles Testen)
- Manifest-gesteuertes HTTP Pull-OTA fuer Fleet-Rollout

## API-Richtung (Server)
- GET /api/firmware/manifest?hardware_version=0.1&channel=stable
- GET /api/firmware/manifest?device_id=rfidbox-01
- GET /api/firmware/download/esp32/{hardware_version}/{channel}/latest.bin
- POST /api/firmware/upload/esp32/{hardware_version}

## VS Code / PlatformIO Entwicklung
- Phase 1: bestehendes `esp32dev`-Environment bleibt aktiv
- Phase 2: generische Firmware + Upload per CI/CD auf Server
- Direkte OTA-Targets fuer Entwicklung koennen parallel bestehen bleiben

## Discovery
- mDNS-Namensschema: rfidbox-{id}.local
- Kuenftiges Dashboard:
  - Status: firmware_version, hardware_version, machine_id
  - Aktionen: channel wechseln, sofortiges Update triggern

## Entscheidungen
- Keine SD-Karte
- Config persistent in NVS/Onboard-Flash
- Firmware unkomprimiert fuer ESP32 OTA
- Fleet-Prinzip gesetzt (Manifest + channels + overrides + promotion)
