#!/usr/bin/env bash
# Läuft einmalig als Systemd-Service nach Pi Imager's firstrun.sh.
# Shared Secrets (API_TOKEN etc.) kommen aus /etc/smart-terminal/base.env (im Image gebacken).
# Gerätespezifische Werte (TERMINAL_ID, Drucker-IPs) werden per Touch-UI übergeben
# und als Umgebungsvariablen von diesem Service erwartet.
set -euo pipefail

BASE_ENV="/etc/smart-terminal/base.env"
INSTALL_DIR="/opt/smart-terminal"

if [ ! -f "$BASE_ENV" ]; then
  echo "ERROR: $BASE_ENV nicht gefunden — Image korrekt gebaut?"
  exit 1
fi

if [ -z "${TERMINAL_ID:-}" ]; then
  echo "ERROR: TERMINAL_ID nicht gesetzt — wurde die First-Boot-UI abgeschlossen?"
  exit 1
fi

echo "Provisioning Smart Terminal (3D-Print Server) als ${TERMINAL_ID}..."

# Shared + gerätespezifische Config zusammenführen
mkdir -p "$INSTALL_DIR/config"
cat "$BASE_ENV" > "$INSTALL_DIR/config/terminal.env"
echo "TERMINAL_ID=${TERMINAL_ID}" >> "$INSTALL_DIR/config/terminal.env"
# Drucker-IPs kommen zur Laufzeit vom easyVerwaltung-Server — nicht lokal konfiguriert.

# Manifest-URL und Kanal aus zusammengeführter Config lesen
MANIFEST_URL=$(grep '^OTA_MANIFEST_URL=' "$INSTALL_DIR/config/terminal.env" | cut -d= -f2-)
CHANNEL=$(grep '^OTA_CHANNEL=' "$INSTALL_DIR/config/terminal.env" | cut -d= -f2-)

# Image vom Share holen
MANIFEST=$(curl -sf "$MANIFEST_URL")
IMAGE_URL=$(echo "$MANIFEST" | jq -r --arg ch "$CHANNEL" '.[$ch].url')
IMAGE_SHA=$(echo "$MANIFEST" | jq -r --arg ch "$CHANNEL" '.[$ch].sha256')

echo "Lade Image: $IMAGE_URL"
TMP=$(mktemp)
curl -sf "http://$(grep '^OTA_MANIFEST_URL=' "$INSTALL_DIR/config/terminal.env" | cut -d= -f2- | cut -d/ -f1-3 | sed 's|http://||')${IMAGE_URL}" -o "$TMP"
echo "$IMAGE_SHA  $TMP" | sha256sum -c -

docker load < "$TMP"
rm -f "$TMP"

# docker-compose.yml aus dem Image-Verzeichnis ins Install-Dir kopieren
cp /boot/docker-compose.yml "$INSTALL_DIR/docker-compose.yml" 2>/dev/null || true

# Systemd-Timer für stündliche Updates einrichten
cp /usr/local/lib/smart-terminal/smart-terminal-update.service /etc/systemd/system/
cp /usr/local/lib/smart-terminal/smart-terminal-update.timer   /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now smart-terminal-update.timer

# Stack starten
cd "$INSTALL_DIR"
docker compose up -d

# Diesen Service deaktivieren — läuft nie wieder
systemctl disable smart-terminal-firstboot.service
rm -f /etc/systemd/system/smart-terminal-firstboot.service

echo "Provisioning abgeschlossen. Neustart..."
reboot
