#!/usr/bin/env bash
# Schnelles Dev-Deployment vom Mac auf den Raspberry Pi per SSH.
# Zieht den aktuellen Git-Stand auf dem Pi und startet die Container neu.
#
# Aufruf:
#   ./scripts/deploy-pi.sh pi@smartterminal-dev.local
#   ./scripts/deploy-pi.sh marius@192.168.1.50
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: $0 <user@pi-host> [remote-repo-dir]"
  exit 1
fi

TARGET="$1"
REMOTE_REPO="${2:-~/easyVerwaltungHardware}"
REMOTE_APP="${REMOTE_REPO}/software/SmartTerminal_3D_Print"

echo "→ Deploy nach ${TARGET} (${REMOTE_APP})..."
ssh "$TARGET" bash -s -- "$REMOTE_REPO" "$REMOTE_APP" <<'EOF'
set -euo pipefail
REPO_DIR="$1"
APP_DIR="$2"
# Tilde expandieren
REPO_DIR="${REPO_DIR/#\~/$HOME}"
APP_DIR="${APP_DIR/#\~/$HOME}"

cd "$REPO_DIR"
git pull --ff-only
cd "$APP_DIR"
docker compose -f docker-compose.yml -f docker-compose.pi.yml up -d --build
docker compose -f docker-compose.yml -f docker-compose.pi.yml ps
EOF
echo "✓ Deployment abgeschlossen."
