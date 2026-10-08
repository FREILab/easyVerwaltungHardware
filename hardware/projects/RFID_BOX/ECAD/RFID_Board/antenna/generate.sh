#!/usr/bin/env bash
# Erzeugt die Antennen-Footprints fuer das RFID-Board mit KiCoil.
#
# KiCoil braucht Python >= 3.13. KiCads mitgelieferter Interpreter ist 3.9,
# deshalb laeuft das hier in einer eigenen Umgebung ausserhalb von KiCad.
# Beim ersten Aufruf wird sie angelegt, danach wiederverwendet.
set -euo pipefail
cd "$(dirname "$0")"

VENV="${KICOIL_VENV:-$HOME/.cache/kicoil-venv}"
PY313="${PYTHON313:-/opt/homebrew/bin/python3.13}"

if [ ! -x "$VENV/bin/kicoil" ]; then
    echo "== Richte KiCoil-Umgebung in $VENV ein"
    "$PY313" -m venv "$VENV"
    "$VENV/bin/pip" install -q --disable-pip-version-check kicoil
fi
KICOIL="$VENV/bin/kicoil"
PY="$VENV/bin/python"

# Gemeinsame Wicklungsparameter. Sie gelten fuer alle Varianten, damit die
# Formen untereinander vergleichbar bleiben.
COMMON=(--turns 3 --trace-width 1.0 --clearance 0.5 --single-layer
        --keepout-margin 3)

gen() {           # gen <name> <beschreibung> <kicoil-argumente...>
    local name="$1"; shift
    local desc="$1"; shift
    echo "== $name  ($desc)"
    "$KICOIL" "${COMMON[@]}" --footprint-name "NFC_Loop_$name" \
        "$@" "NFC_Loop_$name.kicad_mod" 2>/dev/null
    "$KICOIL" "${COMMON[@]}" --format svg "$@" "render_$name.svg" 2>/dev/null
    if command -v rsvg-convert >/dev/null; then
        rsvg-convert -w 430 -b white "render_$name.svg" -o "render_$name.png"
        rm -f "render_$name.svg"
    fi
    grep -o "inductance approximately [0-9.]* µH" "NFC_Loop_$name.kicad_mod"
}

# Konturen fuer den SVG-Weg. KiCoils SVG-Import versteht nur Polygone,
# die Rundungen werden deshalb hier schon aufgeloest (siehe shape_svg.py).
"$PY" shape_svg.py rounded-rect rr.svg --width 78 --height 78 --radius 12
"$PY" shape_svg.py ellipse      el.svg --width 78 --height 58
"$PY" shape_svg.py stadium      st.svg --width 78 --height 48

gen 78x78_3T "Rechteck, scharfe Ecken"  rectangle --width 78 --height 78 --annular-width 4.5
gen rr       "Rechteck, Eckradius 12"   svg rr.svg
gen el       "Ellipse 78 x 58"          svg el.svg
gen st       "Racetrack 78 x 48"        svg st.svg

echo
echo "Fertig. Footprints: NFC_Loop_*.kicad_mod"
