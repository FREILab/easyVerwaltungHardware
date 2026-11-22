# SmartTerminal 3D-Print

Touch-Terminal (Raspberry Pi 5 + 5" DSI 1280×720) als PrusaLink-Broker und Abrechnungsterminal zwischen PrusaSlicer, Prusa-Druckerflotte (MINI, MK3S, MK3.5, MK4, XL) und **easyVerwaltung**.

## Architektur (Blockebene)

```
PrusaSlicer (PC) ── :8101..:8104 ──▶ [ ui / nginx ] ──(X-Printer-Slot)──▶ [ backend :8000 ]
Touch 1280×720   ── :80 (Kiosk)  ──▶ [ ui / nginx ] ──(/api/*)──────────▶   FastAPI + SQLite (WAL)
PN532/LED/Buzzer ── SPI / GPIO   ──▶ [ device-agent :8081 ] ◀─ WS/REST ─▶   G-Code/BGCode-Parser
                                                                              │              │
                                                                      easyVerwaltung    PrusaLink
                                                                      (Payment API)     (Drucker)
```

- **`backend`**: PrusaLink-Emulation, `.gcode`/`.bgcode`-Parser (Gewicht, Zeit, Preis, Vorschau), SQLite-Queue (WAL-Mode), Abrechnung (`easyVerwaltung`), Weiterleitung an echte Drucker.
- **`ui`**: Kiosk-Oberfläche (`1280×720` Querformat) + `nginx`-Proxy für `/api/*` und die Drucker-Ports `:8101..:8104`.
- **`device-agent`**: Hardware-Abstraktion (PN532 NFC über SPI, WS2812 LED-Ring, Buzzer). Läuft im Dev-Modus als Mock.

## Ports

| Port | Zweck |
|---|---|
| `80` | Touch-UI (Kiosk) + `/api/*` |
| `8101`–`8104` | PrusaLink-Emulation für Drucker-Slot 1–4 (`<Pi-IP>:8101` im PrusaSlicer) |
| `8000` | Backend direkt (Dev/Debug) |
| `8081` | Device-Agent direkt (Dev/Debug) |

## Ablauf & Preislogik

1. Nutzer sendet Job per PrusaSlicer an `<Terminal-IP>:810X` → Job landet für 5 min in der Warteschlange.
2. Am Terminal Job wählen → RFID-Karte vorhalten (`POST /api/service/payment/authorize`).
3. Bei Bestätigung: Drucker-Status (`IDLE`) prüfen → Abbuchen (`POST /api/service/payment/charge` mit `Idempotency-Key`) → Datei im Hintergrund an den Drucker streamen (kein HTTP-Timeout bei großen Dateien) → Druck starten.
4. Sonderfälle im Modell vorgesehen: **Vereinsdruck** (Bereich aus `GET /api/service/metadata` + PIN) und **Bereits bezahlt** (Nachdruck nach Fehldruck + PIN, lokal protokolliert). Guthaben-Aufladung und BV-Freigabe sind über Feature-Flags vorbereitet.
5. **Preis:** `total filament cost` aus `.gcode`/`.bgcode`, abgesichert über Untergrenze/Fallback (`filament_used_g × Preis/g`).

## Schnellstart

### 1. Konfiguration anlegen
```bash
cp config/terminal.example.env config/terminal.env
cp config/printers.example.json config/printers.dev.json
```

### 2. Starten mit Docker (Mac oder Pi Dev ohne Hardware)
```bash
docker compose up --build
```

### 3. Starten auf dem Raspberry Pi 5 (mit echter Hardware)
```bash
docker compose -f docker-compose.yml -f docker-compose.pi.yml up -d --build
```

### 4. Health-Endpunkte
- UI: `http://localhost` (Health: `http://localhost/healthz`)
- Backend: `http://localhost:8000/api/health`
- Device-Agent: `http://localhost:8081/health`

## Umsetzungsplan

- [x] **Phase 0:** Architektur, Compose-Trennung (Mac/Pi), nginx-Proxy, Log-Rotation, Healthchecks
- [x] **Phase 1:** PrusaLink-Emulation, `.gcode`/`.bgcode`-Parser, SQLite-Queue, Touch-UI (`1280×720`) + Mocks
- [x] **Phase 2:** Echter PrusaLink-Client (Status, Upload, Start) – live verbunden mit Prusa MINI
- [ ] **Phase 3:** Echte `easyVerwaltung`-Anbindung (`authorize`, `charge`, `cancel`, `metadata`)
- [ ] **Phase 4:** Pi-5-Hardware im `device-agent` (PN532, WS2812, Buzzer)
- [ ] **Phase 5:** Kiosk-Betrieb, Ausfalltests (Netz/Drucker/Strom), OTA-Rollout
