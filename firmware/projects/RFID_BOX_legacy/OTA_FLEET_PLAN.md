# OTA + Fleet Plan – RFID Box (ESP32)

## Status

- **Phase 1 – abgeschlossen (2026-05-25):** Alle Nodes laufen mit ArduinoOTA + Telnet-Debug (Port 23). Jeder Node erreichbar via `<hostname>.local:23`.
- **Phase 2 – in Planung:** HTTP Pull-OTA via Proxmox-Share. Nodes prüfen selbstständig ob neue Firmware vorliegt und updaten sich ohne USB.

---

## Architektur-Entscheidungen

| Thema | Entscheidung | Begründung |
|---|---|---|
| Firmware-Typ | **Eine generische Binary** pro Channel | Kein N-fach-Build pro Release |
| Device-Identity | **NVS (Preferences)** – beim USB-Erstflash geschrieben | OTA überschreibt nur App-Partition, NVS bleibt erhalten |
| Identity-Fallback | **MAC-basierter Notfall-Hostname** (`rfidbox-<mac4>.local`) | Device bleibt per Telnet erreichbar auch wenn NVS leer |
| Fleet-Mapping | **Explizites Opt-in** in `fleet.json`, kein Default-Fallback | Kein unbeabsichtigtes Update bei fehlendem Eintrag |
| Share-Struktur | **Proxmox-Share** unter `/share/nodes/` | Passt zur bestehenden Infrastruktur (siehe Proxmox-Share.md) |
| CI-Deploy | **GitHub Actions → SCP → Proxmox** | Server hat Internet, manueller Trigger für volle Kontrolle |

---

## Share-Struktur

```
/share/nodes/
  fleet.json                        ← Node-ID → Channel + Projekt-Zuweisung (z.B. RFID_BOX_legacy)
  manifest.json                     ← Versions-/URL-Info pro Channel + Projekt
  stable/
    RFID_BOX_legacy/
      v1.0.1.bin
      v1.0.1.sha256
  beta/
    RFID_BOX_legacy/
      v1.1.0-beta.1.bin
      v1.1.0-beta.1.sha256
  dev/
    RFID_BOX_legacy/                ← Test-Builds für einzelne Nodes
      v1.1.0-dev.1.bin
      v1.1.0-dev.1.sha256
```

### `manifest.json` (Nodes)

```json
{
  "stable": {
    "RFID_BOX_legacy": { "version": "1.0.1",        "url": "/nodes/stable/RFID_BOX_legacy/v1.0.1.bin",        "sha256": "..." }
  },
  "beta": {
    "RFID_BOX_legacy": { "version": "1.1.0-beta.1", "url": "/nodes/beta/RFID_BOX_legacy/v1.1.0-beta.1.bin",  "sha256": "..." }
  },
  "dev": {
    "RFID_BOX_legacy": { "version": "1.1.0-dev.1",  "url": "/nodes/dev/RFID_BOX_legacy/v1.1.0-dev.1.bin",    "sha256": "..." }
  }
}
```

### `fleet.json`

Explizites Opt-in – nicht gelistete Nodes erhalten kein Update. Jeder Eintrag definiert Channel **und** Firmware-Projekt:

```json
{
  "nodes": {
    "xtool-01":              { "channel": "stable", "project": "easyVerwaltung"  },
    "3dprinter-01":          { "channel": "stable", "project": "easyVerwaltung"  },
    "3dprinter-02":          { "channel": "stable", "project": "easyVerwaltung"  },
    "3dprinter-03":          { "channel": "stable", "project": "easyVerwaltung"  },
    "lathe-01":              { "channel": "stable", "project": "easyVerwaltung"  },
    "lathe-02":              { "channel": "stable", "project": "easyVerwaltung"  },
    "metal-mill-01":         { "channel": "stable", "project": "RFID_BOX_legacy" },
    "metal-mill-02":         { "channel": "stable", "project": "RFID_BOX_legacy" },
    "embroiderymachine-01":  { "channel": "stable", "project": "easyVerwaltung"  },
    "cncmill-wood-01":       { "channel": "stable", "project": "easyVerwaltung"  },
    "cncmill-metal-01":      { "channel": "stable", "project": "easyVerwaltung"  },
    "3dprinter-xl-01":       { "channel": "stable", "project": "easyVerwaltung"  },
    "panel-saw-01":          { "channel": "stable", "project": "easyVerwaltung"  },
    "wood-planer-01":        { "channel": "stable", "project": "easyVerwaltung"  },
    "wood-bandsaw-01":       { "channel": "stable", "project": "easyVerwaltung"  },
    "miter-saw-01":          { "channel": "stable", "project": "easyVerwaltung"  },
    "wood-routertable-01":   { "channel": "stable", "project": "easyVerwaltung"  }
  }
}
```

**Migration Legacy → easyVerwaltung:** Eintrag in `fleet.json` ändern (`"project": "easyVerwaltung"`). Beim nächsten Check lädt das Device die neue Firmware – kein USB, kein physischer Zugriff. Rollback identisch.

**Testfall:** `metal-mill-01` auf Beta testen → `"channel": "beta"` setzen → Node zieht Testversion. Nach Abnahme wieder auf `"stable"`.

---

## Device-Flow (Firmware)

```
Trigger: sobald WiFi verbunden (nach Boot oder Reconnect) + alle 6h
  1. GET {OTA_SHARE_URL}/nodes/fleet.json
     → eigene machine_id suchen
     → nicht gefunden → kein Update (fail-safe)
  2. { channel, project } aus fleet.json
     → GET {OTA_SHARE_URL}/nodes/manifest.json
     → manifest[channel][project] → version, url, sha256
  3. Läuft bereits project == FIRMWARE_PROJECT UND version == FIRMWARE_VERSION?
     → ja  → "Firmware up to date."
     → nein → GET binary-url → sha256 prüfen → Update.writeStream() → restart()
     (gilt für Version-Update UND Projekt-Wechsel z.B. RFID_BOX_legacy → easyVerwaltung)
```

**Sicherheitslogik:**
- Kein Update wenn `currentState == RUNNING` (Maschine aktiv)
- Kein Update wenn WiFi nicht verbunden
- Kein paralleler HTTP-Request (`isHttpRequestInProgress`)

---

## Device-Identity (NVS)

Beim **USB-Erstflash** schreibt `setup()` die Build-Flags einmalig in NVS (`Preferences`, Namespace `"device"`):

| NVS-Key | Build-Flag | Beispiel |
|---|---|---|
| `machine_id` | `MACHINE_ID` | `"metal-mill-01"` |
| `machine_name` | `MACHINE_NAME` | `"metal-mill"` |
| `ota_hostname` | `OTA_HOSTNAME` | `"metal-mill-01"` |
| `auth_const` | `RFIDCARD_AUTH_CONST` | `true` |
| `cont_check` | `CONTINUOUS_SERVER_CHECK` | `false` |

**OTA überschreibt ausschließlich die App-Partition – NVS bleibt unangetastet.**

**Notfall-Hostname:** Wenn NVS kein `ota_hostname` enthält (z.B. fabrikneues Gerät oder Partition-Fehler), wird `rfidbox-<mac4>.local` gesetzt (letzten 2 Bytes der MAC). Node bleibt per Telnet erreichbar und kann manuell re-provisioniert werden.

---

## CI/CD – GitHub → Share

Tag-Format (bestehende Konvention): `rfid-box-legacy/v{version}`

| Tag | Channel | Deploy-Ziel |
|---|---|---|
| `rfid-box-legacy/v1.0.1` | stable | `/share/nodes/stable/RFID_BOX_legacy/` |
| `rfid-box-legacy/v1.1.0-beta.1` | beta | `/share/nodes/beta/RFID_BOX_legacy/` |
| `rfid-box-legacy/v1.1.0-dev.1` | dev | `/share/nodes/dev/RFID_BOX_legacy/` |

```yaml
# .github/workflows/release.yml (vereinfacht)
on:
  push:
    tags: ['rfid-box-legacy/v*']

jobs:
  deploy:
    steps:
      - name: Determine version and channel
        id: meta
        run: |
          # Tag: "rfid-box-legacy/v1.1.0-beta.1" → VERSION="1.1.0-beta.1"
          VERSION=${GITHUB_REF_NAME#rfid-box-legacy/v}
          if [[ $VERSION == *-beta* ]]; then CHANNEL=beta
          elif [[ $VERSION == *-dev* ]];  then CHANNEL=dev
          else                                 CHANNEL=stable
          fi
          echo "version=$VERSION" >> $GITHUB_OUTPUT
          echo "channel=$CHANNEL" >> $GITHUB_OUTPUT

      - name: Write secrets
        run: |
          # WiFi → secret.h (umgeht INI-Sonderzeichen-Probleme)
          cat > src/secret.h << EOF
          #define WIFI_SSID     "${{ secrets.WIFI_SSID }}"
          #define WIFI_PASSWORD "${{ secrets.WIFI_PASSWORD }}"
          EOF
          # Alle anderen Credentials → platformio.secrets.ini
          cat > platformio.secrets.ini << EOF
          [secrets]
          server_ip      = ${{ secrets.SERVER_IP }}
          auth_token     = ${{ secrets.AUTH_TOKEN }}
          ota_share_url  = ${{ secrets.OTA_SHARE_URL }}
          ota_share_user = ${{ secrets.OTA_SHARE_USER }}
          ota_share_pass = ${{ secrets.OTA_SHARE_PASS }}
          EOF

      - name: Build firmware
        run: pio run -e generic

      - name: Deploy to share
        run: |
          VERSION=${{ steps.meta.outputs.version }}
          CHANNEL=${{ steps.meta.outputs.channel }}
          scp .pio/build/generic/firmware.bin \
            proxmox:/share/nodes/$CHANNEL/RFID_BOX_legacy/v$VERSION.bin
          ssh proxmox "update-node-manifest.sh $CHANNEL RFID_BOX_legacy $VERSION"
```

**Trigger:** `git tag rfid-box-legacy/v1.0.2 && git push --tags` – kein automatischer Deploy ohne bewusste Aktion.

---

## Firmware-Änderungen (Implementierung)

### `platformio.ini`

```ini
[env]
lib_deps =
    miguelbalboa/MFRC522 @ ^1.4.11
    thijse/ArduinoLog @ ^1.1.1
    jandrassy/TelnetStream @ ^1.3.0
    bblanchon/ArduinoJson @ ^7        ; NEU

build_flags =
    ; WiFi kommt aus src/secret.h (gitignored, kein INI-Parsing-Problem)
    -DSERVER_HOST=\"${secrets.server_ip}\"
    -DAUTHENTICATION_TOKEN=\"${secrets.auth_token}\"
    -DOTA_SHARE_URL=\"${secrets.ota_share_url}\"    ; NEU
    -DOTA_SHARE_USER=\"${secrets.ota_share_user}\"  ; NEU
    -DOTA_SHARE_PASS=\"${secrets.ota_share_pass}\"  ; NEU
    -DOTA_ENABLED=1
    -DFIRMWARE_VERSION=\"1.0.0\"                    ; NEU
    -DFIRMWARE_PROJECT=\"RFID_BOX_legacy\"          ; NEU – Manifest-Key

; Generisches Build-Environment für GitHub Actions / OTA-Share
[env:generic]
; Keine device-spezifischen Flags – liest machine_id etc. aus NVS
```

### `platformio.secrets.ini` (und `.example`)

```ini
[secrets]
server_ip        = YOUR_SERVER_IP
auth_token       = YOUR_AUTH_TOKEN
ota_share_url    = http://YOUR_PROXMOX_IP/share
ota_share_user   = YOUR_SHARE_USER
ota_share_pass   = YOUR_SHARE_PASS
```

WiFi-Credentials bleiben in `src/secret.h` (gitignored) – INI-Parser haben Probleme mit Sonderzeichen wie `%` oder `(` in Passwörtern:

```cpp
// src/secret.h
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-p@ss(word%"
```

### `src/main.cpp` – neue Funktionen

**`loadDeviceConfig()`** – in `setup()` vor WiFi aufrufen:
- Schreibt Build-Flags bei erstem Boot in NVS (falls Key noch nicht existiert)
- Liest immer aus NVS → globale `g_machine_id`, `g_machine_name`, `g_ota_hostname`, `g_rfidcard_auth_const`, `g_continuous_server_check`
- Setzt MAC-Fallback-Hostname wenn NVS leer

**`checkForFirmwareUpdate()`** – in `setup()` nach OTA/Telnet, und in `loop()` alle 6h:
- Alle HTTP-Requests mit `http.setAuthorization(OTA_SHARE_USER, OTA_SHARE_PASS)`
- fleet.json → `{ channel, project }` → `manifest[channel][project]` → Versionsvergleich → HTTP-OTA via `Update.writeStream()`

Alle `MACHINE_ID`, `MACHINE_NAME`, `RFIDCARD_AUTH_CONST`-Referenzen im Code werden auf `g_machine_id.c_str()` etc. umgestellt.

---

## Verifikation

1. `pio run -e generic` – kompiliert ohne device-spezifische Flags
2. `pio run -e xtool_ota` – bestehendes Env unverändert funktionsfähig
3. Nach USB-Erstflash: Telnet → `[config] machine_id=xtool-01 (from NVS)`
4. Node nicht in fleet.json → `[OTA-Pull] xtool-01 not in fleet, skipping`
5. Node in fleet.json, gleiche Version → `[OTA-Pull] Firmware up to date`
6. Node in fleet.json, neue Version → Download → Reboot → neue Version läuft
7. NVS-Lösch-Test → Node bootet als `rfidbox-<mac4>.local`, Telnet erreichbar
