# Machine Node / RFID_BOX — Layoutregeln

Breiten, Via-Größen, Domänen und Abstände. Die
[Abstandsmatrix](#3-domänen-und-abstände) ist die verbindliche Quelle;
`ECAD/IO_Board/IO_Board.kicad_dru` bildet sie ab.

Was das Gerät ist und tut, steht in der [README](README.md), die
Fehlerbetrachtung in [FMEA.md](FMEA.md).

## 1. Fertigungsbasis

| Board | Lagen | Kupfer |
|---|---|---|
| I/O-Board 230V | 2 | **70 µm (2 oz)** — wegen des 8-A-Lastpfads |
| I/O-Board 24V | 2 | 35 µm |
| RFID-Board | **4** | 35 µm außen, 17,5 µm innen |

Alle FR4, 1,6 mm. Untergrenzen: 0,2 mm Bahn/Abstand, 0,3 mm Bohrung, 0,5 mm
Via, 0,5 mm Kupfer zur Boardkante, 1,0 mm Fräsnut.

Breiten nach IPC-2221, ΔT = 5 K. Auf Innenlagen halbiert sich der Faktor:
dort braucht eine Bahn rund das Fünffache — Strom auf Innenlagen deshalb nur
als Fläche.

| Via | Pad | Verwendung |
|---:|---:|---|
| **⌀0,2 mm** | 0,45 mm | enge Bereiche mit Kleinstströmen |
| **⌀0,3 mm** | 0,6 mm | Signale, GND-Stitching |
| **⌀0,6 mm** | 1,0 mm | Versorgungsschienen |
| **⌀1,0 mm** | 1,4 mm | Lastpfad |

Restring ≥ 0,125 mm je Seite. Stückzahlen in den Tabellen mit Faktor 0,7 für
eng gesetzte Via-Felder gerechnet.

## 2. Leiterbahnbreiten

### 2.1 I/O-Boards — 2 Lagen

**P:** Primärseite, Netzpotential · **S:** SELV · **F:** Feldseite, extern
gespeist. Die ISO-Insel gehört trotz 3,3-V-Pegeln zur Primärseite.

| Klasse | Netze | Breite 230V / 24V | Vias | Board |
|---|---|---:|---|---|
| **P:** Lastpfad | `PIN_L`, `PIN_N`, `PIN_FUSED_L`, `PIN_*_SWITCHED` | 4,0 mm Polygon | kein Lagenwechsel; unvermeidbar **5 × ⌀1,0 mm** | nur 230V |
| **P:** PE | `PIN_PE` | 4,0 / 1,0 mm | wie Lastpfad | beide |
| **P:** Eingang + Steuerzweig | `PIN_CTRL_L/N`, auf dem 24V-Board auch `PIN_L`, `PIN_N` | 1,0 mm | **1 × ⌀0,6 mm** | beide |
| **P:** ISO-Insel | `+3V3_ISO`, `GND_ISO`, `GND2_ISO`, UART, Shunt-Sense | 0,4 mm | **1 × ⌀0,2 mm** | nur 230V |
| **P:** GND ISO-Insel | `GND_ISO` | eigene Massefläche | Stitching ⌀0,2 mm | nur 230V |
| **S:** 24 V | `+24V` | 0,8 mm | **1 × ⌀0,6 mm** | beide |
| **S:** 5 V | `+5V` | 1,0 / 1,3 mm | **2 × ⌀0,6 mm** | beide |
| **S:** 3,3 V | `+3V3` | 0,5 mm | **1 × ⌀0,3 mm** | nur 230V |
| **S:** Signale | Enable, `ISO_EN`, UART, Kabel zum RFID-Board | 0,3 mm | **1 × ⌀0,3 mm** | beide |
| **S:** GND | `GND` | Massefläche | Stitching ⌀0,3 mm, Raster ≤ 10 mm | beide |
| **F:** Kontakt K2 | `C`, `NO`, `NC` | 0,5 mm | **1 × ⌀0,3 mm** | nur 24V |

- Lastpfad als Polygon ohne Lagenwechsel; an unvermeidbaren Engstellen
  mindestens 2,65 mm, Bauteilpads ausgenommen.
- R307 in Kelvin-Anbindung, Sense-Abgriffe direkt an den Pads, je ein Ferrit
  (FB301, FB302) in beiden Sense-Leitungen.
- Ferrite am Isolator FB303–FB307: VISOOUT→VISOIN, GND2_ISO→GND_ISO und in
  den Zuleitungen VIO und VDD.
- Abblockung am Isolator je Versorgungspin 10 nF + 1 µF + 10 µF, **die 10 nF
  weniger als 1 mm vom Pin**, 0603.
- Entladekette an C300 aus drei Widerständen in Reihe, je mit Grenzspannung
  ≥ 150 V.
- Keine ISO-Insel auf dem 24V-Board; der Relaistreiber ist auf beiden Boards
  redundant aus Q301 und Q300 aufgebaut.
- Die 8-mm-Barriere gilt auch auf dem 24V-Board, dort nur um Eingangsbereich
  und U300 herum.
- Die Abschnitte F302 → L300 und Shunt → Lastrelais tragen kein eigenes
  Netzlabel.

### 2.2 RFID-Board — 4 Lagen

Lagen: **L1** Signal + Bauteile · **L2** GND durchgehend · **L3** Flächen
`+3V3` / `+5V` · **L4** Signal + Kabelstecker.

| Klasse | Netze | Breite | Vias |
|---|---|---:|---|
| 5 V | `+5V` (LED-Streifen, Buzzer) | 1,3 mm | **2 × ⌀0,6 mm** |
| 3,3 V | `+3V3` (~285 mA Mittel, ~630 mA Spitze) | 0,5 mm | **2 × ⌀0,3 mm** |
| Antennentreiber | TX1/TX2 → Matching → Antenne | ≥ 0,8 mm, symmetrisch | **keine** |
| Antennenschleife | — | 1,0 mm, 0,5 mm Abstand | **keine** |
| LED-Daten | Single-Wire nach 74AHCT125, 5 V | 0,25 mm | **1 × ⌀0,3 mm** |
| Signale | SPI, I2C, UART, GPIO, IRQ/RST | 0,25 mm | **1 × ⌀0,3 mm** |
| USB D+/D− | Service-Buchse J400 | 0,25 mm, eng gekoppelt | **keine** im Paar |
| GND | `GND` | Lage L2, **unter der Antenne ausgespart** | Stitching ⌀0,3 mm, ≤ 5 mm am Modulrand |

- Strom auf L2/L3 nur als Fläche, nie als Bahn.
- L2 nicht aufschneiden — **außer unter der Antenne**, dort muss die Fläche
  auf allen vier Lagen weg. Signale dürfen die Aussparung nicht queren.
- **Zwei Keepouts auf allen vier Lagen:** WROOM-Modul und die 80 × 50 mm der
  RFID-Antenne. Auslegung der Antenne in
  `ECAD/RFID_Board/antenna/README.md`.
- TX1/TX2 symmetrisch und gleich lang, Matching dicht am CLRC663, keine Vias
  im TX-Pfad.
- **Kupferfläche am Tab des 3V3-Reglers**, mindestens 100 mm² auf der
  3,3-V-Fläche. Ohne sie schaltet der Regler bei Lastspitzen ab.
- USB Full Speed, keine Impedanzkontrolle: symmetrisch, < 50 mm, durchgehend
  über L2, keine Stubs.

## 3. Domänen und Abstände

Betrifft nur das I/O-Board; das RFID-Board liegt vollständig in SELV
(0,2 mm durchgehend).

| Domäne | umfasst |
|---|---|
| **L**, **N** | Netzpole vor und hinter dem Relais, Netzklassen über `/PIN_L*` und `/PIN_N*` |
| **PE** | Schutzleiter, über C305/C308 mit dem Steuerzweig gekoppelt — die beiden sind deshalb zwingend Y2 |
| **ISO-Insel** | `+3V3_ISO`, `GND_ISO`, `GND2_ISO`, UART zu U301, Shunt-Sense. Isoliert **gegen SELV**, nicht berührsicher: hängt über R307 am Lastpfad und über NT300 auf N-Potential |
| **SELV** | 24 V / 5 V / 3,3 V, Logik, Relaistreiber, gesamtes RFID-Board |
| **FELD** | extern gespeiste Seite der Erweiterungsboards |

Normbasis IEC 60664-1, 250 V AC, Überspannungskategorie II,
Verschmutzungsgrad 2, Werkstoffgruppe IIIa.

Abstandsmatrix, Luft- **und** Kriechstrecke:

| | L | N | ISO-Insel | SELV | PE | FELD |
|---|---:|---:|---:|---:|---:|---:|
| **L** | 0,5 mm | 3,0 mm | 3,0 mm | 8,0 mm | 3,0 mm | 8,0 mm |
| **N** | | 0,5 mm | 0,5 mm | 8,0 mm | 3,0 mm | 8,0 mm |
| **ISO-Insel** | | | 0,3 mm | 8,0 mm | 3,0 mm | 8,0 mm |
| **SELV** | | | | 0,2 mm | 0,2 mm | 2,0 mm |
| **PE** | | | | | — | 8,0 mm |

Die 0,5 mm gelten zwischen verschiedenen Netzen desselben Pols. Eine
Ausnahme trägt der Regelsatz: innerhalb von U301 gelten 0,2 mm, weil dort
die Isolation vom Bauteil kommt und die Pads nur 7,25 mm auseinander liegen.

- Die 8,0 mm liegen über der Norm und kommen von den Bauteilen (K300, U301).
  **Nicht auf 5 mm reduzieren.**
- Die ISO-Insel wird komplett von der Barriere umschlossen, auch wo auf ihr
  nur 3,3 V liegen.
- `GND_ISO` und `GND` als getrennte Flächen mit definiertem Umriss statt
  freier Füllung — ihre Kanten sind die längste Strecke der Barriere.
- Keine Vias in der Barriere; die 8 mm gelten auf beiden Lagen.
- Die 8 mm gelten auch für das **Verbindungskabel** zum RFID-Board: Stecker
  und Kabelführung halten 8 mm zu Netzkupfer. Querschnitt und Länge des
  Kabels sind getrennt zu bemessen, die Breiten oben enden am Stecker.
- Netzkupfer inkl. ISO-Insel ≥ 2,0 mm zu Kante, Bohrungen und Fräsungen;
  SELV ≥ 0,5 mm. Fräsnuten ≥ 1,0 mm breit.
- **Kein Kupfer im Umkreis von 4 mm um VISOOUT und GND2 von U301** (Vorgabe
  des Datenblatts, Bedingung für CISPR 32 Class B).
- `GND2_ISO` und `GND_ISO` über einen Ferrit trennen; `GND_ISO` in
  Kelvin-Anbindung hart am Shunt-Knoten.
- L300 trennt die Wicklungen nur über das Rastermaß 4,0 mm ±0,5 — erfüllt
  die 3,0 mm, bei einem Typwechsel gegenprüfen.
- Lötstopplack, Bestückungsdruck und Schutzlack zählen nicht zur
  Kriechstrecke.
