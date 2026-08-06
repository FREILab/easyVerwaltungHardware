#!/usr/bin/env python3
"""
Türlog-Auswertung — dampft die SD-Karten-Logs (ESP-TX-Bus) auf einen Digest ein.

Der Logger zeichnet die ESP-TX-Leitung auf: DBG:-Diagnosezeilen, PING, LED:, MOTOR:,
BUZZ: sowie die vom ESP gespiegelten ATmega-Events (DBG:[...] [ATM] ...).

Aufruf:
    python3 analyze_log.py <logdatei> [weitere_logdateien ...]

Beantwortet direkt die offenen Debug-Checks (siehe DEBUG_tuer-ausfall.md):
    - Bild A/B:  trat jemals  atmega=DEAD  auf?
    - Check 1:   war  state=  in den HB-Zeilen durchgehend STANDBY?
    - Check 2:   tauchte  [ATM] ... RFID reinit dead  auf?  (SPI-tot vs. RF-tot)
    - Reboots:   BOOT-Events (ESP + ATmega) mit Grund
    - Stalls:    Lücken in der HB-Kadenz (Logging/ESP-Hänger)
"""

import re
import sys

# Regex-Helfer — tolerant gegenüber einem evtl. vom Logger vorangestellten Präfix:
# wir suchen die Felder einfach irgendwo in der Zeile.
RE_TS      = re.compile(r"\[(\d{2}:\d{2}:\d{2})\]|\[\+(\d+)s\]")  # NTP-Zeit oder +Sek seit Boot
RE_UP      = re.compile(r"\bup=(\d+)")
RE_STATE   = re.compile(r"\bstate=(\w+)")
RE_ATMEGA  = re.compile(r"\batmega=(\w+)")
RE_RSSI    = re.compile(r"\brssi=(-?\d+)")
RE_HEAP    = re.compile(r"\bheap=(\d+)")
RE_FRAG    = re.compile(r"\bfrag=(\d+)")
RE_SLOWDT  = re.compile(r"SLOWLOOP dt=(\d+)")
RE_BOOTRSN = re.compile(r"BOOT reason=(\S+)")
RE_VER     = re.compile(r"\bver=([0-9a-fA-F]{2})")   # ATmega-HB: RC522 VersionReg
RE_TX      = re.compile(r"\btx=([0-9a-fA-F]{2})")    # ATmega-HB: RC522 TxControlReg

HB_GAP_WARN_S = 15   # HB sollte alle 5s kommen; Lücke > 15s = Stall/Hänger


def ts_of(line):
    """Liefert den Zeitstempel-String der Zeile (HH:MM:SS oder +Ns) oder '?'."""
    m = RE_TS.search(line)
    if not m:
        return "?"
    return m.group(1) if m.group(1) else f"+{m.group(2)}s"


def natkey(path):
    """Natürliche Sortierung nach Dateiname (LOG00013 < LOG00014), damit die
    Dateien chronologisch zusammengehängt werden — sonst ist der 'Tail' die
    falsche Datei. Zahl im Namen entscheidet."""
    import os
    m = re.search(r"(\d+)", os.path.basename(path))
    return (int(m.group(1)) if m else 0, path)


def main(paths):
    paths = sorted(paths, key=natkey)
    print(f"# Datei-Reihenfolge (chronologisch): {', '.join(paths)}")
    lines = []
    for p in paths:
        try:
            with open(p, "r", errors="replace") as f:
                lines.extend(l.rstrip("\n") for l in f)
        except OSError as e:
            print(f"FEHLER beim Lesen von {p}: {e}", file=sys.stderr)
            return 1

    total = len(lines)
    if total == 0:
        print("Log ist leer.")
        return 0

    # Sammelbehälter
    counts = {}
    def bump(k): counts[k] = counts.get(k, 0) + 1

    dead_lines       = []   # atmega=DEAD
    nonstandby_lines = []   # state != STANDBY
    reinit_dead      = []   # [ATM] RFID reinit dead
    boot_esp         = []   # BOOT (ESP)
    boot_atm         = []   # [ATM] BOOT
    health_restart   = []   # HEALTH restart
    slowloops        = []   # (ts, dt)
    rfid_timeline    = []   # alle RFID-bezogenen Zeilen
    hb_gaps          = []   # (ts, luecke_s)
    reboots_up       = 0    # up= ist gefallen -> Neustart
    alive_count      = 0    # ATmega-Selbst-Heartbeat (ATMDBG:ALIVE)
    alive_faults     = []   # ALIVE ... FAULT (RC522-Register defekt)
    ver_seen         = {}   # RC522 VersionReg-Werte
    tx_seen          = {}   # RC522 TxControlReg-Werte
    atm_up_prev      = None
    atm_hb_gaps      = []   # (ts, luecke_s) im ATmega-Heartbeat

    first_ts = None
    last_ts  = None
    up_min = up_max = None
    prev_up = None
    state_seen   = {}       # wert -> anzahl
    atmega_seen  = {}

    for ln in lines:
        ts = ts_of(ln)
        if RE_TS.search(ln):
            if first_ts is None:
                first_ts = ts
            last_ts = ts

        is_hb = "HB up=" in ln

        # --- Klassifikation ---
        if is_hb:
            bump("HB")
            m = RE_STATE.search(ln)
            if m:
                st = m.group(1)
                state_seen[st] = state_seen.get(st, 0) + 1
                if st != "STANDBY":
                    nonstandby_lines.append((ts, st, ln))
            m = RE_ATMEGA.search(ln)
            if m:
                av = m.group(1)
                atmega_seen[av] = atmega_seen.get(av, 0) + 1
                if av == "DEAD":
                    dead_lines.append((ts, ln))
            # up= verfolgen: Reboots + HB-Lücken
            mu = RE_UP.search(ln)
            if mu:
                up = int(mu.group(1))
                up_min = up if up_min is None else min(up_min, up)
                up_max = up if up_max is None else max(up_max, up)
                if prev_up is not None:
                    if up < prev_up:
                        reboots_up += 1
                    else:
                        gap = up - prev_up
                        if gap > HB_GAP_WARN_S:
                            hb_gaps.append((ts, gap))
                prev_up = up

        if "[ATM]" in ln:
            bump("ATM-reflektiert")
            if "RFID reinit dead" in ln:
                reinit_dead.append((ts, ln)); bump("RFID reinit dead")
            if "RFID card=" in ln:
                bump("[ATM] RFID card"); rfid_timeline.append((ts, ln))
            if "BOOT reason=" in ln:
                boot_atm.append((ts, ln))
            if "ALIVE" in ln:
                alive_count += 1; bump("[ATM] ALIVE (Heartbeat)")
                mver = RE_VER.search(ln)
                mtx  = RE_TX.search(ln)
                if mver:
                    k = mver.group(1).lower(); ver_seen[k] = ver_seen.get(k, 0) + 1
                if mtx:
                    k = mtx.group(1).lower(); tx_seen[k] = tx_seen.get(k, 0) + 1
                if "FAULT" in ln:
                    alive_faults.append((ts, ln)); bump("RC522 FAULT")
                mup = RE_UP.search(ln)   # ATmega-eigenes up= -> HB-Lücken erkennen
                if mup:
                    u = int(mup.group(1))
                    if atm_up_prev is not None and u >= atm_up_prev and (u - atm_up_prev) > 30:
                        atm_hb_gaps.append((ts, u - atm_up_prev))
                    atm_up_prev = u
        else:
            # ESP-eigene BOOT-Zeile (ohne [ATM])
            if "BOOT reason=" in ln and is_hb is False:
                boot_esp.append((ts, ln))

        if "[ESP] RFID:" in ln:
            bump("[ESP] RFID gelesen"); rfid_timeline.append((ts, ln))
        if "Zugang gewährt" in ln or "Zugang gewaehrt" in ln:
            bump("Zugang gewährt"); rfid_timeline.append((ts, ln))
        if "Karte abgelehnt" in ln:
            bump("Karte abgelehnt"); rfid_timeline.append((ts, ln))
        if "Server nicht erreichbar" in ln:
            bump("Server nicht erreichbar"); rfid_timeline.append((ts, ln))

        if "HEALTH restart" in ln:
            health_restart.append((ts, ln)); bump("HEALTH restart")
        if "HEALTH fail" in ln:
            bump("HEALTH fail")

        ms = RE_SLOWDT.search(ln)
        if ms:
            slowloops.append((ts, int(ms.group(1)))); bump("SLOWLOOP")

        if "[HTTP] connect" in ln and "ok=0" in ln:
            bump("HTTP connect FAIL")

    # ───────────────────────── Ausgabe ─────────────────────────
    def section(title): print(f"\n--- {title} ---")
    def listing(items, fmt, leer="<keine>"):
        if not items:
            print(f"  {leer}")
        else:
            for it in items:
                print("  " + fmt(it))

    print("=" * 60)
    print("Türlog-Auswertung")
    print("=" * 60)
    print(f"Dateien:        {', '.join(paths)}")
    print(f"Zeilen gesamt:  {total}")
    print(f"Zeitbereich:    {first_ts}  →  {last_ts}")
    if up_min is not None:
        print(f"up= (HB):       {up_min}s … {up_max}s   (Reboots via up=-Reset: {reboots_up})")
    if state_seen:
        print(f"state-Werte:    " + ", ".join(f"{k}×{v}" for k, v in state_seen.items()))
    if atmega_seen:
        print(f"atmega-Werte:   " + ", ".join(f"{k}×{v}" for k, v in atmega_seen.items()))

    section("Event-Zählung")
    for k in sorted(counts):
        print(f"  {k:24s} {counts[k]}")

    section("ROTE FLAGGEN (die entscheidenden Checks)")
    print(f"[Bild A/B]  atmega=DEAD:")
    listing(dead_lines, lambda x: f"{x[0]}  {x[1]}", leer="<keine>  → Bild B bestätigt (ATmega lief)")
    print(f"[Check 1]   HB state != STANDBY:")
    listing(nonstandby_lines, lambda x: f"{x[0]}  state={x[1]}", leer="<keine>  → ESP war bereit (kein State-Hänger)")
    print(f"[Check 2]   RFID reinit dead (VersionReg tot → SPI weg):")
    listing(reinit_dead, lambda x: f"{x[0]}  {x[1]}",
            leer="<keine>  → 'alive-but-not-reading': unbedingtes Re-Init nötig")
    print(f"BOOT (ESP):")
    listing(boot_esp, lambda x: f"{x[0]}  {x[1]}")
    print(f"BOOT (ATmega):")
    listing(boot_atm, lambda x: f"{x[0]}  {x[1]}")
    print(f"HEALTH restart:")
    listing(health_restart, lambda x: f"{x[0]}  {x[1]}")
    print(f"HB-Lücken > {HB_GAP_WARN_S}s (Logging/ESP-Stall):")
    listing(hb_gaps, lambda x: f"{x[0]}  Lücke {x[1]}s")
    print(f"SLOWLOOP (>1s Loop):")
    if slowloops:
        mx = max(slowloops, key=lambda x: x[1])
        print(f"  {len(slowloops)}× , max dt={mx[1]}ms @ {mx[0]}")
    else:
        print("  <keine>")

    section("ATmega-Heartbeat + RC522-Register (neue Firmware ab 2026-07-02)")
    if alive_count == 0:
        print("  <keine ALIVE-Zeilen>  → Firmware noch ohne ATmega-Heartbeat (alter Stand)")
    else:
        print(f"  ALIVE-Heartbeats: {alive_count}")
        if ver_seen:
            print("  RC522 ver= gesehen: " +
                  ", ".join(f"{k}×{v}" for k, v in sorted(ver_seen.items())) +
                  "   (0x91/0x92 = ok, 00/ff = SPI tot)")
        if tx_seen:
            print("  RC522 tx=  gesehen: " +
                  ", ".join(f"{k}×{v}" for k, v in sorted(tx_seen.items())) +
                  "   (Bit0/1 gesetzt = Antenne an)")
        print("  RC522-Register-FAULT (ALIVE…FAULT):")
        listing(alive_faults, lambda x: f"{x[0]}  {x[1]}",
                leer="<keine>  → RC522 durchgehend gesund")
        print(f"  ATmega-HB-Lücken >30s (Loop-Stall / Link-Tod):")
        listing(atm_hb_gaps, lambda x: f"{x[0]}  Lücke {x[1]}s")

    section(f"RFID-Timeline ({len(rfid_timeline)} Einträge)")
    if not rfid_timeline:
        print("  <keine RFID-Aktivität im gesamten Log>  ← Karten kamen nie durch")
    else:
        for ts, ln in rfid_timeline:
            print(f"  {ts}  {ln}")

    tailn = 30
    section(f"Letzte {tailn} Zeilen (Tail vor Kartenentnahme)")
    for ln in lines[-tailn:]:
        print("  " + ln)

    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
