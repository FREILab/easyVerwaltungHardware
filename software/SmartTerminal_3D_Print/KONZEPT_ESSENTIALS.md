# Smart Terminal 3D-Print – Konzept-Essentials

Grundlage für die Neukonzeption. Alles, was hier nicht steht, ist offen.

---

## Umfeld

Ehrenamtlich betriebener Makerspace. Zuverlässigkeit auf Industrieniveau.

- Keine ständige Betreuung vor Ort – Fehler fängt das Terminal selbst ab.
- 24/7-Betrieb, keine verlorenen Buchungen, definierter Zustand nach Stromausfall oder Netzabbruch.
- Gerätetausch durch Laien in < 10 min.
- Wechselnde, oft unerfahrene Nutzer – bedienbar ohne Einweisung, jeder Screen hat einen Ausweg.

---

## Use Case

Ein Druck startet erst nach RFID-Identifikation und Abrechnung.

1. Nutzer sliced und sendet den Job per „Upload & Print" an den gewünschten Drucker (Druckerwahl im Slicer).
2. Job landet in einer Warteschlange am Terminal, Drucker startet nicht.
3. Nutzer wählt am Terminal seinen Job, hält die RFID-Karte vor, bestätigt den Preis.
4. Terminal gibt den Job an den Drucker weiter. Abbuchung erst, wenn der Drucker angenommen hat.

### Varianten

| Fall | Kern |
|---|---|
| Persönlicher Druck | Abbuchung vom Guthaben |
| Guthaben zu gering | Fehlbetrag anzeigen, am Terminal aufladen, direkt weiter zum Druck |
| Nachdruck nach Fehldruck | „Bereits bezahlt" per PIN bestätigen, als Nachdruck markiert |
| Vereinsdruck | Bereich wählen (Vorstand, Werkstatt, Projekt …), PIN, Anrechnung auf den Bereich |
| Konto ansehen | Guthaben und letzte Transaktionen ohne Druck |
| Nutzer freischalten | Karte ohne Freigabe wird per Karte einer berechtigten Person (BV/Einweisung) freigeschaltet |
| Druckerstatus | bereit / druckt / offline für alle Drucker, immer sichtbar |
| Laufenden Druck abbrechen | offen |

### Werte

| Parameter | Wert |
|---|---|
| Lebensdauer Job in der Warteschlange | 5 min |
| Timeout RFID-Screen | 60 s |
| Job sichtbar nach Upload | ≤ 3 s |
| Reaktion nach Kartenscan | < 1 s |

---

## Kurzbeschreibung

Terminal am Druckerbereich. Nutzer senden Jobs wie gewohnt aus dem Slicer, das Terminal hält sie zurück, bis per RFID identifiziert und abgerechnet ist (Guthaben, Vereinsdruck oder Nachdruck), und gibt sie dann an den Drucker weiter. Buchungen laufen über die easyVerwaltung.

Abgekapselte Lösung ohne Abhängigkeit zu anderen Geräten oder Infrastruktur. Schnittstellen nach außen:

- easyVerwaltung-API (Nutzer, Freigaben, Guthaben, Buchungen)
- Drucker per PrusaLink
- Slicer-Upload

### Festgelegt

- Python 3, ein Prozess für UI und Logik; UI mit PySide6/QML direkt auf dem Display (`eglfs`, ohne Desktop und Browser).
- Gegenüber dem Slicer: PrusaLink-kompatibler Upload-Endpunkt je Drucker (eigener Port).
- Hostname `3d-print-server`; im Betrieb feste DHCP-Reservierung und LAN-DNS-Eintrag.

### Offen

- Speichermedium (SD oder SSD)

---

## Drucker

| Drucker | Modell | Düse | PrusaLink-Auth | Hinweis |
|---|---|---|---|---|
| Drucker 1 | MK3.5 | 0,6 mm | Benutzer/Passwort | meist `.bgcode` |
| Drucker 2a | MK4S | High Flow | Benutzer/Passwort | meist `.bgcode` |
| Drucker 2b | MK4S | 0,4 mm | Benutzer/Passwort | meist `.bgcode` |
| Drucker 3 | MK3S | 0,4 mm | API-Key | PrusaLink auf Raspberry Pi Zero, älterer API-Stand, `.gcode` |
| Drucker 4 | XL 5T | HF / non-HF | Benutzer/Passwort | bis 5 Werkzeuge – Mehrfarb-Filamentverbrauch im Preis |

Preis, Druckdauer und Filamentmenge müssen aus `.gcode` und `.bgcode` lesbar sein.

Adressen und Zugangsdaten der Drucker nur in der lokalen Terminal-Konfiguration. Beim Umstieg erhält jeder Drucker neue Zugangsdaten, die nur das Terminal kennt.

---

## Hardware (vorhandenes DevKit, keine Festlegung)

| Komponente | Details |
|---|---|
| Raspberry Pi 5 + Active Cooler | 27-W-USB-C-Netzteil |
| 5" DSI-Touchdisplay | 720×1280, kapazitiv, IPS, Optical Bonding |
| PN532 NFC-Modul | SPI |
| NeoPixel-Ring, 12 LEDs | Statusanzeige |
| Aktiver Buzzer | akustisches Feedback |
| I²C-Pegelwandler | 3,3 V ↔ 5 V für Buzzer und LED-Ring |
| microSD ≥ 32 GB | Speichermedium offen |

---

## Deployment und Wartung

Kein Docker. Das Repo ist die einzige Quelle; auf dem Pi läuft ein Klon, eingerichtet per Skript, Dienste als systemd-Units.

### Im Repo

- Anwendungscode
- `install.sh` – richtet einen frischen Pi in einem Durchlauf ein
- `update.sh` – spielt einen Tag ein, mit Rollback
- systemd-Units für Dienste und Kiosk-Browser
- Beispielkonfiguration – echte Konfiguration und Zugangsdaten nur auf dem Pi, nie in GitHub

### Ersteinrichtung

1. Raspberry Pi OS Lite per Pi Imager flashen, Netzwerk, SSH-Schlüssel und Hostname dabei setzen.
2. Per VS Code Remote-SSH vom Entwicklungsrechner verbinden.
3. `git clone`, `./install.sh` – Pakete, Python-Umgebung, Dienste, Kiosk, SPI/I²C.
4. Konfiguration eintragen (Drucker, easyVerwaltung-Zugang), Dienste starten.

### Updates

- Auf dem Pi läuft ausschließlich ein Git-Tag (z. B. `st3d-v1.2`), nie `main`.
- `./update.sh <tag>`: Tag holen, installieren, Dienste neu starten, Health-Check; bei Fehler zurück auf den vorherigen Tag.
- Update wird manuell per SSH ausgelöst, nicht automatisch.

### Betrieb ohne Betreuung

- systemd startet abgestürzte Dienste neu.
- Hardware-Watchdog startet das Gerät bei Hängern neu.
- Root-Dateisystem schreibgeschützt (Overlay), offene Buchungen auf eigener Datenpartition.
- Logs im systemd-Journal, abrufbar per SSH.

### Gerätetausch

Neuen Pi flashen, `install.sh`, Konfiguration zurückspielen. Reicht das nicht für < 10 min durch Laien: vorbereitete Ersatz-SD im Schrank.
