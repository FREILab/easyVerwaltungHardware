# Debugging: wiederkehrender Tür-Ausfall (ATmega „verschluckt sich")

Stand: 2026-06-30 · Projekt: `DOOR_legacy_ESPArdu` · Board: RobotDyn UNO+WiFi R3

## Beobachtung (Ausfall vom Morgen 2026-06-30)

- ESP loggte **durchgehend** bis zum Ziehen der SD-Karte → ESP lief stabil.
- Auf der SD-Karte nur „wenige Infos über IP connection des ESP" (vermutlich die
  HB-Heartbeat-Zeilen, die die IP prominent zeigen).
- Tür **reagierte nicht auf RFID-Karte**.
- Keine ATmega-Outputs im Log — aber: der Logger tappt nur die **ESP→ATmega**-Leitung;
  ATmega-Nachrichten sehen wir nur indirekt über die ESP-Spiegelung (`DBG:[ATM] …`).

## Architektur-Kontext (für Wiedereinstieg)

```
[Backend] ←HTTP→ [ESP8266: Logik/State/WiFi/OTA] ←UART 9600→ [ATmega328P: RFID/Motor/LED]
```
- Ein gemeinsamer Hardware-UART, 9600 Baud. SD-Logger hängt am Bus, liest die ESP-TX-Leitung.
- ESP spiegelt Diagnose + ATmega-Events als `DBG:…` auf Bus **und** Telnet
  (`busDbg()`, [src_esp/main.cpp:105](src_esp/main.cpp#L105)).
- ATmega spricht nur bei Events (`RFID:`, `BTN:`, `REED:`, `MOTOR:OK/TIMEOUT`),
  auf `PING` mit `PONG`, und `ATMDBG:BOOT …` beim Start. **Kein eigener Heartbeat.**
- ATmega-Firmware-Update braucht USB + DIP-Wechsel vor Ort (nicht OTA!).
  ESP-Update geht per OTA (`door_01_esp_ota`).

## Diagnose: zwei Fehlerbilder, die gleich aussehen

| | Bild A: ATmega hängt/tot | Bild B: ATmega lebt, nur RFID tot |
|---|---|---|
| `PONG` | hört auf | kommt weiter |
| HB-Feld `atmega=` | `DEAD` | `OK` |
| `DBG:[ATM] …` | keine mehr | keine RFID, aber sonst aktiv |
| LEDs | eingefroren | reagieren auf ESP-Befehle |
| Ursache | Hang / Reset / TX-Draht tot | RC522 weggekippt (SPI/EMI/Brownout) |

RC522-Healthcheck (re-init bei VersionReg 0x00/0xFF) ist bereits drin
([src_atmega/main.cpp:181](src_atmega/main.cpp#L181), Commit 28f77c6).
Da der Fehler **danach** wiederkam: entweder Bild A, oder der RC522 kippt so weg,
dass der VersionReg-Check ihn nicht fängt (liest gültige Version, aber Feld/Antenne tot;
oder `PCD_Init()` heilt den Bus nicht voll).

## SCHRITT 0 (zuerst tun): vorhandenes Log auswerten

Das entscheidende Feld steht **schon im Log**. Jede Heartbeat-Zeile
([src_esp/main.cpp:230](src_esp/main.cpp#L230)) endet mit `atmega=OK|DEAD`:

```
HB up=… wifi=192.168… … state=STANDBY atmega=OK    ← Bild B (ATmega lebt, RFID tot)
HB up=… wifi=192.168… … state=STANDBY atmega=DEAD  ← Bild A (ATmega weg)
```

**Im Log prüfen:**
1. Letzte HB-Zeilen vor dem Kartenziehen: steht da `atmega=OK` oder `DEAD`?
2. Taucht irgendwo `DBG:[ATM] BOOT reason=…` auf? (= ATmega hat rebootet → A,
   `reason` sagt warum: `WDT` Hänger, `BOD` Brownout/Motorstrom, `EXT`/`POR` Strom)
3. HB-Kadenz: alle 5 s? Lücken/Stopp = ESP-Problem; durchgehend = ESP ok (erwartet).

**Achtung False Positive:** `atmega=DEAD` erscheint auch während langer Motorfahrten
(ATmega bis ~42 s blockiert, kein PONG; Timeout 15 s, [src_esp/settings.h:25](src_esp/settings.h#L25)).
Beim stillen Morgen-Ausfall gab's keine Fahrt → hier sauber zu werten.

→ **Ergebnis (2026-06-30):** **Bild B** — laut Auswertung **kein `atmega=DEAD`** im Log.
ATmega lief weiter (PONG kam), nur der RFID-Pfad war tot. → ATmega-Hang vom Tisch,
Verdächtiger ist der **RC522-Leser** (SPI lebendig, RF-Frontend tot). Siehe Abschnitt unten.

---

## Befund 2026-06-30: Bild B → RC522 (verifiziert)

ATmega lebte, nur Karten wurden nicht mehr gelesen. Library-Verhalten geprüft
([MFRC522.cpp:198-242](.pio/libdeps/door_01_atmega_usb/MFRC522/src/MFRC522.cpp#L198-L242)):

- Das `PCD_Init()` (Health-Check + nach jedem Lesen) macht im Betrieb nur einen
  **Soft-Reset** — Hard-Reset nur, wenn RST-Pin LOW gelesen wird (passiert nie im Betrieb).
- `PCD_Init()` ruft am Ende `PCD_AntennaOn()` (TX1/TX2 wieder ein) → ein volles Re-Init
  **heilt** einen weggekippten RF-Frontend.
- **Lücke:** Health-Check ruft `PCD_Init()` **nur bei VersionReg 0x00/0xFF** auf (= totes SPI).
  Den Fall „SPI lebt, RF/Register glitchen" deckt er **nicht** ab → Karten werden stumm ignoriert.

### Vermutete physikalische Ursache
- EMI/Brownout durch **Stepper-Motor** glitcht den RC522. Passt zur Morgen-Timeline:
  letzte Abend-Fahrt hinterlässt geglitchten Leser → sitzt über Nacht tot → erste
  Morgen-Karte schlägt fehl, VersionReg noch gültig → keine Recovery.

### Noch zwei KOSTENLOSE Log-Checks (vor Reflash!)
Beides steht in den ATmega-Reflexionen im vorhandenen Log:
1. **`state=` in den HB-Zeilen durchgehend `STANDBY`?** Falls ESP in `IDENT`/`RESET`
   festhing → **ESP-Bug, per OTA fixbar, kein Reflash nötig**. (Unwahrscheinlich, aber 10 s Suche.)
2. **`DBG:[ATM] … RFID reinit dead` im Log?**
   - **Ja** → VersionReg war tot → SPI weg → Soft-Re-Init heilt nicht → **Hard-Reset/Power-Fix** nötig.
   - **Nein** (und kein `RFID card=`) → Leser still tot bei gültigem VersionReg →
     **„alive-but-not-reading"** → **unbedingtes Re-Init** nötig.

### Fix-Plan (braucht EINEN ATmega-Reflash vor Ort — bündeln!)
Da Confirm + Fix beide einen Reflash brauchen, alles in **einen** Flash packen:

1. **Recovery härten** ([src_atmega/main.cpp:181](src_atmega/main.cpp#L181), `pollRFID` Health-Check):
   - **Unbedingtes volles Re-Init** alle ~5 s im Idle (nicht nur bei VersionReg 0x00/0xFF)
     ODER Register-Drift prüfen (TxControlReg-Antennenbits `0x03`, ModeReg `0x3D`) und bei
     Abweichung re-initen. Heilt „alive-but-not-reading".
   - **Echten Hard-Reset** auf dem Health-Pfad: RST-Pin (Pin 5) ~wenige µs LOW, dann HIGH,
     `delay(50)`, dann `PCD_Init()`. Gründlicher als Soft-Reset bei wedged Chip.
   - **Re-Init nach jeder Motorfahrt** (`runOpenDoor`/`runCloseDoor`) → trifft die EMI-Quelle direkt.
2. **Diagnose mitflashen** (damit nächster Ausfall eindeutig ist):
   - VersionReg **und** TxControlReg periodisch loggen (`ATMDBG:RFID ver=.. tx=..`),
     jedes Re-Init mit Registerwert.
   - ATmega-Selbst-Heartbeat `ATMDBG:ALIVE n=<count>` (siehe Maßnahme 3 oben).
   → Falls es trotzdem wieder ausfällt, zeigen die Logs exakt die Registerwerte.

**Nächster Schritt:** Erst die zwei kostenlosen Log-Checks oben. Dann ein gebündelter
ATmega-Reflash mit gehärteter Recovery + Diagnose + Heartbeat.

---

## Log-Auswertung (echte SD-Logs LOG00013+00014, ~144k Zeilen)

Ausgewertet mit `analyze_log.py`. **Diagnose Bild B jetzt hart bestätigt:**

**Beweise:**
1. **`state=` immer STANDBY** (68463×, kein einziges IDENT/RESET) → ESP war durchgehend
   bereit, kein State-Hänger, kein OTA-Bug. Check 1 negativ.
2. **`RFID reinit dead` = 0** → VersionReg war nie tot, SPI lebte. Check 2 negativ
   → **„alive-but-not-reading"** bestätigt.
3. **Neueste Datei LOG00014 (letzte ~1h40m, up=279523→285573, KEIN Reboot):
   NULL Kartenlesungen**, aber `[ATM] REED=0/REED=1` um „07:51" → Tür wurde **physisch
   benutzt, ohne dass eine Karte gelesen wurde**. ATmega lebte (REED kam durch,
   `atmega=OK` durchgehend), nur der RC522 las nichts. → Lehrbuch-Bild-B.
   Letzte erfolgreiche Lesung noch in LOG00013; RC522 starb um den Übergang 13→14
   und blieb ≥1h40m tot — ohne Reboot, ohne VersionReg-Fehler.

**Die 4× `atmega=DEAD` sind KEINE echten Tode:** alle mit `up=17..33` (kurz nach Reboot)
und geclustert mit 4 `HEALTH restart` (00:40–00:54). Nach `ESP.restart()` ist
`lastPongMs=0` und `setup()` blockiert bis 25s im `connectWiFi()`, bevor das erste
`PING` rausgeht → HB zeigt zwangsläufig kurz DEAD bis zum ersten PONG. Post-Reboot-Artefakt.

### Nebenbefunde (NICHT der Tür-Ausfall, aber echte Reliability-Themen)
- **Schwaches WiFi: RSSI durchgehend −74…−78 dBm.** → 192× `SLOWLOOP` (Loop-Stall,
  max **10,7 s**!), meist WiFi-Reconnect/DHCP-Blockade im Loop.
- **Server nachts zeitweise weg:** 27× `HEALTH fail`, 4× `HEALTH restart` → ESP hat sich
  4× in 13 Min neu gestartet (00:40–00:54). Reboot hilft nicht, wenn nur der *Server*
  weg ist → Threshold/Logik überdenken (nur bei echtem WiFi-Problem rebooten, nicht bei
  Server-Ausfall). ESP-seitig, per OTA fixbar.
- **Bus-Korruption:** 13 Zeilen mit nicht-druckbaren Bytes (`…??OK`) in LOG00013 → EMV/
  Timing-Glitches auf dem UART (selten, ~0,01%). Gleiche Umwelt-Familie wie der RC522-Tod.
- Kein `MOTOR:TIMEOUT` → Motor unauffällig.
- Max. Uptime **~3,3 Tage am Stück** ohne Reboot → ESP-Firmware grundsätzlich stabil.

### ENTSCHIEDENER Fix (Sub-Fall „alive-but-not-reading")
ATmega-Reflash (vor Ort, USB+DIP), gebündelt:
1. **Unbedingtes volles RC522-Re-Init** periodisch im Idle (nicht nur bei VersionReg
   0x00/0xFF) + **Hard-Reset via RST-Pin** (Pin 5 kurz LOW→HIGH) statt nur Soft-Reset.
2. **Re-Init nach jeder Motorfahrt** (EMV-Quelle Stepper).
3. **Register-Diagnose loggen**: `ATMDBG:RFID ver=.. tx=..` (VersionReg + TxControlReg-
   Antennenbits) periodisch → nächster Ausfall zeigt die Frontend-Register direkt.
4. **ATmega-Selbst-Heartbeat** `ATMDBG:ALIVE n=..`.

Optional ESP-seitig (OTA, ohne Reflash): HEALTH-Restart-Logik entschärfen; WiFi-Signal
am Standort verbessern (−78 dBm ist grenzwertig).

---

## UMGESETZT (2026-07-02) — ATmega-Patch, Build OK

Alle vier Punkte im ATmega (`src_atmega/`), kompiliert sauber
(`pio run -e door_01_atmega_usb` → RAM 25,6 %, Flash 42,6 %). **Noch nicht geflasht.**

1. **`rfidHardReset()`** — Hard-Reset über RST-Pin (Pin 5: LOW→HIGH→50ms) + volles
   `PCD_Init()` (alle Register neu, Antenne an). Gründlicher als der bisherige Soft-Reset.
2. **`pollRFID()`**: alten VersionReg-Check ersetzt durch **unbedingtes `rfidHardReset()`
   alle `RFID_REINIT_INTERVAL_MS` (10s)** → heilt den „alive-but-not-reading"-Fall.
   Max. Tür-Totzeit nach Glitch = 10s (statt ≥1h40m).
3. **Re-Init nach jeder Motorfahrt** in `runOpenDoor()`/`runCloseDoor()` (EMV-Quelle).
4. **`sendAtmegaHeartbeat()`** alle `ATMEGA_HB_INTERVAL_MS` (5s):
   `ATMDBG:ALIVE n=<loops> up=<s> ver=<hex> tx=<hex> [FAULT]` → im SD-Log als
   `[ATM] ALIVE …`. Liveness + RC522-Registerdiagnose. `FAULT` = ver 00/ff oder Antenne aus.

Neue Settings: `RFID_REINIT_INTERVAL_MS=10000`, `ATMEGA_HB_INTERVAL_MS=5000`
(bewusst verschieden, damit der HB die Register auch vor dem Re-Init samplet).

`analyze_log.py` erweitert: eigene Sektion „ATmega-Heartbeat + RC522-Register" —
zeigt ver/tx-Histogramm, listet `FAULT`-Zeilen und ATmega-HB-Lücken.

### Deployment (vor Ort, ATmega ist NICHT OTA)
```bash
# DIP: SW3+SW4 ON (ATmega flashen), Rest OFF
pio run -e door_01_atmega_usb --target upload
# danach DIP zurück auf Normal-Betrieb: SW1+SW2 ON
```
Für Tür 02: `door_02_atmega_usb`.

### Verifikation nach Flash
Im Telnet/SD-Log müssen jetzt regelmäßig `[ATM] ALIVE … ver=91 tx=83`-Zeilen erscheinen
(ver=91/92, tx mit gesetzten Antennenbits). Taucht künftig ein Ausfall auf, zeigt eine
`ALIVE … FAULT`-Zeile den RC522-Registerzustand direkt — dann wissen wir, ob der
Hard-Reset das Frontend nicht heilt (→ Hardware/Power) oder ob es an etwas anderem liegt.

### Status
- **tuer-01: geflasht & verifiziert (2026-07-02)** — Telnet zeigt `[ATM] ALIVE n≈195
  up=.. ver=92 tx=83` alle 5s, kein FAULT, atmega=OK. RC522 gesund, Heartbeat +
  10s-Re-Init laufen. Baseline `n≈195` Loops/5s = gesunder Idle.
- **tuer-02: noch flashen.**

### Offen / später
- Bewährungsprobe: ob der Ausfall jetzt ausbleibt. Falls doch → `ALIVE … FAULT`-Zeile
  im Log zeigt den RC522-Registerzustand direkt.
- ESP-seitig per OTA: HEALTH-Restart entschärfen (nicht rebooten, wenn nur der Server
  weg ist), WiFi-Signal am Standort verbessern (−78 dBm).

## Maßnahmen, priorisiert

### 1. Sofort, ESP-only, per OTA (kein Reflash) — Observability
- `OK→DEAD`-Übergang als eigenes getimestamptes Event loggen
  („ATMEGA DEAD, letztes PONG vor X s") statt nur als HB-Feld → exakter Ausfallzeitpunkt.
- Rohe, nicht-gematchte ATmega-Zeilen mitloggen statt verwerfen
  (aktuell still verworfen in `processSerial()`, [src_esp/main.cpp:243](src_esp/main.cpp#L243))
  → Bus-Korruption wird sichtbar.
- Deploy: `pio run -e door_01_esp_ota --target upload`
- **Status: noch nicht umgesetzt** (Claude kann das schreiben, wenn gewünscht).

### 2. Messlücke physisch schließen
- Zusätzlich die **ATmega-TX-Leitung** in den Logger abgreifen (Y-Split / 2. Logger).
- Erst damit lässt sich „hängt vs. Draht tot" hardwareseitig trennen
  (Originalworte des ATmega statt nur ESP-Echo).

### 3. ATmega-Selbst-Heartbeat (braucht EINEN Reflash vor Ort)
- `ATMDBG:ALIVE n=<loopcount> up=<ms>` alle ~2 s → „loopt noch" direkt sichtbar,
  trennt A/B sofort.
- Gleichzeitig RC522-Recovery härten: voller Power-Down/Reset statt nur `PCD_Init()`,
  und gelesenen `VersionReg`-Wert mitloggen (Degradation sichtbar).
- Lohnt sich, da Fehler trotz RC522-Healthcheck wiederkam → ein letzter ATmega-Flash.

### 4. Strukturelle Lücke: ESP kann toten ATmega nicht heilen
- ESP **erkennt** `atmega=DEAD`, tut aber nichts. Ein ATmega, der seinen eigenen
  4-s-WDT verhungern lässt, ist remote nicht heilbar.
- Saubere Lösung: **ESP-GPIO → ATmega-RESET** verdrahten (kleine Drahtbrücke;
  ab Werk nicht vorhanden, DIP-Schalter routen nur TXD0/RXD0).
  Dann kann der ESP bei `atmega=DEAD` automatisch hart resetten.
- Das ist die zu treffende Architektur-Entscheidung für echte Selbstheilung.

## Hängt-Kandidaten ATmega (falls Bild A)

- **RC522/SPI-Wedge** (#1 Verdacht): Library-Calls blockieren meist nicht, sondern
  geben `false` → ATmega loopt weiter → wäre eher Bild B. Aber wedged Bus + ggf. Hang.
- **Brownout durch Motorstrom** → Reset (`BOOT reason=BOD`). Wenn Reflexion fehlt = Link-Problem.
- `runCloseDoor()` Reed-Wartung bis 30 s ([src_atmega/main.cpp:290](src_atmega/main.cpp#L290))
  — blockiert, aber mit Timeout; kein Dauer-Hang.
- 64-Byte-UART-RX-Überlauf, wenn ATmega lange blockiert (ESP spammt PING/HB/DBG)
  → verlorene/korrupte Bytes; meist benigne, aber Bus-Korruption möglich.

## Nächster konkreter Schritt

1. **Log auswerten** (Schritt 0) → Bild A oder B festhalten.
2. Je nach Ergebnis: RC522 härten (B) oder ATmega-Hang/Reset jagen (A).
3. Parallel kann Punkt 1 (ESP-Log-Verbesserungen, OTA) sofort umgesetzt werden.
