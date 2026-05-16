#!/usr/bin/env bash
# Wird vom Systemd-Timer stündlich aufgerufen.
# Prüft manifest.json auf dem Share und updated bei neuer Version.
set -euo pipefail

MANIFEST_URL=$(grep '^OTA_MANIFEST_URL=' /opt/smart-terminal/config/terminal.env | cut -d= -f2-)
CHANNEL=$(grep '^OTA_CHANNEL=' /opt/smart-terminal/config/terminal.env | cut -d= -f2-)
CURRENT_VERSION_FILE="/opt/smart-terminal/.version"

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

if [ -z "$MANIFEST_URL" ] || [ -z "$CHANNEL" ]; then
  echo "ERROR: OTA_MANIFEST_URL oder OTA_CHANNEL fehlt in /opt/smart-terminal/config/terminal.env"
  exit 1
fi

MANIFEST=$(curl -sf "$MANIFEST_URL")
REMOTE_VERSION=$(echo "$MANIFEST" | jq -r --arg ch "$CHANNEL" '.[$ch].version')
REMOTE_URL=$(echo "$MANIFEST"     | jq -r --arg ch "$CHANNEL" '.[$ch].url')
REMOTE_SHA=$(echo "$MANIFEST"     | jq -r --arg ch "$CHANNEL" '.[$ch].sha256')
RESOLVED_REMOTE_URL=$(resolve_artifact_url "$MANIFEST_URL" "$REMOTE_URL")

CURRENT_VERSION=$(cat "$CURRENT_VERSION_FILE" 2>/dev/null || echo "none")

if [ "$REMOTE_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Already up to date ($CURRENT_VERSION)."
  exit 0
fi

echo "New version: $REMOTE_VERSION (current: $CURRENT_VERSION). Updating..."

TMP=$(mktemp)
curl -sf "$RESOLVED_REMOTE_URL" -o "$TMP"
echo "$REMOTE_SHA  $TMP" | sha256sum -c -

docker load < "$TMP"
rm "$TMP"

cd /opt/smart-terminal
docker compose up -d

echo "$REMOTE_VERSION" > "$CURRENT_VERSION_FILE"
echo "Update to $REMOTE_VERSION complete."
