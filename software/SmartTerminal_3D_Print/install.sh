#!/usr/bin/env bash
# Richtet den Pi für das Smart Terminal 3D-Print ein. Mehrfach ausführbar.
# Aufruf aus dem geklonten Repo:  ./install.sh
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(git -C "$APP_DIR" rev-parse --show-toplevel)"
SERVICE_USER="st3d"
CONFIG_DIR="/etc/st3d"

log() { printf '\n==> %s\n' "$*"; }

if [[ $EUID -eq 0 ]]; then
    echo "Nicht als root starten – das Skript ruft sudo selbst auf." >&2
    exit 1
fi

if [[ "$(uname -m)" != "aarch64" ]]; then
    echo "Warnung: kein aarch64-System ($(uname -m)). Gedacht für Raspberry Pi OS 64 bit." >&2
fi

log "Pakete installieren"
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    git python3 python3-venv python3-pip \
    libegl1 libgles2 libgbm1 libdrm2 libinput10 libmtdev1 libxkbcommon0 \
    libfontconfig1 libdbus-1-3 fonts-dejavu-core

python3 - <<'EOF'
import sys
if sys.version_info < (3, 11):
    sys.exit(f"Python >= 3.11 nötig, gefunden {sys.version.split()[0]}")
EOF

log "SPI und I2C aktivieren"
if command -v raspi-config >/dev/null; then
    sudo raspi-config nonint do_spi 0
    sudo raspi-config nonint do_i2c 0
else
    echo "raspi-config fehlt – übersprungen."
fi

log "Dienstbenutzer $SERVICE_USER"
if ! id "$SERVICE_USER" >/dev/null 2>&1; then
    sudo useradd --system --no-create-home --shell /usr/sbin/nologin "$SERVICE_USER"
fi
for group in video render input spi i2c gpio; do
    if getent group "$group" >/dev/null; then
        sudo usermod -aG "$group" "$SERVICE_USER"
    fi
done

if ! sudo -u "$SERVICE_USER" test -r "$APP_DIR/src/st3d/__main__.py"; then
    echo "Fehler: $SERVICE_USER kann $APP_DIR nicht lesen. Repo nach /opt klonen (siehe README)." >&2
    exit 1
fi

# git describe für die Versionsanzeige läuft als $SERVICE_USER auf einem fremden Repo.
if ! git config --system --get-all safe.directory 2>/dev/null | grep -qx "$REPO_DIR"; then
    sudo git config --system --add safe.directory "$REPO_DIR"
fi

log "Python-Umgebung"
if [[ ! -d "$APP_DIR/.venv" ]]; then
    python3 -m venv "$APP_DIR/.venv"
fi
"$APP_DIR/.venv/bin/pip" install --upgrade pip
if ! "$APP_DIR/.venv/bin/pip" install -r "$APP_DIR/requirements.txt"; then
    echo "PySide6 ließ sich nicht installieren. Die aarch64-Pakete brauchen Raspberry Pi OS auf Basis Debian 13 (trixie)." >&2
    exit 1
fi

log "Konfiguration"
sudo install -d -m 0750 -o root -g "$SERVICE_USER" "$CONFIG_DIR"
if [[ ! -f "$CONFIG_DIR/config.toml" ]]; then
    sudo install -m 0640 -o root -g "$SERVICE_USER" "$APP_DIR/config.example.toml" "$CONFIG_DIR/config.toml"
    echo "Angelegt: $CONFIG_DIR/config.toml"
else
    echo "Vorhanden, unverändert: $CONFIG_DIR/config.toml"
fi

log "systemd-Dienst"
sed "s|@APP_DIR@|$APP_DIR|g" "$APP_DIR/systemd/st3d.service" \
    | sudo tee /etc/systemd/system/st3d.service >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable st3d.service
sudo systemctl restart st3d.service

log "Fertig"
systemctl --no-pager --lines=5 status st3d.service || true
echo
echo "Logs:  journalctl -u st3d -f"
