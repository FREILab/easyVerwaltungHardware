# Review RFID_Board

- **Reviewer:** Marc Ihle
- **Datum:** 22.11.2026 *(im Original-CSV: 22.11.2028; Titel im Original-CSV versehentlich „Review IO_Board“)*
- **Status:** Offen (noch nichts umgesetzt)

Skala **Wichtigkeit:** `1` = Änderung nice to have … `5` = Änderung absolut notwendig

## Übersicht

| ID | Wichtigkeit | SCH/BRD | Position (x / y) | Bauteil | Problem | Status |
|---|:---:|:---:|---|---|---|---|
| `RFID-01` | 1 | SCH 1 | — | `IC101B/C` | Buzzer-Treiber benötigt 2 uC-Ausgänge; `SN74AHCT125` liefert nur 5 mA | Offen |
| `RFID-02` | 1 | SCH 2 | — | `IC200` | `VDD` aktuell 3,3 V statt 5,0 V typ. (3,0–5,5 V laut Datenblatt) | Offen |
| `RFID-03` | 3 | SCH 1, 4 | — | `IC100A`, `D400` | Keine Stroko zwischen Schutzdiode `D400` und ESP (`IC100A`) | Offen |
| `RFID-04` | 3 | SCH 1, 4 | — | `D401` | `VBUS` ist über `D401` direkt mit `+5V` verbunden (kein Überspannungsschutz) | Offen |
| `RFID-05` | 1 | SCH 4 | — | `C400`, `R404` | Rückfrage: Wieso ist Shield über RC-Glied statt direkt mit `GND` verbunden? | Offen |
| `RFID-06` | — | SCH 4 | — | `J400` | Info: USB-C-Buchse fast baugleich auch von Würth (`629722000214`) | Offen |
| `RFID-07` | 2 | SCH | — | — | Allgemein: Ferrit-Bead/Drossel zwischen `+3V3` und Bypass-Cs erwägen | Offen |
| `RFID-08` | 2 | BRD | `6,6205; 7,0925` / `3.3996` | — | `GND`- und Versorgungs-Planes nicht gut gegeneinander mit Cs verkoppelt | Offen |
| `RFID-09` | 2 | BRD | — | — | RXT-Leitungen (`RX+`/`RX-`) asymmetrisch verlegt | Offen |
| `RFID-10` | 4 | BRD | — | — | `SPI-MOSI` und `SCK` laufen im Spulenbereich (ohne `GND`-Fläche darunter) | Offen |
| `RFID-11` | 2 | BRD | — | — | Vias unter dem USB-C-Connector zu dicht beieinander (`GND`-Fläche unterbrochen) | Offen |
| `RFID-12` | 2 | BRD | — | — | `C101` zu weit von `SW100` entfernt | Offen |
| `RFID-13` | 1 | BRD | — | — | Reset-Taster ohne ESD-Schutz (`GND`-Anschluss) | Offen |
| `RFID-14` | — | BRD | — | — | Frage: Löcher für Montageschrauben vorsehen? | Offen |
| `RFID-15` | — | — | — | — | Frage: Angebot von Würth für identische/ähnliche Bauteile einholen? | Offen |

## Details

### Schema (SCH)

#### `RFID-01` — Buzzer-Treiber `IC101B/C` (Wichtigkeit 1, SCH 1, `IC101B/C`)
- **Problem:** Der Buzzer-Treiber benötigt 2 Ausgänge vom uC. Der `SN74AHCT125` liefert nur 5 mA Ausgangsstrom. Reicht das für den Buzzer?
- **Begründung:** Es sind zwar noch IO-Pins am uC übrig, aber verschwenden muss man sie ja nicht…
- **Vorschlag:** `74AUP2G04` verwenden: Der hat 20 mA Ausgangsstrom und invertiert. Damit kann aus einem Eingangssignal ein invertiertes und ein nicht-invertiertes Ausgangssignal (2× invertiert) erzeugt werden. Sofern `RGB_LED_5V` auch getrieben werden soll, kann der `74AUP3G04` verwendet werden. Dann muss der uC das Signal invertieren.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-02` — Versorgungsspannung `IC200` (Wichtigkeit 1, SCH 2, `IC200`)
- **Problem:** Laut Datenblatt ist der Versorgungsspannungsbereich von `VDD` 3,0 V bis 5,5 V mit 5,0 V typ. Wir verwenden aktuell 3,3 V.
- **Begründung:** Ob der Chip besser läuft, wenn wir 5,0 V nehmen?
- **Vorschlag:** Wir könnten einen 0-Ohm-Switch vorsehen (2 Widerstände, von denen nur einer bestückt wird; ggf. mit überlappendem Pad), mit dem man zwischen 3,3 V und 5,0 V wählen kann.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-03` — Fehlende Stroko zwischen `D400` und `IC100A` (Wichtigkeit 3, SCH 1, 4, `IC100A`, `D400`)
- **Problem:** Zwischen der Schutzdiode `D400` und dem ESP (`IC100A`) befindet sich keine Stroko.
- **Begründung:** Gleichtakt-Störer und ESD-Pulse können (trotz TVS-Diode) bis zum ESP vordringen.
- **Vorschlag:** Einbau einer Stroko, z. B. `7442335900` von Würth, zwischen ESP und `D400`.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-04` — Überspannungsschutz an `VBUS` / `D401` (Wichtigkeit 3, SCH 1, 4, `D401`)
- **Problem:** `VBUS` ist über `D401` direkt mit `+5V` verbunden.
- **Begründung:** Bei Überspannung am USB-C-Anschluss kann das Teile der Schaltung töten.
- **Vorschlag:** Einbau einer TVS-Diode von `VBUS` gegen `GND`. Die begrenzt dann die `VBUS`-Spannung.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-05` — Shield-Anbindung über RC-Glied (Wichtigkeit 1, SCH 4, `C400`, `R404`)
- **Problem:** Wieso ist Shield nicht direkt mit `GND` verbunden?
- **Begründung:** Ich sehe keinen Vorteil darin, den Shield über ein RC-Glied anzuschließen. Gibt es da Erkenntnisse, die ich noch nicht kenne?
- **Status:** Offen
- **Kommentar:** —

#### `RFID-06` — Alternative USB-C-Buchse `J400` (Info, SCH 4, `J400`)
- **Hinweis:** Nur zur Info: Den gibt es auch fast baugleich von Würth: `629722000214`.
- **Begründung:** Ich denke, dass wir die von Würth kostenlos oder sehr günstig bekommen könnten (je nach Menge).
- **Status:** Offen
- **Kommentar:** —

#### `RFID-07` — Ferrit-Bead/Drossel an `+3V3` (Wichtigkeit 2, SCH)
- **Problem:** Allgemein: Evtl. zwischen `+3V3` und den Bypass-Cs eine Ferrit-Bead/Drossel vorsehen.
- **Begründung:** Damit könnten störende Netze und empfindliche Netze besser voneinander separiert werden. Zudem können einzelne Ferrit-Beads bei der Inbetriebnahme entfernt werden, um einzelne Netze testweise zu trennen.
- **Vorschlag:** Z. B. `742792017` von Würth vorsehen. Der hält 3,5 A aus und hat die Bauform 0805. Kann also auch gegen einen 0-Ohm-Widerstand getauscht werden. (Von der Serie gibt es jede Menge Induktivitätswerte, falls der vorgeschlagene nicht passt. Auch in anderen Baugrößen vorhanden.)
- **Status:** Offen
- **Kommentar:** —

### Layout (BRD)

#### `RFID-08` — Verkopplung `GND`- und Versorgungs-Planes (Wichtigkeit 2, BRD, x = `6,6205; 7,0925` / y = `3.3996`)
- **Problem:** Die `GND`- und Versorgungsspannungs-Planes sind nicht gut gegeneinander mit Cs verkoppelt.
- **Begründung:** Damit die beiden Lagen nicht gegenseitig aufschwingen können, sollten sie mit Bypass-Cs verbunden werden. Bei kleinen Leiterplatten wie dieser reicht die Kapazität der Planes nicht unbedingt aus.
- **Vorschlag:** In der Nähe von `J101` einen zusätzlichen Bypass-C anbringen, ebenso über den Schlitz zwischen `GND` und `+3V3` 2 Stück (siehe Koordinaten). Am unteren Rand und am rechten Rand die beiden `GND`-Flächen mit Vias verbinden.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-09` — RXT-Leitungen asymmetrisch verlegt (Wichtigkeit 2, BRD)
- **Problem:** RXT-Leitungen asymmetrisch verlegt.
- **Begründung:** Unterschiedliche Kapazitäten und Induktivitäten der Leitungen können evtl. zu Phasenverschiebungen und damit zu Empfangsproblemen führen.
- **Vorschlag:** `RX+` und `RX-` jeweils als gleich dicke Leitungen verlegen.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-10` — `SPI-MOSI` und `SCK` im Spulenbereich (Wichtigkeit 4, BRD)
- **Problem:** `SPI-MOSI` und `SCK` laufen im Spulenbereich.
- **Begründung:** Es fehlt eine `GND`-Fläche unter den Signalleitungen. Damit muss der Rückstrom einen Umweg machen, was zu Induktivität und Einkopplung der NFC-Frequenz und anderer HF-Störungen führen kann.
- **Vorschlag:** Die 6 Datenleitungen so weit nach rechts ziehen, dass sie über der `GND`-Plane liegen. Der Platz dafür ist ja da. Dabei die 6 Vias so weit auseinanderziehen, dass kein Schlitz in der `GND`-Fläche entsteht, sondern der `GND` um jede Via herumlaufen kann.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-11` — Vias unter dem USB-C-Connector zu dicht (Wichtigkeit 2, BRD)
- **Problem:** Vias unter dem USB-C-Connector zu dicht beieinander.
- **Begründung:** Die `GND`-Fläche wird unterbrochen, wodurch der Rückstrom einen Umweg nehmen muss.
- **Vorschlag:** Vias etwas nach oben auseinanderziehen.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-12` — `C101` zu weit von `SW100` entfernt (Wichtigkeit 2, BRD)
- **Problem:** `C101` zu weit von `SW100` entfernt.
- **Begründung:** Für einen ESD-Schutz beim Berühren des Tasters (siehe Punkt 13) kann `C101` den Impuls gegen `GND` ableiten. Dazu muss eine induktionsarme Verbindung zu `GND` bestehen.
- **Vorschlag:** Plätze von `R100` und `C101` tauschen. Damit wird die Leitungslänge deutlich verkürzt.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-13` — Reset-Taster ohne ESD-Schutz (Wichtigkeit 1, BRD)
- **Problem:** Reset-Taster ohne ESD-Schutz.
- **Begründung:** Wenn jemand elektrostatisch aufgeladen den Taster drückt, soll es ja nur zum Reset, aber nicht zum Totalausfall kommen ;-) `C101` verhindert schon weitgehend einen kritischen Überschlag zu `IC100`.
- **Vorschlag:** Z. B. `435151014824` von Würth vorsehen. Dieser Taster hat einen `GND`-Anschluss und verhindert so einen ESD-Überschlag auf kritische Leitungen.
- **Status:** Offen
- **Kommentar:** —

#### `RFID-14` — Montageschrauben-Löcher (Frage, BRD)
- **Frage:** Sollten wir Löcher für Montageschrauben vorsehen?
- **Status:** Offen
- **Kommentar:** —

### Allgemein

#### `RFID-15` — Angebot von Würth für Bauteile (Frage)
- **Frage:** Einige der verwendeten Bauteile gibt es identisch oder sehr ähnlich auch von Würth. Macht es Sinn, sich ein Angebot geben zu lassen?
- **Begründung:** Identische oder fast gleiche Bauteile: Dioden, Kondensatoren, Widerstände, Taster, Connectors, LEDs. Ähnliche Bauteile: Funk-Chips mit ESP32.
- **Status:** Offen
- **Kommentar:** —
