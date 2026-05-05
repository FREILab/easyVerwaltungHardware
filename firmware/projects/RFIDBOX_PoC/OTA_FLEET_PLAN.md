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

## Detaillierte Ablage-Struktur (Server)
Empfohlener Root auf dem Server:
- /srv/rfidbox/

Vollstaendige Struktur:
```text
/srv/rfidbox/
	firmware/
		manifest/
			manifest.json
			manifest.schema.json
			manifest.history/
				2026-05-05T120000Z-manifest.json
		esp32/
			0.1/
				stable/
					latest.bin
					latest.sha256
					metadata.json
					archive/
						1.2.0/
							firmware.bin
							firmware.sha256
							metadata.json
						1.2.1/
							firmware.bin
							firmware.sha256
							metadata.json
				beta/
					latest.bin
					latest.sha256
					metadata.json
					archive/
						1.3.0-beta.1/
						1.3.0-beta.2/
		picow/
			1.0/
				stable/
					latest.bin.gz
					latest.sha256
					metadata.json
					archive/
						1.2.0/
							firmware.bin.gz
							firmware.sha256
							metadata.json
				beta/
					latest.bin.gz
					latest.sha256
					metadata.json
					archive/
						1.3.0-beta.1/
			1.1/
				stable/
				beta/
		active/
			esp32-0.1 -> /srv/rfidbox/firmware/esp32/0.1/stable/
			picow-1.0 -> /srv/rfidbox/firmware/picow/1.0/stable/
		fallback/
			esp32-0.1 -> /srv/rfidbox/firmware/esp32/0.1/archive/1.2.0/
			picow-1.0 -> /srv/rfidbox/firmware/picow/1.0/archive/1.2.0/
	uploads/
		incoming/
		processing/
		failed/
	logs/
		api/
		download/
		upload/
	backups/
		daily/
		weekly/
```

Dateirollen pro Release-Ordner:
- firmware.bin oder firmware.bin.gz: eigentliche Firmware
- firmware.sha256: SHA256 ueber das Firmware-File
- metadata.json: version, build_time, git_commit, size, compressed, min_bootloader

Beispiel metadata.json:
```json
{
	"platform": "picow",
	"hardware_version": "1.0",
	"channel": "stable",
	"version": "1.2.0",
	"file": "firmware.bin.gz",
	"sha256": "...",
	"size_bytes": 352118,
	"compressed": true,
	"build_time_utc": "2026-05-05T09:10:11Z",
	"git_commit": "abc1234",
	"min_bootloader": "1.0.0"
}
```

Berechtigungsmodell (IT):
- rfidbox-device-ro: nur Read auf /srv/rfidbox/firmware und /srv/rfidbox/firmware/manifest
- rfidbox-release-rw: Write auf /srv/rfidbox/uploads/incoming und /srv/rfidbox/firmware/*/beta
- rfidbox-admin: Write auf stable, active, fallback, manifest
- Kein Device-Write auf Serverablage

Retention/Archiv-Regeln:
- stable: immer 1x latest + mindestens 2 vorige Versionen im archive/
- beta: latest + die letzten 5 Betas
- manifest.history: jede Aenderung versioniert speichern
- logs: 30 Tage lokal, danach optional zentrales Logsystem

Freigabe-Workflow (kurz):
- Upload geht immer zuerst nach beta/archive/{version}
- Smoke-Test auf Einzelknoten (per override)
- Danach Promotion durch Kopie/Link auf stable/latest
- fallback-Link zeigt auf letzte stabile Vorgaengerversion

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
