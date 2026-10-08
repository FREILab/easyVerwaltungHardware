#!/usr/bin/env python3
"""Erzeugt Polygon-SVGs fuer KiCoil.

KiCoils SVG-Import liest nur das erste <path> und versteht darin
ausschliesslich "M x y L x y ... Z" mit leergetrennten Koordinaten —
keine Boegen, keine H/V-Kurzformen, keine Kommas. Runde Konturen muessen
deshalb hier schon in Strecken aufgeloest werden.

Alle Konturen werden um (0,0) zentriert, damit KiCoil nicht selbst
nachzentriert.
"""
import argparse
import math


def rounded_rect(w, h, r, tol=0.03):
    """Rechteck mit Eckradius, Ecken als Polygonzug."""
    r = min(r, w / 2, h / 2)
    # Segmentwinkel aus der zulaessigen Pfeilhoehe: r*(1-cos(a/2)) <= tol
    n = max(3, math.ceil((math.pi / 2) / (2 * math.acos(max(-1.0, 1 - tol / r)))))
    pts = []
    corners = [(w / 2 - r, h / 2 - r, 0),
               (-(w / 2 - r), h / 2 - r, 90),
               (-(w / 2 - r), -(h / 2 - r), 180),
               (w / 2 - r, -(h / 2 - r), 270)]
    for cx, cy, a0 in corners:
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def ellipse(w, h, segments=96):
    a, b = w / 2, h / 2
    return [(a * math.cos(2 * math.pi * i / segments),
             b * math.sin(2 * math.pi * i / segments)) for i in range(segments)]


def stadium(w, h, tol=0.03):
    """Rechteck mit halbkreisfoermigen Schmalseiten (Racetrack)."""
    return rounded_rect(w, h, min(w, h) / 2, tol)


def write_svg(path, pts, margin=1.0):
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    w = max(xs) - min(xs) + 2 * margin
    h = max(ys) - min(ys) + 2 * margin
    # Pfad in SVG-Koordinaten (y nach unten), Inhalt bleibt um 0 zentriert
    d = "M " + " L ".join(f"{x:.4f} {y:.4f}" for x, y in pts) + " Z"
    body = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w:.3f}mm" '
            f'height="{h:.3f}mm" viewBox="{-w/2:.3f} {-h/2:.3f} {w:.3f} {h:.3f}">\n'
            f'  <path d="{d}" fill="black"/>\n</svg>\n')
    open(path, "w").write(body)
    print(f"{path}: {len(pts)} Punkte, {max(xs)-min(xs):.1f} x {max(ys)-min(ys):.1f} mm")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("shape", choices=["rounded-rect", "ellipse", "stadium"])
    p.add_argument("outfile")
    p.add_argument("--width", type=float, required=True)
    p.add_argument("--height", type=float, required=True)
    p.add_argument("--radius", type=float, default=12.0)
    a = p.parse_args()
    if a.shape == "rounded-rect":
        pts = rounded_rect(a.width, a.height, a.radius)
    elif a.shape == "ellipse":
        pts = ellipse(a.width, a.height)
    else:
        pts = stadium(a.width, a.height)
    write_svg(a.outfile, pts)
