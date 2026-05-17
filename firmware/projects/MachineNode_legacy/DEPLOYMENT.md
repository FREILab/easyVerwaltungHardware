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

1. Compile Project
2. Env: Configure Profile
3. Env: Build Profile
4. Upload: Current
5. Upload: Profile
6. Flash: Current Build

Empfehlung fuer den Alltag:
1. Fuer ein bestimmtes Profil Upload: Profile nutzen.
2. Fuer wiederholte Uploads ohne Profilwechsel Upload: Current nutzen.

## Erstflash Beispiel: Drucker 3 (printer3d_01)

Ziel: Node fuer Profil printer3d_01_ota erstmalig aufspielen.

1. In VS Code: Tasks: Run Task -> Upload: Profile.
2. Im Profil-Dialog printer3d_01_ota auswaehlen.
3. Falls der Node nicht automatisch erkannt wird:
   - BOOTSEL gedrueckt halten.
   - USB einstecken.
   - BOOTSEL loslassen.
   - Upload: Profile erneut ausfuehren.
4. Erfolg pruefen:
   - Task-Ausgabe zeigt den Flash-Fortschritt bis 100%.
   - Device startet neu.

Was Upload: Profile intern macht:
1. CMake Configure mit -DENV_PROFILE=<profil>
2. Build mit ninja
3. Flash via picotool load ... -fx

## Wiederholtes Update fuer denselben Node

Wenn das Profil bereits aktiv ist:

1. Tasks: Run Task -> Upload: Current

Das ist der schnellste Weg fuer iterative Firmware-Updates.

## Welche Profile gibt es?

Die Profilnamen sind kompatibel zur RFID_BOX_legacy Namenslogik (z. B. printer3d_01_ota, metal_mill_02_usb, rfidbox_ota, usw.).

Die Profilauswahl passiert ueber ENV_PROFILE im Build.

## WLAN / OTA Status

Aktuell implementiert:
1. WLAN-Connect im Firmware-Start
2. OTA-Selftest (Konfig + WLAN-Pruefung)

Noch nicht implementiert:
1. Firmware per WLAN herunterladen
2. In Update-Slot schreiben
3. Boot-Switch auf neues Image

Daher gilt momentan:
1. Erstflash per USB: ja
2. Folgeupdates per WLAN: noch nein
3. Folgeupdates per USB: ja

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
