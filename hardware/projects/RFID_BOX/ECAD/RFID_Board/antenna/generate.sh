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

# Freie Flaeche auf dem RFID-Board und Randabstand des Kupfers zur
# Platinenkante. Daraus folgt die Kontur; alles Weitere haengt daran.
AREA_W=80
AREA_H=50
EDGE=0.5
RADIUS=10          # Eckradius der Kontur
START=bottom       # Lage des Abgriffs: bottom, bottom-left, left, ...

# Wicklungsparameter. Die Steigung trace+clearance gilt KiCoil als
# Mindestabstand: die Ringbreite muss turns * (trace + clearance) sein.
TRACE=1.0
CLEAR=0.5
TURNS=(2 3 4)

# Konturmass ist die Mittellinie der aeussersten Windung, deshalb halbe
# Leiterbreite und Randabstand abziehen.
read -r SHAPE_W SHAPE_H < <("$PY" -c "
print($AREA_W - 2*$EDGE - $TRACE, $AREA_H - 2*$EDGE - $TRACE)")

COMMON=(--trace-width "$TRACE" --clearance "$CLEAR" --single-layer
        --keepout-margin 3)

# KiCoils SVG-Import versteht nur Polygone, die Rundungen werden deshalb
# hier schon aufgeloest (siehe shape_svg.py).
"$PY" shape_svg.py rounded-rect contour.svg \
    --width "$SHAPE_W" --height "$SHAPE_H" --radius "$RADIUS" --start "$START"

gen() {           # gen <windungen>
    local turns="$1"
    local name="${AREA_W}x${AREA_H}_${turns}T"
    local annular
    annular=$("$PY" -c "print($turns * ($TRACE + $CLEAR))")
    echo "== NFC_Loop_$name  ($turns Windungen, Ringbreite $annular mm)"
    "$KICOIL" --turns "$turns" "${COMMON[@]}" --footprint-name "NFC_Loop_$name" \
        svg --annular-width "$annular" contour.svg "NFC_Loop_$name.kicad_mod" 2>/dev/null
    "$KICOIL" --turns "$turns" "${COMMON[@]}" --format svg \
        svg --annular-width "$annular" contour.svg "render_$name.svg" 2>/dev/null
    if command -v rsvg-convert >/dev/null; then
        rsvg-convert -w 430 -b white "render_$name.svg" -o "render_$name.png"
        rm -f "render_$name.svg"
    fi
    grep -o "inductance approximately [0-9.]* µH" "NFC_Loop_$name.kicad_mod"
}

echo "== Flaeche ${AREA_W} x ${AREA_H} mm, Kontur ${SHAPE_W} x ${SHAPE_H} mm (Mittellinie), Abgriff $START"
for t in "${TURNS[@]}"; do
    gen "$t"
done

# Vergleichsbild aus den Einzelrendern. Pillow kommt mit KiCoil mit.
"$PY" - "${TURNS[@]}" <<'PYEOF'
import sys
try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit(0)
names = [f"render_80x50_{t}T.png" for t in sys.argv[1:]]
imgs = [Image.open(n).convert("RGB") for n in names]
w, h, pad, head = imgs[0].width, imgs[0].height, 12, 22
cols = 2
rows = -(-len(imgs) // cols)
sheet = Image.new("RGB", (cols * w + (cols + 1) * pad,
                          rows * (h + head) + (rows + 1) * pad), "white")
d = ImageDraw.Draw(sheet)
for i, (img, t) in enumerate(zip(imgs, sys.argv[1:])):
    x = pad + (i % cols) * (w + pad)
    y = pad + (i // cols) * (h + head + pad)
    d.text((x, y + 4), f"{t} Windungen", fill="black")
    sheet.paste(img, (x, y + head))
sheet.save("vergleich.png")
print("vergleich.png")
PYEOF

echo
echo "Fertig. Footprints: NFC_Loop_*.kicad_mod"
