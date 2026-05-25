# easyVerwaltung Hardware

Firmware und Dokumentation für die **easyVerwaltung**-Hardware des FREILab. Das System steuert Maschinenzugang, Nutzungsprotokollierung und Sicherheit via RFID-Authentifizierung gegen das easyVerwaltung-Backend.

---

## Hardware-Übersicht

### RFID-Box — Maschinenfreigabe

| Projekt | Hardware | API | Status |
|---|---|---|---|
| [RFID_BOX_legacy](firmware/projects/RFID_BOX_legacy/) | ESP32 Dev1 | Legacy | Stabil — [v1.0.0](https://github.com/FREILab/easyVerwaltungHardware/releases/tag/rfid-box-legacy%2Fv1.0.0) |
| [RFID_BOX_easyVerwaltung](firmware/projects/RFID_BOX_easyVerwaltung/) | ESP32 Dev1 | easyVerwaltung | Getestet — Server noch nicht in Production |

### Tür-Steuerung

| Projekt | Hardware | API | Status |
|---|---|---|---|
| [DOOR_legacy](firmware/projects/DOOR_legacy/) | Arduino UNO R4 WiFi | Legacy | Scheduled for Deletion |
| [DOOR_legacy_ESPArdu](firmware/projects/DOOR_legacy_ESPArdu/) | RobotDyn UNO+WiFi R3 (ATmega328P + ESP8266) | Legacy | Stabil — [v1.0.0](https://github.com/FREILab/easyVerwaltungHardware/releases/tag/door-legacy-espardu%2Fv1.0.0) |
| [DOOR_easyVerwaltung](firmware/projects/DOOR_easyVerwaltung/) | Arduino UNO R4 WiFi | easyVerwaltung | In Entwicklung |

### Lasersaur HMI

| Projekt | Hardware | API | Status |
|---|---|---|---|
| [LASERSAUR_HMI_legacy](firmware/projects/LASERSAUR_HMI_legacy/) | Arduino Mega + Ethernet-Shield | Legacy | Stabil (nicht mit neuem Server getestet) — wird durch ESPArdu ersetzt |
| [LASERSAUR_HMI_ESPArdu](firmware/projects/LASERSAUR_HMI_ESPArdu/) | Mega2560 WiFi R3 (ATmega2560 + ESP8266) | Legacy | Implementiert, noch nicht getestet |
| [LASERSAUR_HMI_easyVerwaltung](firmware/projects/LASERSAUR_HMI_easyVerwaltung/) | Arduino Giga R1 | easyVerwaltung | In Entwicklung |

### Machine Node

| Projekt | Hardware | API | Status |
|---|---|---|---|
| [MachineNode_legacy](firmware/projects/MachineNode_legacy/) | ESP32-S3-WROOM-1-N16R8 | Legacy | Stabil |
| [MachineNode_easyVerwaltung](firmware/projects/MachineNode_easyVerwaltung/) | ESP32-S3-WROOM-1-N16R8 | easyVerwaltung | Geplant |

---

## Repository-Struktur

```
firmware/
  projects/          # Firmware-Targets (je ein PlatformIO-Projekt)
  shared/
    easyAPI/         # Gemeinsame Backend-Kommunikation (HTTP/JSON)
```

Jedes Projekt in `projects/` enthält eine eigene `README.md` mit Hardware-Details, DIP-Belegung, Flash-Anleitung und Debugging-Hinweisen.

---

## Einrichtung

### Voraussetzungen

- Python ≥ 3.8
- PlatformIO Core (`pip install platformio`)

### Credentials

Jedes Projekt benötigt eine `platformio.secrets.ini` (nicht in Git). Vorlage liegt als `.example`-Datei bei:

```bash
cp platformio.secrets.ini.example platformio.secrets.ini
# Datei befüllen, dann:
pio run
```

Details zu den Feldern stehen in der jeweiligen Projekt-README.
