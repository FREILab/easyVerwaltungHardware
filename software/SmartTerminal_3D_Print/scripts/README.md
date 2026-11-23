# Raspberry Pi 5 — Setup & Deployment

Kompakte Anleitung für ein neues **SmartTerminal 3D-Print** (Raspberry Pi 5 + Waveshare 5" DSI LCD (D), 24/7-Kiosk).

## 1. SD-Karte flashen (Raspberry Pi Imager)

- **Modell:** Raspberry Pi 5
- **OS:** **Raspberry Pi OS Lite (64-bit)** *(ohne Desktop)*
- **Anpassung (OS Customisation):**
  - **Hostname:** `smartterminal-dev` *(oder Geräte-Hostname)*
  - **Benutzername:** `printmaster`
  - **Passwort:** `FREILab` *(Dev-Standard – für Produktivbetrieb ggf. ändern / SSH-Key ergänzen)*
  - **WLAN / Zeitzone:** lokales Netz + `Europe/Berlin`
  - **SSH:** aktivieren → **Passwort-Authentifizierung** nutzen
  - **Raspberry Pi Connect:** **aus** (nicht aktivieren)

## 2. Hardware verbinden

- Waveshare **5inch DSI LCD (D)** per DSI-Kabel an **DSI1** (Standard-Port) anschließen.
- Offizielles **27W USB-C Netzteil** + Active Cooler verwenden.

## 3. Einmaliges Setup auf dem Pi

Nach dem ersten Boot per SSH verbinden:

```bash
ssh printmaster@smartterminal-dev.local
```

Auf dem Pi `git` installieren, Repo klonen und Setup starten:

```bash
sudo apt-get update && sudo apt-get install -y git
git clone https://github.com/FREILab/easyVerwaltungHardware.git ~/easyVerwaltungHardware
bash ~/easyVerwaltungHardware/software/SmartTerminal_3D_Print/scripts/setup-pi.sh
sudo reboot
```

`setup-pi.sh` richtet vollautomatisch ein:
- Waveshare 5" DSI Overlay (`vc4-kms-dsi-waveshare-panel,5_0_inch`) + Querformat (`1280×720`) + Touch-Rotation
- `SPI` & `I2C` aktiviert (für PN532 / Sensoren)
- 24/7-Härtung: Pi-5-Hardware-Watchdog (`15 s`), `journald` im RAM (`32 MB`), WLAN-PowerSave aus
- Docker + minimaler Wayland-Kiosk (`labwc` + `Chromium`) als `systemd`-Services (`smart-terminal-stack.service`, `smart-terminal-kiosk.service`)

## 4. Drucker & Modus anpassen (auf dem Pi)

```bash
cd ~/easyVerwaltungHardware/software/SmartTerminal_3D_Print
nano config/printers.dev.json   # Drucker-IP & PrusaLink API-Key / Digest-Login eintragen
nano config/terminal.env        # PRINTER_MODE=live setzen
docker compose -f docker-compose.yml -f docker-compose.pi.yml up -d
```

## 5. Laufendes Deployment vom Mac

Nach Änderungen & `git push` auf dem Mac reicht ein Befehl:

```bash
./scripts/deploy-pi.sh printmaster@smartterminal-dev.local
```

## 6. Kurz-Troubleshooting

- **Bild steht auf dem Kopf (Hochkant):**
  ```bash
  DISPLAY_ROTATE=180 bash ~/easyVerwaltungHardware/software/SmartTerminal_3D_Print/scripts/setup-pi.sh && sudo reboot
  ```
- **DSI-Kabel steckt an DSI0 statt DSI1:**
  ```bash
  DSI_PORT=dsi0 bash ~/easyVerwaltungHardware/software/SmartTerminal_3D_Print/scripts/setup-pi.sh && sudo reboot
  ```
- **Container-Status & Logs:**
  ```bash
  cd ~/easyVerwaltungHardware/software/SmartTerminal_3D_Print
  docker compose -f docker-compose.yml -f docker-compose.pi.yml ps
  docker compose -f docker-compose.yml -f docker-compose.pi.yml logs -f backend
  ```
- **Kiosk-Browser neu starten:**
  ```bash
  sudo systemctl restart smart-terminal-kiosk.service
  ```
