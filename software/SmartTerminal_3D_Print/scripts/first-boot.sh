#!/usr/bin/env bash
# Läuft einmalig als Systemd-Service nach Pi Imager's firstrun.sh.
# Shared Secrets (API_TOKEN etc.) kommen aus /etc/smart-terminal/base.env (im Image gebacken).
# Gerätespezifische Werte (TERMINAL_ID, Drucker-IPs) werden per Touch-UI übergeben
# und als Umgebungsvariablen von diesem Service erwartet.
set -euo pipefail

BASE_ENV="/etc/smart-terminal/base.env"
INSTALL_DIR="/opt/smart-terminal"

resolve_artifact_url() {
  local manifest_url="$1"
  local artifact_url="$2"
  local manifest_origin manifest_dir

  if [[ "$artifact_url" =~ ^https?:// ]]; then
    echo "$artifact_url"
    return 0
  fi

  manifest_origin=$(printf '%s\n' "$manifest_url" | sed -E 's#^(https?://[^/]+).*$#\1#')

  if [[ "$artifact_url" == /* ]]; then
    echo "${manifest_origin}${artifact_url}"
    return 0
  fi

  manifest_dir=$(printf '%s\n' "$manifest_url" | sed -E 's#^(https?://.*/)[^/]*$#\1#')
  echo "${manifest_dir}${artifact_url}"
}

if [ -f "$BASE_ENV" ]; then
  CONFIG_SOURCE="$BASE_ENV"
else
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
cat "$CONFIG_SOURCE" > "$INSTALL_DIR/config/terminal.env"
echo "TERMINAL_ID=${TERMINAL_ID}" >> "$INSTALL_DIR/config/terminal.env"
# Drucker-IPs kommen zur Laufzeit aus printers.json auf dem Proxmox-Share — nicht lokal konfiguriert.

# Manifest-URL und Kanal aus zusammengeführter Config lesen
MANIFEST_URL=$(grep '^OTA_MANIFEST_URL=' "$INSTALL_DIR/config/terminal.env" | cut -d= -f2-)
CHANNEL=$(grep '^OTA_CHANNEL=' "$INSTALL_DIR/config/terminal.env" | cut -d= -f2-)

# Image vom Share holen
MANIFEST=$(curl -sf "$MANIFEST_URL")
IMAGE_URL=$(echo "$MANIFEST" | jq -r --arg ch "$CHANNEL" '.[$ch].url')
IMAGE_SHA=$(echo "$MANIFEST" | jq -r --arg ch "$CHANNEL" '.[$ch].sha256')
RESOLVED_IMAGE_URL=$(resolve_artifact_url "$MANIFEST_URL" "$IMAGE_URL")

echo "Lade Image: $RESOLVED_IMAGE_URL"
TMP=$(mktemp)
curl -sf "$RESOLVED_IMAGE_URL" -o "$TMP"
echo "$IMAGE_SHA  $TMP" | sha256sum -c -

docker load < "$TMP"
rm -f "$TMP"

# docker-compose.yml aus dem Image-Verzeichnis ins Install-Dir kopieren
cp /boot/docker-compose.yml "$INSTALL_DIR/docker-compose.yml" 2>/dev/null || true

# Update-Skript bereitstellen (Skelett-freundlich mit mehreren Quellen)
mkdir -p "$INSTALL_DIR/scripts"
if [ -f /usr/local/lib/smart-terminal/update-check.sh ]; then
  cp /usr/local/lib/smart-terminal/update-check.sh "$INSTALL_DIR/scripts/update-check.sh"
elif [ -f "$(dirname "$0")/update-check.sh" ]; then
  cp "$(dirname "$0")/update-check.sh" "$INSTALL_DIR/scripts/update-check.sh"
fi

if [ -f "$INSTALL_DIR/scripts/update-check.sh" ]; then
  chmod +x "$INSTALL_DIR/scripts/update-check.sh"
fi

# Systemd-Timer für stündliche Updates einrichten
if [ -f /usr/local/lib/smart-terminal/smart-terminal-update.service ] && [ -f /usr/local/lib/smart-terminal/smart-terminal-update.timer ]; then
  cp /usr/local/lib/smart-terminal/smart-terminal-update.service /etc/systemd/system/
  cp /usr/local/lib/smart-terminal/smart-terminal-update.timer   /etc/systemd/system/
elif [ -f "$(dirname "$0")/smart-terminal-update.service" ] && [ -f "$(dirname "$0")/smart-terminal-update.timer" ]; then
  cp "$(dirname "$0")/smart-terminal-update.service" /etc/systemd/system/
  cp "$(dirname "$0")/smart-terminal-update.timer"   /etc/systemd/system/
else
  cat >/etc/systemd/system/smart-terminal-update.service <<'EOF'
[Unit]
Description=Smart Terminal OTA Update Check
After=docker.service network-online.target
Wants=network-online.target

[Service]
Type=oneshot
ExecStart=/opt/smart-terminal/scripts/update-check.sh
EOF

  cat >/etc/systemd/system/smart-terminal-update.timer <<'EOF'
[Unit]
Description=Run Smart Terminal OTA update check hourly

[Timer]
OnBootSec=5min
OnUnitActiveSec=1h
Persistent=true

[Install]
WantedBy=timers.target
EOF
fi

systemctl daemon-reload
systemctl enable --now smart-terminal-update.timer

# Stack starten
cd "$INSTALL_DIR"
docker compose up -d

# Diesen Service deaktivieren — läuft nie wieder
systemctl disable smart-terminal-firstboot.service || true
rm -f /etc/systemd/system/smart-terminal-firstboot.service

echo "Provisioning abgeschlossen. Neustart..."
reboot
