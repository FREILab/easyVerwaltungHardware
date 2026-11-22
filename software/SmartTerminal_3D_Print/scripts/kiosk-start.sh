#!/usr/bin/env bash
# Wird von labwc beim Start des Kiosk-Services ausgeführt.
# Dreht das Waveshare 5" DSI Display auf Querformat (1280x720), wartet kurz auf den
# lokalen UI-Container und startet Chromium im Vollbild-Kiosk-Modus.
set -euo pipefail

ROTATE="${DISPLAY_ROTATE:-90}"
TARGET_URL="${KIOSK_URL:-http://127.0.0.1}"

# DSI-Ausgang ermitteln (typisch DSI-1 oder DSI-2) und ins Querformat drehen
DSI_OUT=$(wlr-randr 2>/dev/null | awk '/^DSI-/{print $1; exit}' || true)
if [ -n "$DSI_OUT" ]; then
  wlr-randr --output "$DSI_OUT" --transform "$ROTATE" || true
fi

# Warten bis nginx/UI erreichbar ist (max. 60 s), damit keine Fehlerseite erscheint
for _ in $(seq 1 60); do
  if curl -sf "${TARGET_URL}/healthz" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

# Passendes Chromium-Binary finden (Bookworm: chromium-browser, Trixie: chromium)
BROWSER=$(command -v chromium || command -v chromium-browser)

exec "$BROWSER" \
  --kiosk \
  --ozone-platform=wayland \
  --enable-features=OverlayScrollbar \
  --touch-events=enabled \
  --disable-pinch \
  --overscroll-history-navigation=0 \
  --noerrdialogs \
  --disable-infobars \
  --no-first-run \
  --disable-session-crashed-bubble \
  --disable-restore-session-state \
  --disable-features=TranslateUI \
  --check-for-update-interval=31536000 \
  --autoplay-policy=no-user-gesture-required \
  --user-data-dir=/tmp/chromium-kiosk \
  "$TARGET_URL"
