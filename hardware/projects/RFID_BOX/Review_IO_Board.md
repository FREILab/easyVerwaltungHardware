# Review IO_Board

- **Reviewer:** Marc Ihle
- **Datum:** 22.11.2026 *(im Original-CSV: 22.11.2028)*
- **Status:** Alle 9 Punkte umgesetzt, bereit zur Delta-Review (Antworten auch in [Review_Marc_Ihle.xlsx](Review/Review_Marc_Ihle.xlsx))

Skala **Wichtigkeit:** `1` = Änderung nice to have … `5` = Änderung absolut notwendig

## Übersicht

| ID | Wichtigkeit | SCH/BRD | Position (x / y) | Bauteil | Problem | Status |
|---|:---:|:---:|---|---|---|---|
| `IO-01` | 3 | SCH | — | — | Es fehlt ein Y-Kondensator von mind. 1 nF / 250 VAC zwischen `GND` und `PIN_PE` | Umgesetzt |
| `IO-02` | 3 | BRD | — | `R302` | Abstand von `Net-(R301-Pad2)` zu `GND_ISO` mit 0,68 mm ist gering | Umgesetzt |
| `IO-03` | 1 | BRD | — | `C319` | Via zu `GND_ISO` zu weit weg vom Pad | Umgesetzt |
| `IO-04` | 1 | BRD | — | `C320` | Via zu `GND_ISO` zu weit weg vom Pad | Umgesetzt |
| `IO-05` | 1 | BRD | — | `C322` | Via zu `GND_ISO` zu weit weg vom Pad | Umgesetzt |
| `IO-06` | 1 | BRD | `142.73` / `73.245` | — | GND-Fläche unterbrochen | Umgesetzt |
| `IO-07` | 1 | BRD | `142.84` / `66.875` | — | GND-Fläche eingeschnürt | Umgesetzt |
| `IO-08` | 1 | BRD | — | `C321` | `C321` induktiv angebunden an `U303` | Umgesetzt |
| `IO-09` | 1 | BRD | — | `R307` | Wenig Kupferfläche an den Pins | Umgesetzt |

## Details

### Schema (SCH)

#### `IO-01` — Y-Kondensator zwischen `GND` und `PIN_PE` (Wichtigkeit 3, SCH)
- **Problem:** Es fehlt ein Y-Kondensator von mind. 1 nF 250 VAC zwischen `GND` und `PIN_PE`.
- **Begründung:** Das Schaltnetzteil `U300` wird eine Gleichtakt-Störspannung zwischen Primär- und Sekundärseite erzeugen. (MeanWell-Netzteile sind bekannt dafür, dass sie zwar ein CE-Kennzeichen tragen, aber bei EMV-Prüfern immer wieder ein Stirnrunzeln bis hin zu Kopfschütteln induzieren.) Die Primärseite ist über die Y-Kondensatoren `C305` und `C308` für HF hart mit PE verbunden. Die Störspannung wird sich somit über `C309` sekundärseitig auf `GND` übertragen. Über ein Kabel an `J302` (Antenne) kann sich die Störung gut nach außen übertragen.
- **Vorschlag:** Möglichst nah am GND-Anschluss von `U300` und `C309` einen Y-Kondensator anbringen, der möglichst direkt mit den `PIN_PE`-Anschlüssen von `C305` und `C308` verbunden ist. (Direkt rechts neben `C305` platzieren. Das kann aber eng werden mit den Isolationsabständen...) Die PE-Leitung zwischen den Kondensatoren dick (flächig) auslegen. Die aktuelle PE-Leitung von `C305` ist zu dünn.
- **Status:** Umgesetzt
- **Kommentar:** (umgesetzt) `C330` (Y2, 1 nF / 300 VAC, Würth `890404123001CS`, IEC 60384-14) direkt neben `C305`/`U300` zwischen `GND` und `PIN_PE` ergänzt. Die PE-Anbindung zwischen `C305`, `C308` und `C330` ist jetzt flächig als Kupferpolygon auf Top und Bottom ausgelegt; die 8-mm-Barriere zu Netz/SELV bleibt vollständig eingehalten.

### Layout (BRD)

#### `IO-02` — Abstand `R302` / `Net-(R301-Pad2)` zu `GND_ISO` (Wichtigkeit 3, BRD, `R302`)
- **Problem:** Abstand von `Net-(R301-Pad2)` zu `GND_ISO` mit 0,68 mm ist gering.
- **Begründung:** Am Netz `Net-(R301-Pad2)` liegt 1/3 der 230 VAC-Spannung an. Daher ist ein erhöhter Abstand zu `GND_ISO` sinnvoll. Abstand N–ISO: 0,5 mm; N–L: 3 mm. Vorschlag: Bei 1/3 der Netzspannung 3 mm / 3 = 1 mm vorsehen.
- **Vorschlag:** `R302` nach links verschieben, so dass sich der Abstand zur `GND_ISO`-Leitung vergrößert.
- **Status:** Umgesetzt
- **Kommentar:** (umgesetzt) `R302` nach links verschoben und die Leiterbahn `Net-(R301-Pad2)` angepasst. Der Abstand zu `GND_ISO` beträgt jetzt durchgehend ≥ 1,10 mm (vorher 0,68 mm), bei gleichzeitig vollem Abstand zu `R300`/`R301`.

#### `IO-03` — Via zu `GND_ISO` an `C319` (Wichtigkeit 1, BRD, `C319`)
- **Problem:** Via zu `GND_ISO` zu weit weg vom Pad.
- **Begründung:** Induktivität der Leitung (1 nH/mm) verschlechtert HF-Dämpfung des Filters.
- **Vorschlag:** Via(s) so nah wie möglich neben dem Pad platzieren, gerade so weit weg, dass kein Lot in das Via fließt. Idealerweise zwei Vias pro Pad nebeneinander anbringen.
- **Status:** Umgesetzt
- **Kommentar:** (umgesetzt) Zwei Vias (Ø 0,3/0,6 mm) direkt neben dem `GND_ISO`-Pad von `C319` platziert (mit ausreichend Lötstoppsteg gegen Lotfluss ins Via).

#### `IO-04` — Via zu `GND_ISO` an `C320` (Wichtigkeit 1, BRD, `C320`)
- **Problem:** Via zu `GND_ISO` zu weit weg vom Pad.
- **Begründung:** Induktivität der Leitung (1 nH/mm) verschlechtert HF-Dämpfung des Filters.
- **Vorschlag:** Via(s) so nah wie möglich neben dem Pad platzieren, gerade so weit weg, dass kein Lot in das Via fließt. Idealerweise zwei Vias pro Pad nebeneinander anbringen.
- **Status:** Umgesetzt
- **Kommentar:** (umgesetzt) Zwei Vias (Ø 0,3/0,6 mm) direkt neben dem `GND_ISO`-Pad von `C320` platziert.

#### `IO-05` — Via zu `GND_ISO` an `C322` (Wichtigkeit 1, BRD, `C322`)
- **Problem:** Via zu `GND_ISO` zu weit weg vom Pad.
- **Begründung:** Induktivität der Leitung (1 nH/mm) verschlechtert HF-Dämpfung des Filters.
- **Vorschlag:** Via(s) so nah wie möglich neben dem Pad platzieren, gerade so weit weg, dass kein Lot in das Via fließt. Idealerweise zwei Vias pro Pad nebeneinander anbringen.
- **Status:** Umgesetzt
- **Kommentar:** (umgesetzt) Zwei Vias (Ø 0,3/0,6 mm) direkt neben dem `GND_ISO`-Pad von `C322` platziert.

#### `IO-06` — GND-Fläche unterbrochen (Wichtigkeit 1, BRD, x = 142.73 / y = 73.245)
- **Problem:** GND-Fläche unterbrochen.
- **Begründung:** Ideal wäre eine durchgehende GND-Fläche ohne Schlitze. Da dies bei 2 Lagen nicht möglich ist, sollte versucht werden, Schlitzbereiche gering zu halten. Hier lässt sich das sehr einfach verbessern.
- **Vorschlag:** `+3V3`-Leitung: den diagonalen Streckenabschnitt nach unten verschieben. Damit rückt die Leitung weg von `relay_en_HS`, so dass Platz für die GND-Fläche entsteht.
- **Status:** Umgesetzt
- **Kommentar:** (umgesetzt) Diagonalabschnitt der `+3V3`-Leitung nach unten weg von `relay_en_HS` verschoben, sodass die GND-Fläche auf der Bottom-Seite wieder durchgehend verbunden ist.

#### `IO-07` — GND-Fläche eingeschnürt (Wichtigkeit 1, BRD, x = 142.84 / y = 66.875)
- **Problem:** GND-Fläche eingeschnürt.
- **Begründung:** Ideal wäre eine durchgehende GND-Fläche ohne Schlitze. Da dies bei 2 Lagen nicht möglich ist, sollte versucht werden, Schlitzbereiche gering zu halten. Hier lässt sich das sehr einfach verbessern.
- **Vorschlag:** `relay_en_LS` bei der oberen Via erst ein Stück horizontal führen. Damit wird der Steg der GND-Fläche breiter.
- **Status:** Umgesetzt
- **Kommentar:** (umgesetzt) `relay_en_LS` vom oberen Via zunächst horizontal geführt, wodurch der GND-Steg auf der Bottom-Seite deutlich breiter geworden ist.

#### `IO-08` — `C321` induktiv angebunden an `U303` (Wichtigkeit 1, BRD, `C321`)
- **Problem:** `C321` induktiv angebunden an `U303`.
- **Begründung:** Die HF-Unterdrückung im IC leidet, wenn der Eingang induktiv und mit langer Leitung angebunden ist.
- **Vorschlag:** `R313` und `C321` tauschen, so dass `C321` näher am IC liegt. Evtl. `C321` senkrecht stellen. Evtl. größere `GND_ISO`-Fläche auf Top-Seite vorsehen, so dass die `GND_ISO`-Pins der Cs rund um das IC induktivitätsarm mit den `GND_ISO`-Pins des ICs verbunden sind. Die Fläche großzügig mit Vias versehen.
- **Status:** Umgesetzt
- **Kommentar:** (umgesetzt) `R313` und `C321` getauscht, sodass `C321` direkt am Eingang von `U303` sitzt, mit kurzer Anbindung und zwei `GND_ISO`-Vias am Pad. Zusätzlich großflächiges `GND_ISO`-Polygon auf der Top-Seite rund um `U303` ergänzt (direkte Pad-Anbindung, 9 Stitching-Vias zur Bottom-Fläche; der 4-mm-Sperrbereich um `U301` bleibt unberührt).

#### `IO-09` — Wenig Kupferfläche an `R307` (Wichtigkeit 1, BRD, `R307`)
- **Problem:** Wenig Kupferfläche an den Pins.
- **Begründung:** SMD-Widerstände entwärmen sich hauptsächlich über die angebundene Kupferfläche. `R307` wird zwar nur 128 mW @ 8 A abführen, wird aber ohne Kühlfläche dennoch warm. Die beiden Leitungen an `R307` sind zwar so bemessen, dass sie sich bei 8 A nicht zu sehr selbst erwärmen, sind aber für die zusätzliche Wärmeabfuhr von `R307` etwas klein.
- **Vorschlag:** Das Polygon an den Pins vergrößern. Zusätzlich großzügig auf der Bottom-Seite Kupferfläche vorsehen und mit Vias nach oben verbinden.
- **Status:** Umgesetzt
- **Kommentar:** (umgesetzt) Kupferpolygone an beiden Pins von `R307` auf Top (70 µm) vergrößert und durch entsprechende Gegenflächen auf der Bottom-Seite ergänzt, jeweils mit 4 thermischen Vias (Ø 0,6/1,0 mm) pro Seite angebunden. Die Kelvin-Sense-Leitungen zu `FB302`/`FB305` bleiben oben sauber getrennt.
