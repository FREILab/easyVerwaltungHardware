// ============================================================================
// SmartTerminal 3D-Print — Simples, parametrisches Gehäuse (Pilot-Version v2)
// ============================================================================
// Kabel-Layout (wie am Tisch aufgebaut):
//   - Stromversorgung über Display-USB-C -> Kabel geht gerade NACH UNTEN
//     (Richtung LAN/USB-Ports, hinter dem HW-147 NFC-Modul vorbei)
//   - Display um 180° gedreht (breites Flex-Kinn OBEN, dünner Rand UNTEN)
//   - HW-147 NFC-Modul direkt UNTER dem Display (vor dem USB-C-Kabelkanal)
//   - 30x30 mm Lüfter im Rückdeckel (bläst direkt auf die Pi-5-CPU)
//   - Rechte Seitenwand komplett frei für modulare, anschraubbare Skådis-Halterung!
//
// Koordinatensystem (Blick von VORNE auf das Display):
//   +X = RECHTS  (Skådis-Halterung)
//   +Y = OBEN    (Waveshare 5" DSI Display oben, HW-147 NFC-Modul unten)
//   +Z = VORNE   (Frontfläche = shell_d, Rückseite = 0)
//
// Alle Einzelteile ("front", "back", "skadis_*") werden beim Export
// automatisch flach & ohne Support auf Z = 0 gelegt.
// ============================================================================

// Auswahl: "assembly" | "front" | "back" | "skadis_parallel" | "skadis_side"
part = "assembly";

$fn = 36;

// ============================================================================
// 1. MESSWERTE (Messschieber — hier bei Bedarf feinjustieren)
// ============================================================================

// Waveshare 5" DSI Display (Hochkant, 180° gedreht: breites Kinn OBEN!)
disp_w           = 70.6;    // Außenmaß Glas Breite
disp_h           = 127.0;   // Außenmaß Glas Höhe
disp_tol         = 0.6;     // Umlaufendes Spiel für Glastasche
disp_vis_w       = 62.2;    // Sichtbares Bild Breite
disp_vis_h       = 110.0;   // Sichtbares Bild Höhe
disp_vis_margin  = 1.2;     // Fenster +1.2 mm größer (bleibt im schwarzen Rand)
disp_bezel_top   = 12.5;    // Breites Flex-Kinn OBEN (unten schmaler Rand ~4.5 mm)

// Gesamttiefe Display-Front bis Pi-5-Oberseite inkl. Bolzen & Dupont-Stecker
stack_depth      = 36.6;
extra_clearance  = 5.4;     // Luft für Dupont + USB-C-Stecker hinter NFC + Luftstrom
inner_depth      = stack_depth + extra_clearance; // 42.0 mm (bei kürzeren Bolzen einfach reduzieren)

// HW-147 / PN532 NFC-Modul (sitzt unter dem Display, VOR dem USB-C-Kabel)
nfc_w            = 40.5;
nfc_h            = 43.0;
nfc_tol          = 0.6;
nfc_hole_inset   = 7.7;     // Lochmitte von Kante (unten links & oben rechts von vorne gesehen)
nfc_standoff_h   = 2.6;     // Abstand zur Frontwand (Platz für DIP-Schalter & Quarz)
nfc_front_wall   = 1.6;     // Dünne Kunststoffwand für volle 13.56-MHz-Reichweite
nfc_gap_to_disp  = 8.0;     // Etwas Luft zwischen Display-Unterkante & NFC für USB-C-Stecker

// 30x30 mm Lüfter (Rückdeckel, bläst direkt auf Pi-5-CPU)
fan_size         = 30.0;
fan_hole_pitch   = 24.0;    // Standard 30x30 mm Lüfter
fan_hole_d       = 3.2;     // M3 Durchgangsloch
fan_cutout_d     = 27.5;
fan_offset_x     = 0.0;     // Mitte Pi 5
fan_offset_y     = 24.0;    // Über Pi-5-CPU

// Kabelauslass UNTEN (USB-C-Stromkabel + optional Ethernet)
bottom_cable_w   = 36.0;    // Breite des Kabelauslasses unten (rechts/mittig bei USB-C & LAN)
bottom_cable_d   = 22.0;    // Tiefe von der Rückseite her (hinter dem NFC-Modul!)

// Gehäuse-Parameter
wall             = 3.0;     // Seitenwandstärke
front_wall       = 2.4;     // Frontwandstärke um das Display
back_wall        = 2.4;     // Dicke Rückdeckel
corner_r         = 5.0;     // Außenradius
lip_depth        = 1.8;     // Deckel-Einlegerand
screw_m3_pilot   = 2.85;    // Direktes Einschneiden M3 (für Ruthex M3 Insert: 4.0)

// Rechte M4-Anschraubpunkte (mittig ZWISCHEN den Skådis-Haken -> Haken bleiben 100% massiv!)
mount_m4_d       = 4.2;
mount_m4_nut_af  = 7.2;     // M4-Mutter Schlüsselweite (innen)
mount_ys         = [-20.0, 20.0]; // 2x M4-Schrauben (40 mm Abstand, frei zwischen den 3 Haken)
skadis_hook_ys   = [-40.0, 0.0, 40.0]; // 3x IKEA Skådis-Haken im exakten 40-mm-Raster!

// IKEA Skådis Standardmaße + Standoff rechts (hier direkt anpassbar!)
skadis_standoff  = 16.0;    // Abstand rechts zur Skådis-Wand (frei anpassbar!)
skadis_pitch_y   = 40.0;    // Vertikaler Schlitzabstand (selbe Spalte)
skadis_hook_w    = 4.5;     // Schlitzbreite = 5.0 mm
skadis_hook_t    = 4.6;     // Haken-Dicke
skadis_board_t   = 5.4;     // Brettstärke 5.0 mm + 0.4 mm Spiel
skadis_hook_drop = 8.0;     // Einrast-Zunge nach unten hinter der Platte

// ============================================================================
// Abgeleitete Maße
// ============================================================================
inner_w = max(disp_w + disp_tol, 72.0) + 4.0;                       // 75.2 mm
inner_h = disp_h + disp_tol + nfc_gap_to_disp + nfc_h + nfc_tol + 6.0; // 185.2 mm
outer_w = inner_w + 2 * wall;                                       // 81.2 mm
outer_h = inner_h + 2 * wall;                                       // 191.2 mm
shell_d = front_wall + inner_depth;                                 // 44.4 mm

// Display-Position (oben ausgerichtet)
disp_center_y = outer_h / 2 - wall - 3.0 - (disp_h + disp_tol) / 2;
disp_win_w    = disp_vis_w + disp_vis_margin;
disp_win_h    = disp_vis_h + disp_vis_margin;
// Fenster-Mitte relativ zur Glas-Mitte (oben breites Flex-Kinn 12.5 mm, unten 4.5 mm)
disp_win_y    = disp_center_y + (disp_h / 2 - disp_bezel_top - disp_vis_h / 2);

// NFC-Position (zentriert unter dem Display)
nfc_center_y  = disp_center_y - (disp_h + disp_tol) / 2 - nfc_gap_to_disp - (nfc_h + nfc_tol) / 2;

// 4 Eck-Schraubdome für Rückdeckel
boss_x = inner_w / 2 - 4.4;
boss_y = inner_h / 2 - 4.4;

// ============================================================================
// Hilfsmodule
// ============================================================================
module rrect2d(w, h, r) {
    offset(r = r) offset(delta = -r) square([w, h], center = true);
}

module rbox(w, h, d, r) {
    linear_extrude(height = d) rrect2d(w, h, r);
}

// ============================================================================
// TEIL 1: FRONTSCHALE (Z = shell_d ist VORNE, Z = 0 ist HINTEN)
// ============================================================================
module front_shell_upright() {
    difference() {
        // Außenkörper
        rbox(outer_w, outer_h, shell_d, corner_r);

        // Haupt-Innenraum (offen nach hinten bei Z = 0 bis Z = inner_depth)
        translate([0, 0, -0.5])
            rbox(inner_w, inner_h, inner_depth + 0.5, max(1.5, corner_r - wall));

        // Display-Sichtfenster (durchgehend durch Frontwand bei Z = shell_d)
        translate([0, disp_win_y, inner_depth - 1])
            rbox(disp_win_w, disp_win_h, front_wall + 2, 1.5);

        // Display-Glastasche (0.8 mm von innen in Frontwand eingelassen)
        translate([0, disp_center_y, inner_depth - 0.5])
            rbox(disp_w + disp_tol, disp_h + disp_tol, 1.3, 1.0);

        // NFC-Tasche: Frontwand von innen auf dünne 1.6 mm abtragen
        translate([0, nfc_center_y, inner_depth - 0.5])
            rbox(nfc_w + nfc_tol, nfc_h + nfc_tol, (front_wall - nfc_front_wall) + 0.5, 1.5);

        // Dezentes NFC-Symbol außen auf der Front (0.5 mm vertieft, ohne Support druckbar)
        translate([0, nfc_center_y, shell_d - 0.5])
            linear_extrude(height = 0.6) {
                difference() { circle(r = 11.0); circle(r = 9.4); }
                difference() { circle(r = 7.0);  circle(r = 5.4); }
                circle(r = 2.5);
            }

        // Kabelauslass UNTEN (-Y-Wand, offen nach hinten zum Einlegen des USB-C-Kabels)
        translate([-bottom_cable_w / 2, -outer_h / 2 - 1, -0.5])
            cube([bottom_cable_w, wall + 3, bottom_cable_d + 0.5]);

        // Lüftungsschlitze in der Oberseite (Warmluft-Auslass)
        for (ix = [-24 : 8 : 24]) {
            translate([ix, outer_h / 2 - wall / 2, inner_depth / 2])
                cube([3.2, wall + 2, inner_depth - 14], center = true);
        }

        // Einlegerand hinten (Z = 0..lip_depth) für bündigen Rückdeckel
        translate([0, 0, -0.5])
            rbox(inner_w + 2.2, inner_h + 2.2, lip_depth + 0.5, max(2.0, corner_r - wall + 1.0));

        // 3x M4-Löcher rechts (+X) im 40-mm-Raster für modulare Skådis-Halterung
        for (my = mount_ys) {
            translate([outer_w / 2 - wall - 1, my, shell_d / 2])
                rotate([0, 90, 0])
                    cylinder(d = mount_m4_d, h = wall + 3);
        }
    }

    // --- Innen-Strukturen (alle hängen direkt an Z = inner_depth -> 0 Support beim Druck!) ---

    // 4x Eck-Schraubdome für Rückdeckel (M3)
    for (sx = [-1, 1], sy = [-1, 1]) {
        difference() {
            translate([sx * boss_x, sy * boss_y, lip_depth])
                cylinder(d = 7.6, h = inner_depth - lip_depth);
            translate([sx * boss_x, sy * boss_y, lip_depth - 0.5])
                cylinder(d = screw_m3_pilot, h = 13.0);
        }
    }

    // Führungsstege rund um das Waveshare-Display-Glas
    disp_guide_h = 3.5;
    for (sx = [-1, 1]) {
        translate([sx * ((disp_w + disp_tol) / 2 + 1.1), disp_center_y, inner_depth - disp_guide_h / 2])
            cube([2.2, disp_h - 16, disp_guide_h], center = true);
    }
    translate([0, disp_center_y - (disp_h + disp_tol) / 2 - 1.3, inner_depth - disp_guide_h / 2])
        cube([disp_w - 12, 2.6, disp_guide_h], center = true);

    // 2x Diagonale HW-147 NFC-Befestigungsdome (unten links & oben rechts von VORNE!)
    nfc_holes = [
        [-(nfc_w / 2 - nfc_hole_inset), -(nfc_h / 2 - nfc_hole_inset)], // unten links (-X, -Y)
        [ +(nfc_w / 2 - nfc_hole_inset), +(nfc_h / 2 - nfc_hole_inset)], // oben rechts (+X, +Y)
    ];
    nfc_boss_h = nfc_standoff_h + 4.5;
    nfc_inner_z = shell_d - nfc_front_wall;
    for (h = nfc_holes) {
        translate([h[0], nfc_center_y + h[1], nfc_inner_z - nfc_boss_h]) {
            difference() {
                cylinder(d = 6.4, h = nfc_boss_h);
                translate([0, 0, -0.5])
                    cylinder(d = 2.6, h = nfc_boss_h - 0.8); // M3 selbstschneidend / M2.5
            }
        }
    }
    // Kleine seitliche Führungsnasen für HW-147 Platine
    for (sx = [-1, 1]) {
        translate([sx * ((nfc_w + nfc_tol) / 2 + 1.0), nfc_center_y, nfc_inner_z - 2.5])
            cube([2.0, 24, 5.0], center = true);
    }

    // Verstärkung + Sechskant-Mutterntaschen innen für die 3 M4-Skådis-Punkte rechts
    for (my = mount_ys) {
        difference() {
            translate([inner_w / 2 - 2.2, my, shell_d / 2])
                cube([4.4, 13.0, 13.0], center = true);
            translate([inner_w / 2 - 5.0, my, shell_d / 2])
                rotate([0, 90, 0])
                    cylinder(d = mount_m4_d, h = 10);
            translate([inner_w / 2 - 4.5, my, shell_d / 2])
                rotate([0, 90, 0])
                    cylinder(d = mount_m4_nut_af / cos(30), h = 4.2, $fn = 6);
        }
    }
}

// ============================================================================
// TEIL 2: RÜCKDECKEL MIT 30x30 mm LÜFTER (bläst direkt auf Pi-5-CPU)
// ============================================================================
module back_lid_upright() {
    lid_w = inner_w + 1.8;
    lid_h = inner_h + 1.8;
    lid_r = max(1.8, corner_r - wall + 0.8);

    difference() {
        union() {
            // Grundplatte (Z = 0 außen, Z = back_wall innen)
            rbox(lid_w, lid_h, back_wall, lid_r);
            // Zentrier-Rahmen innen für 30x30 mm Lüfter
            translate([fan_offset_x, fan_offset_y, back_wall + 1.0])
                difference() {
                    cube([fan_size + 4.4, fan_size + 4.4, 2.0], center = true);
                    cube([fan_size + 0.6, fan_size + 0.6, 3.0], center = true);
                }
        }

        // 4x M3-Senklöcher für Deckelbefestigung
        for (sx = [-1, 1], sy = [-1, 1]) {
            translate([sx * boss_x, sy * boss_y, -0.5])
                cylinder(d = 3.4, h = back_wall + 2);
            translate([sx * boss_x, sy * boss_y, -0.1])
                cylinder(d1 = 6.4, d2 = 3.4, h = 1.6);
        }

        // 30x30 mm Lüfter-Bohrungen (24x24 mm) + geschütztes Luftgitter
        translate([fan_offset_x, fan_offset_y, -0.5]) {
            for (fx = [-1, 1], fy = [-1, 1]) {
                translate([fx * fan_hole_pitch / 2, fy * fan_hole_pitch / 2, 0])
                    cylinder(d = fan_hole_d, h = back_wall + 4);
            }
            intersection() {
                cylinder(d = fan_cutout_d, h = back_wall + 4);
                union() {
                    for (sy = [-10 : 5 : 10]) {
                        translate([0, sy, (back_wall + 4) / 2])
                            cube([fan_cutout_d + 2, 2.8, back_wall + 4], center = true);
                    }
                }
            }
        }

        // Abluftschlitze im unteren Bereich
        for (iy = [-64 : 7 : -36]) {
            translate([0, iy, -0.5])
                rbox(36, 3.0, back_wall + 2, 1.2);
        }
    }
}

// ============================================================================
// TEIL 3A & 3B: KOMPAKTE SKÅDIS-HALTERUNGEN RECHTS (3x M4, 40-mm-Raster)
// ============================================================================
module skadis_hook_back() {
    // Haken geht in -Z (in die Skådis-Platte) und rastet nach -Y (unten) ein
    translate([-skadis_hook_w / 2, 0, -(skadis_board_t + skadis_hook_t)]) {
        cube([skadis_hook_w, skadis_hook_t, skadis_board_t + skadis_hook_t + 0.5]);
        translate([0, -skadis_hook_drop, 0])
            cube([skadis_hook_w, skadis_hook_drop + skadis_hook_t, skadis_hook_t]);
    }
}

// Variante A: Skådis-Wand PARALLEL hinter dem Display (mit `skadis_standoff` rechts versetzt)
module skadis_mount_parallel_upright() {
    flange_t = 5.0;
    flange_d = 18.0;
    z0       = shell_d / 2 - flange_d / 2;
    arm_len  = skadis_standoff + 12.0;
    y_bot    = -52.0;
    y_top    =  52.0;

    difference() {
        union() {
            // Linker Anschraubflansch an rechter Gehäusewand
            translate([0, y_bot, z0])
                cube([flange_t, y_top - y_bot, flange_d]);
            // Auslegerplatte nach rechts (+X)
            translate([0, y_bot, z0])
                cube([arm_len, y_top - y_bot, 6.0]);
            // Versteifungsrippen oben & unten
            for (ry = [y_bot, y_top - 4.0]) {
                translate([flange_t, ry, z0 + 6.0])
                    cube([arm_len - flange_t, 4.0, 8.0]);
            }
            // 3x IKEA Skådis-Haken im exakten 40-mm-Raster (-40, 0, +40)
            for (hy = skadis_hook_ys) {
                translate([skadis_standoff + 6.0, hy, z0])
                    skadis_hook_back();
            }
        }

        // 2x M4-Löcher (-20, +20)
        for (my = mount_ys) {
            translate([-1, my, shell_d / 2])
                rotate([0, 90, 0])
                    cylinder(d = mount_m4_d, h = flange_t + 2);
        }
    }
}

// Variante B: Skådis-Wand 90° SEITLICH rechts (mit `skadis_standoff` Abstand)
module skadis_mount_side_upright() {
    flange_t = 5.0;
    arm_d    = 18.0;
    z0       = shell_d / 2 - arm_d / 2;
    y_bot    = -52.0;
    y_top    =  52.0;

    difference() {
        union() {
            // Kompakter Standoff-Block zwischen Gehäusewand und Skådis-Wand
            translate([0, y_bot, z0])
                cube([skadis_standoff, y_top - y_bot, arm_d]);
            // 3x IKEA Skådis-Haken nach rechts (+X) im 40-mm-Raster (-40, 0, +40)
            for (hy = skadis_hook_ys) {
                translate([skadis_standoff, hy, shell_d / 2])
                    rotate([0, -90, 0])
                        skadis_hook_back();
            }
        }

        // 2x M4-Durchgangslöcher + Kopfsenkung von rechts (genau zwischen den Haken -> kein Konflikt!)
        for (my = mount_ys) {
            translate([-1, my, shell_d / 2])
                rotate([0, 90, 0])
                    cylinder(d = mount_m4_d, h = skadis_standoff + 2);
            translate([flange_t, my, shell_d / 2])
                rotate([0, 90, 0])
                    cylinder(d = 8.4, h = skadis_standoff);
        }
    }
}

// ============================================================================
// AUSGABE (Einzelteile automatisch flach & ohne Support auf Z = 0!)
// ============================================================================
if (part == "front") {
    translate([0, 0, shell_d]) rotate([0, 180, 0]) front_shell_upright();
} else if (part == "back") {
    back_lid_upright();
} else if (part == "skadis_parallel") {
    translate([0, 0, 0]) rotate([0, 90, 0]) skadis_mount_parallel_upright();
} else if (part == "skadis_side") {
    translate([0, 0, -(shell_d / 2 - 9.0)]) skadis_mount_side_upright();
} else {
    color("SlateGray") front_shell_upright();
    color("LightSteelBlue") translate([0, 0, -18]) back_lid_upright();
    color("DarkOrange") translate([outer_w / 2 + 1.5, 0, 0]) skadis_mount_side_upright();
}
