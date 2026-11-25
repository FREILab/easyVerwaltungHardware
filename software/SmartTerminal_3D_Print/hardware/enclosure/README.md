# Gehäuse – SmartTerminal 3D-Print (Pilot-Version)

Parametrisches 2-teiliges Gehäuse + anschraubbare IKEA-Skådis-Halterung in OpenSCAD ([smartterminal_enclosure.scad](smartterminal_enclosure.scad)), druckbar **ohne Support** (PLA/PETG, `0,2 mm` Layer, `15–20 %` Infill).

## Enthaltene Teile (`stl/`)

| Datei | Beschreibung | Druckausrichtung |
|---|---|---|
| `01_front_shell.stl` | Frontschale für Waveshare 5" DSI (180° gedreht: Flex-Kinn oben) + HW-147 NFC-Modul unten + Kabelauslass unten (USB-C) | Frontfläche flach auf Druckbett |
| `02_back_lid.stl` | Rückdeckel mit geschütztem 30×30-mm-Lüftergitter über Pi-5-CPU | Außenseite flach auf Druckbett |
| `03b_skadis_side.stl` | Skådis-Halterung rechts (Skådis-Wand 90° seitlich rechts, 16 mm Standoff, 3 Haken im 40-mm-Raster) | Seitlich flach auf Druckbett |
| `03a_skadis_parallel.stl` | Alternative Skådis-Halterung rechts (Skådis-Wand parallel hinter dem Display) | Seitlich liegend |

## Schrauben & Montage
- **Display + Pi 5 (feste Verschraubung, null Druck auf Glas):** 4× M2.5×10–12 mm von außen durch den Rückdeckel + Pi 5 direkt in die 4× M2.5-Messingbolzen (`49,0 × 58,0 mm` Raster, `12,9 mm` über unterer Glaskante nach Waveshare-Datenblatt).
- **Rückdeckel an Frontschale:** 4× M3×10–12 mm (schneiden direkt ins Plastik; für Ruthex-M3-Inserts `screw_m3_pilot = 4.0` setzen)
- **HW-147 NFC-Modul:** 2× M2.5 oder M3×6 mm (diagonal: unten links & oben rechts)
- **30×30-mm-Lüfter:** 4× M3×12–16 mm (24×24-mm-Bohrbild, sitzt innen im Rückdeckel mittig zwischen den 4 M2.5-Stutzen)
- **Skådis-Adapter rechts:** 2× M4×12–16 mm + 2× M4-Muttern (Mutterntaschen innen im Gehäuse, 40 mm Abstand)

Alle Maße (Glasmaß, Tiefe `stack_depth`, `skadis_standoff`, Hakenmaße) stehen gesammelt am Anfang von `smartterminal_enclosure.scad`.
