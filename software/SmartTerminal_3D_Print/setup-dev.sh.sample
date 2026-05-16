#!/usr/bin/env bash
# Einmalig auf einem frisch geflashten Standard-Pi-OS ausführen.
# Richtet Docker, den Dev-Boot-Screen und das Arbeitsverzeichnis ein.
#
# Ausführen via SSH direkt nach dem ersten Boot:
#   ssh MY_USER_NAME@smartterminal-dev.local \
#     "git clone https://github.com/yourorg/easyVerwaltungHardware.git ~/easyVerwaltung && \
#      bash ~/easyVerwaltung/software/SmartTerminal_3D_Print/scripts/setup-dev.sh"
set -euo pipefail

REPO_DIR="$HOME/easyVerwaltung"
PROJECT_DIR="$REPO_DIR/software/SmartTerminal_3D_Print"
INSTALL_DIR="/usr/local/lib/smart-terminal"
SERVICE_DIR="/etc/systemd/system"

echo "=== Smart Terminal Dev-Kit Setup ==="

# --- Docker ---
if ! command -v docker &>/dev/null; then
  echo "→ Docker installieren..."
  curl -fsSL https://get.docker.com | sh
  sudo usermod -aG docker "$USER"
  echo "  Docker installiert."
else
  echo "→ Docker bereits vorhanden."
fi

# --- Projektverzeichnis verlinken ---
sudo mkdir -p "$INSTALL_DIR"
sudo ln -sf "$PROJECT_DIR/scripts/dev-boot-screen.py" "$INSTALL_DIR/dev-boot-screen.py"
echo "→ dev-boot-screen.py verlinkt."

# --- Systemd-Service installieren ---
sudo cp "$PROJECT_DIR/scripts/smart-terminal-devscreen.service" "$SERVICE_DIR/"
sudo systemctl daemon-reload
sudo systemctl enable smart-terminal-devscreen.service
echo "→ dev-boot-screen Service aktiviert."

# --- docker-compose.override.yml aktivieren (Dev-Defaults) ---
# override.yml liegt bereits im Repo und wird von docker compose automatisch geladen.
echo "→ docker-compose.override.yml aktiv (Volume-Mounts, uvicorn --reload)."

echo ""
echo "✓ Setup abgeschlossen."
echo "  Verbinde dich ab jetzt so:"
echo "  ssh $USER@$(hostname).local"
echo ""
echo "  Neustart empfohlen damit der Boot-Screen beim nächsten Start erscheint."
read -rp "  Jetzt neu starten? [j/N] " answer
if [[ "${answer,,}" == "j" ]]; then
  sudo reboot
fi
