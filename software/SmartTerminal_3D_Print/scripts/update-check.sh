#!/usr/bin/env bash
# Wird vom Systemd-Timer stündlich aufgerufen.
# Prüft manifest.json auf dem Share und updated bei neuer Version.
set -euo pipefail

MANIFEST_URL=$(grep '^OTA_MANIFEST_URL=' /opt/smart-terminal/config/terminal.env | cut -d= -f2-)
CHANNEL=$(grep '^OTA_CHANNEL=' /opt/smart-terminal/config/terminal.env | cut -d= -f2-)
CURRENT_VERSION_FILE="/opt/smart-terminal/.version"

MANIFEST=$(curl -sf "$MANIFEST_URL")
REMOTE_VERSION=$(echo "$MANIFEST" | jq -r --arg ch "$CHANNEL" '.[$ch].version')
REMOTE_URL=$(echo "$MANIFEST"     | jq -r --arg ch "$CHANNEL" '.[$ch].url')
REMOTE_SHA=$(echo "$MANIFEST"     | jq -r --arg ch "$CHANNEL" '.[$ch].sha256')

CURRENT_VERSION=$(cat "$CURRENT_VERSION_FILE" 2>/dev/null || echo "none")

if [ "$REMOTE_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Already up to date ($CURRENT_VERSION)."
  exit 0
fi

echo "New version: $REMOTE_VERSION (current: $CURRENT_VERSION). Updating..."

TMP=$(mktemp)
curl -sf "$REMOTE_URL" -o "$TMP"
echo "$REMOTE_SHA  $TMP" | sha256sum -c -

docker load < "$TMP"
rm "$TMP"

cd /opt/smart-terminal
docker compose up -d

echo "$REMOTE_VERSION" > "$CURRENT_VERSION_FILE"
echo "Update to $REMOTE_VERSION complete."
