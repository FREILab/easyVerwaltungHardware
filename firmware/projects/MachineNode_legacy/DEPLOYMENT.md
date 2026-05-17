# Deployment Guide (MachineNode_legacy)

Diese Anleitung beschreibt den aktuellen Stand fuer Deployment in diesem Projekt.

## Kurzfassung

1. Erstes Deployment auf einen Node erfolgt per USB.
2. Aktuell erfolgen auch weitere Firmware-Updates per USB.
3. WLAN ist bereits fuer Verbindung und OTA-Selftest vorbereitet, aber ein echter OTA-Download-und-Flash-Flow ist noch nicht implementiert.

## Voraussetzungen

1. Pico 2 W ist angeschlossen.
2. VS Code Tasks sind vorhanden in .vscode/tasks.json.
3. Lokale Konfiguration ist gesetzt in env.local.cmake (aus env.local.cmake.example abgeleitet).

## Wichtige Tasks

### USB-Methode (aktuell aktiv)
1. **Upload USB: Profile** - Konfiguriert Profil, kompiliert, flashed per USB
2. **Upload USB: Current** - Flashed aktuelle Kompilation per USB (schneller)

### OTA-Methode (für später vorbereitet)
3. **Upload OTA: Profile** - Noch nicht implementiert, zeigt Info-Meldung

### Hilfs-Tasks
4. **Compile Project** - Nur kompilieren (Ctrl+Shift+B)
5. **Env: Configure Profile** - CMake konfigurieren
6. **Env: Build Profile** - CMake + Build in einem Schritt
7. **Flash: Current Build** - Nur Flash, Kompilation wird nicht verändert

Empfehlung fuer den Alltag:
1. Fuer ein bestimmtes Profil: **Upload USB: Profile** nutzen
2. Fuer wiederholte Uploads desselben Profils: **Upload USB: Current** nutzen

## Erstflash Beispiel: Drucker 3 (printer3d_01)

Ziel: Node fuer Profil printer3d_01_ota erstmalig aufspielen.

1. In VS Code: Tasks: Run Task -> **Upload USB: Profile**
2. Im Profil-Dialog **printer3d_01_ota** auswaehlen
3. Falls der Node nicht automatisch erkannt wird:
   - BOOTSEL gedrueckt halten
   - USB einstecken
   - BOOTSEL loslassen
   - Upload USB: Profile erneut ausfuehren
4. Erfolg pruefen:
   - Task-Ausgabe zeigt Flash-Fortschritt bis 100%
   - Device startet neu und verbindet sich mit WLAN

Was Upload USB: Profile intern macht:
1. CMake Configure mit -DENV_PROFILE=printer3d_01_ota
2. Build mit ninja
3. Flash via picotool load ... -fx

## Wiederholtes Update fuer denselben Node

Wenn das Profil bereits aktiv ist und du keine Konfiguration ändern willst:

1. Tasks: Run Task -> **Upload USB: Current**

Das ist schneller, da CMake nicht nötig ist. Ideal für Bugfixes und iterative Entwicklung.

## Welche Profile gibt es?

Die Profilnamen folgen der RFID_BOX_legacy Namenslogik:
- **printer3d_01_ota, printer3d_02_ota, printer3d_03_ota**
- **lathe_01_ota, lathe_02_ota**
- **metal_mill_01_ota, metal_mill_02_ota**
- **cncmill_wood_ota, cncmill_metal_ota**
- **rfidbox_ota**
- ... und weitere (Picker-Dialog zeigt alle Optionen)

Die Profilauswahl passiert ueber ENV_PROFILE im Build-Schritt.

**Hinweis:** Alle verfügbaren Profile sind OTA-fähig (_ota), da das Projekt auf WLAN-Updates ausgerichtet ist.
Für lokale Tests via USB ist das _ota-Profil weiterhin nutzbar (OTA-Flag ist in der Firmware aktiv, aber Update lädt noch per USB).

## WLAN / OTA Status

### Aktuell implementiert:
1. **WLAN-Connect im Firmware-Start** - Verbindung zur WiFi-SSID
2. **OTA-Selftest** - Prüfung ob Konfiguration + WiFi valide sind
3. **OTA-Flag pro Profil** - OTA_ENABLED setzt LED-Blink-Rate und kennzeichnet Node als OTA-ready

### Noch nicht implementiert:
1. **HTTP-Download** - Firmware per WLAN vom Server laden
2. **Partition-Management** - Inaktive Partition für neues Image vorbereiten
3. **Boot-Switch** - Auf neue Firmware umschalten und aktivieren
4. **Rollback-Mechanismus** - Fallback bei fehlerhaftem Update

### Deployment aktuell:
| Methode | Verfügbar | Verwendung |
|---------|-----------|------------|
| **USB** | ✅ Ja | Erstes Deployment + alle Updates (via Upload USB: Profile/Current) |
| **OTA (WLAN)** | ❌ Nein | Geplant für später (via Upload OTA: Profile) |

### Roadmap OTA:
1. HTTP-Server für Firmware-Images (zentral oder Edge-basiert)
2. Sichere Download + Signatur-Verifikation
3. Partition-Handling auf Pico 2 W Flash
4. Boot-ROM oder Boot-Loader Anpassung
5. Rollback bei Fehler

## Troubleshooting

### No accessible RP-series devices in BOOTSEL mode were found

1. Node in BOOTSEL bringen (Taste halten beim Einstecken).
2. Upload erneut starten.

### WiFi verbunden nicht moeglich (wifi_connected=0)

1. env.local.cmake vorhanden?
2. WIFI_SSID und WIFI_PASSWORD korrekt gesetzt?
3. Danach neu bauen und neu flashen.

### Falsches Profil geflasht

1. Upload: Profile erneut starten.
2. Richtiges ENV_PROFILE waehlen.

### Bei Upload: Profile passiert nichts

1. Das kann am VS Code Input-Dialog liegen.
2. Dann zuerst Env: Configure Profile ausfuehren und danach Upload: Current.

## Dateiuebersicht

1. Build- und Profil-Logik: CMakeLists.txt
2. Laufzeitkonfig (generiert): build/generated/firmware_config.h
3. Lokale Secrets/Vorgaaben: env.local.cmake
4. Task-Steuerung: .vscode/tasks.json
