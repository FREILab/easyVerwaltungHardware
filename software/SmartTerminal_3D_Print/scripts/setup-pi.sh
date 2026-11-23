#!/usr/bin/env bash
# Idempotentes Setup-Skript für Raspberry Pi 5 (Raspberry Pi OS Lite 64-bit).
# Richtet ein:
#   - Waveshare 5" DSI LCD (D) + Hochkant-Format (720×1280) + Touch-Mapping
#   - SPI & I2C (für PN532 / Sensoren)
#   - 24/7-Härtung (Hardware-Watchdog, RAM-Logs gegen SD-Verschleiß, WLAN-PowerSave aus)
#   - Docker + Wayland-Kiosk (labwc + Chromium, ohne Desktop-Ballast)
#   - Systemd-Services für Stack & Kiosk
#
# Aufruf auf dem frisch geflashten Pi:
#   git clone https://github.com/FREILab/easyVerwaltungHardware.git ~/easyVerwaltungHardware
#   bash ~/easyVerwaltungHardware/software/SmartTerminal_3D_Print/scripts/setup-pi.sh
#
# Optionale Parameter:
#   DSI_PORT=dsi1|dsi0         (Default: dsi1)
#   DISPLAY_ROTATE=normal|180  (Default: normal = Hochkant 720×1280; bei kopfstehendem Bild 180)
set -euo pipefail

DSI_PORT="${DSI_PORT:-dsi1}"
DISPLAY_ROTATE="${DISPLAY_ROTATE:-normal}"
KIOSK_SCALE="${KIOSK_SCALE:-1.5}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
KIOSK_USER="${SUDO_USER:-$USER}"
KIOSK_UID="$(id -u "$KIOSK_USER")"
KIOSK_HOME="$(eval echo "~$KIOSK_USER")"
LIB_DIR="/usr/local/lib/smart-terminal"

if [ -f /boot/firmware/config.txt ]; then
  BOOT_CONFIG="/boot/firmware/config.txt"
else
  BOOT_CONFIG="/boot/config.txt"
fi

echo "=== SmartTerminal 3D-Print — Pi 5 Setup ==="
echo "  User:           $KIOSK_USER (UID $KIOSK_UID)"
echo "  Projekt:        $PROJECT_DIR"
echo "  DSI-Port:       $DSI_PORT (Rotation: ${DISPLAY_ROTATE}°)"
echo "  Boot-Config:    $BOOT_CONFIG"

# --- 1. Basispakete + minimaler Wayland-Kiosk (kein Desktop nötig) ---
echo "→ Pakete installieren (labwc, wlr-randr, chromium, seatd, git, jq, curl)..."
sudo apt-get update -qq
BROWSER_PKG="chromium"
if ! apt-cache show chromium >/dev/null 2>&1; then
  BROWSER_PKG="chromium-browser"
fi
sudo apt-get install -y --no-install-recommends \
  labwc wlr-randr seatd "$BROWSER_PKG" \
  fonts-dejavu-core fonts-noto-color-emoji \
  ca-certificates curl git jq iw

sudo systemctl enable --now seatd

# --- 2. Docker installieren ---
if ! command -v docker >/dev/null 2>&1; then
  echo "→ Docker installieren..."
  curl -fsSL https://get.docker.com | sh
else
  echo "→ Docker bereits installiert."
fi

# Benutzer in Hardware- & Grafikgruppen aufnehmen
for grp in docker video render input spi gpio i2c seat; do
  if getent group "$grp" >/dev/null 2>&1; then
    sudo usermod -aG "$grp" "$KIOSK_USER"
  fi
done

# --- 3. Boot-Config: Waveshare 5" DSI LCD (D) + SPI + I2C ---
echo "→ Konfiguriere Display & Schnittstellen in $BOOT_CONFIG..."
PANEL_LINE="dtoverlay=vc4-kms-dsi-waveshare-panel,5_0_inch"
if [ "$DSI_PORT" = "dsi0" ]; then
  PANEL_LINE="${PANEL_LINE},dsi0"
fi

append_if_missing() {
  local line="$1"
  local file="$2"
  grep -qxF "$line" "$file" || echo "$line" | sudo tee -a "$file" >/dev/null
}

append_if_missing "dtparam=spi=on" "$BOOT_CONFIG"
append_if_missing "dtparam=i2c_arm=on" "$BOOT_CONFIG"
append_if_missing "dtoverlay=vc4-kms-v3d" "$BOOT_CONFIG"
# Alte waveshare-Zeile ggf. sauber ersetzen
sudo sed -i '/^dtoverlay=vc4-kms-dsi-waveshare-panel/d' "$BOOT_CONFIG"
echo "$PANEL_LINE" | sudo tee -a "$BOOT_CONFIG" >/dev/null

# i2c-dev Modul dauerhaft laden, damit /dev/i2c-1 beim Boot existiert
echo "i2c-dev" | sudo tee /etc/modules-load.d/i2c-dev.conf >/dev/null
sudo modprobe i2c-dev || true

# --- 4. 24/7-Härtung: Hardware-Watchdog + RAM-Journald + WLAN PowerSave aus ---
echo "→ 24/7-Härtung aktivieren (Watchdog, RAM-Logs, WLAN PowerSave aus)..."
sudo mkdir -p /etc/systemd/system.conf.d
sudo tee /etc/systemd/system.conf.d/99-watchdog.conf >/dev/null <<'EOF'
[Manager]
RuntimeWatchdogSec=15s
RebootWatchdogSec=2min
EOF

sudo mkdir -p /etc/systemd/journald.conf.d
sudo tee /etc/systemd/journald.conf.d/99-sd-protect.conf >/dev/null <<'EOF'
[Journal]
Storage=volatile
RuntimeMaxUse=32M
EOF
sudo systemctl restart systemd-journald || true

sudo tee /etc/udev/rules.d/99-wifi-no-powersave.rules >/dev/null <<'EOF'
ACTION=="add", SUBSYSTEM=="net", KERNEL=="wlan*", RUN+="/usr/sbin/iw dev $name set power_save off"
EOF

# --- 5. labwc Touch-Mapping (bindet Touch-Koordinaten fest an gedrehtes DSI-Display) ---
echo "→ labwc-Konfiguration (Touch-Rotation + Cursor ausblenden)..."
mkdir -p "$KIOSK_HOME/.config/labwc"
cat >"$KIOSK_HOME/.config/labwc/rc.xml" <<'EOF'
<?xml version="1.0"?>
<labwc_config>
  <touch deviceName="" mouseEmulation="no" />
  <windowRules>
    <windowRule identifier="*" serverDecoration="no" />
  </windowRules>
</labwc_config>
EOF
chown -R "$KIOSK_USER:$KIOSK_USER" "$KIOSK_HOME/.config/labwc"

# --- 6. Lokale Beispiel-Configs anlegen, falls noch nicht vorhanden ---
if [ ! -f "$PROJECT_DIR/config/terminal.env" ]; then
  cp "$PROJECT_DIR/config/terminal.example.env" "$PROJECT_DIR/config/terminal.env"
  echo "→ config/terminal.env aus Vorlage erstellt (bitte bei Bedarf anpassen)."
fi
if [ ! -f "$PROJECT_DIR/config/printers.dev.json" ]; then
  cp "$PROJECT_DIR/config/printers.example.json" "$PROJECT_DIR/config/printers.dev.json"
  echo "→ config/printers.dev.json aus Vorlage erstellt (Drucker-IP/Key eintragen)."
fi

# --- 7. Kiosk- & Stack-Services installieren ---
echo "→ Systemd-Services einrichten..."
sudo mkdir -p "$LIB_DIR"
sudo install -m 0755 "$SCRIPT_DIR/kiosk-start.sh" "$LIB_DIR/kiosk-start.sh"

sed \
  -e "s|__PROJECT_DIR__|${PROJECT_DIR}|g" \
  "$SCRIPT_DIR/smart-terminal-stack.service" \
  | sudo tee /etc/systemd/system/smart-terminal-stack.service >/dev/null

sed \
  -e "s|__KIOSK_USER__|${KIOSK_USER}|g" \
  -e "s|__KIOSK_UID__|${KIOSK_UID}|g" \
  -e "s|__DISPLAY_ROTATE__|${DISPLAY_ROTATE}|g" \
  -e "s|__KIOSK_SCALE__|${KIOSK_SCALE}|g" \
  "$SCRIPT_DIR/smart-terminal-kiosk.service" \
  | sudo tee /etc/systemd/system/smart-terminal-kiosk.service >/dev/null

sudo systemctl daemon-reload
sudo systemctl disable getty@tty1.service 2>/dev/null || true
sudo systemctl enable smart-terminal-stack.service
sudo systemctl enable smart-terminal-kiosk.service

# --- 8. Docker-Images direkt lokal auf dem Pi bauen ---
echo "→ Baue Docker-Images auf dem Pi (erster Build dauert ~1-2 min)..."
(cd "$PROJECT_DIR" && sudo docker compose -f docker-compose.yml -f docker-compose.pi.yml build)

echo ""
echo "✓ Pi-5-Setup abgeschlossen!"
echo "  Nächste Schritte:"
echo "  1. Prüfe ggf. $PROJECT_DIR/config/printers.dev.json und config/terminal.env"
echo "  2. Neustart ausführen: sudo reboot"
echo "  3. Falls das Bild auf dem Kopf steht: DISPLAY_ROTATE=180 bash $0 && sudo reboot"
