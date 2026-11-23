#!/usr/bin/env bash
# Wird von labwc beim Start des Kiosk-Services ausgeführt.
# Betreibt das Waveshare 5" DSI Display im nativen Hochkant-Format (720x1280), setzt
# HiDPI-Skalierung (1,5x = 480x854 logisch) und startet Chromium im Vollbild-Kiosk-Modus.
set -euo pipefail

# normal (0) oder 180 für Hochkant (je nach Einbaurichtung des DSI-Kabels)
ROTATE="${DISPLAY_ROTATE:-normal}"
[ "$ROTATE" = "0" ] && ROTATE="normal"
SCALE="${KIOSK_SCALE:-1.5}"
TARGET_URL="${KIOSK_URL:-http://127.0.0.1/?kiosk=1}"

# DSI-Ausgang ermitteln (Pi 5: typisch DSI-2 oder DSI-1), drehen und Touch darauf mappen
DSI_OUT=$(wlr-randr 2>/dev/null | awk '/^DSI-/{print $1; exit}' || true)
TOUCH_NAME=$(awk -F'"' '/Name=.*[Tt]ouch/{print $2; exit}' /proc/bus/input/devices 2>/dev/null || true)
if [ -n "$DSI_OUT" ]; then
  wlr-randr --output "$DSI_OUT" --transform "$ROTATE" || true
  mkdir -p "$HOME/.config/labwc"
  cat >"$HOME/.config/labwc/rc.xml" <<EOF
<?xml version="1.0"?>
<labwc_config>
  <touch deviceName="${TOUCH_NAME}" mapToOutput="${DSI_OUT}" mouseEmulation="yes" />
  <windowRules>
    <windowRule identifier="*" serverDecoration="no" />
  </windowRules>
</labwc_config>
EOF
  labwc --reconfigure || true
fi

# Warten bis UI + Backend erreichbar sind (max. 60 s), damit keine 502/Fehlerseite erscheint
HEALTH_URL="${TARGET_URL%%\?*}"
HEALTH_URL="${HEALTH_URL%/}/api/health"
for _ in $(seq 1 60); do
  if curl -sf "$HEALTH_URL" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

# Passendes Chromium-Binary finden (Bookworm: chromium-browser, Trixie: chromium)
BROWSER=$(command -v chromium || command -v chromium-browser)

# Translate-Popup zuverlässig in den Profil-Einstellungen deaktivieren
mkdir -p /tmp/chromium-kiosk/Default
printf '{"translate":{"enabled":false},"intl":{"accept_languages":"de-DE,de,en"}}\n' > /tmp/chromium-kiosk/Default/Preferences

exec "$BROWSER" \
  --kiosk \
  --lang=de-DE \
  --ozone-platform=wayland \
  --force-device-scale-factor="$SCALE" \
  --enable-features=OverlayScrollbar \
  --touch-events=enabled \
  --disable-pinch \
  --overscroll-history-navigation=0 \
  --noerrdialogs \
  --disable-infobars \
  --no-first-run \
  --disable-session-crashed-bubble \
  --disable-restore-session-state \
  --disable-features=Translate,TranslateUI \
  --check-for-update-interval=31536000 \
  --autoplay-policy=no-user-gesture-required \
  --user-data-dir=/tmp/chromium-kiosk \
  "$TARGET_URL"
