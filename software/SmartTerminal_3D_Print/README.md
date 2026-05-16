# SmartTerminal_3D_Print

Dieses Verzeichnis ist ein Entwicklungsgeruest fuer das Smart Terminal 3D-Print.

Ziel: schneller Entwicklungsstart mit einer lauffaehigen Basis, die die Architektur und den Lifecycle aus der Wiki abbildet.

## Was dieses Geruest bereits abbildet

- Drei Container gemaess Wiki:
  - backend (FastAPI)
  - ui (nginx + statisches Frontend)
  - device-agent (FastAPI als HAL-Scaffold)
- OTA-Skripte und First-Boot-Skripte als Integrationsgeruest
- Dev-Overrides fuer Remote-SSH-Workflow auf dem Pi

## Was bewusst noch Platzhalter ist

- Job-Queue, RFID-Session und Payment-Logik
- Echte PrusaLink-Emulation und Drucker-Forwarding
- Hardware-Ansteuerung fuer RFID, GPIO und I2C
- Produktionsreifer CI/CD-Lifecycle

## Konsistenz zur Wiki

Dieses Geruest folgt den Rollen und Schnittstellen aus:
- Smart Terminal: Dokumentation & Entwicklung
- Smart Terminal - 3D-Print-Server: Dokumentation & Entwicklung
- Deployment & Betrieb: Smart Terminal

Wichtig: In der Entwicklung kann lokal mit `config/printers.dev.json` gearbeitet werden.
Der Produktionspfad fuer Druckerzuordnung bleibt die server-/share-basierte Konfiguration aus der Wiki.

## Lokaler Start (Entwicklung)

1. Beispielkonfiguration kopieren:

   cp config/terminal.example.env config/terminal.env
   cp config/printers.example.json config/printers.dev.json

2. Stack starten:

   docker compose up --build

3. Verfuegbare Endpunkte:

- UI: http://localhost
- Backend Health: http://localhost:8000/api/health
- Printer API Health: http://localhost:8000/api/v1/health
- Device Agent Health: http://localhost:8081/health

## Leitlinie

Solange Features noch fehlen, sollen Stubs explizit "scaffold" markieren statt Produktionsreife zu suggerieren.
