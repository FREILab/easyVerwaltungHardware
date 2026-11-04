# Kostenoptimierung

Laufendes Kapitel. Hier steht, wo das Gerät teuer ist und was eine
Alternative kosten würde — sortiert nach Ersparnis je Gerät, die größte
zuerst.

Preise aus `MyCSLib/KiCad/parts_db.json`, Stand **2026-11-04**, jeweils der
günstigere Distributor bei **25 Stück**. Die erste Serie sind 30 Geräte.

> **Sicherheit geht vor Kosten.** Keine Position hier hebt eine Festlegung
> aus [Anforderungen.md](Anforderungen.md) oder dem Regelwerk im
> [README](README.md) auf. Was unten unter „Nicht verhandelbar" steht, ist
> entschieden und wird nicht erneut aufgerollt.

## Stand

Materialkosten je Gerät, gerechnet mit den Mengen für **30 Geräte** — je
Teilenummer wird die Menge über alle Stücklistenzeilen summiert und danach
die Staffelstufe bestimmt.

| Baugruppe | je Gerät |
|---|---:|
| I/O-Board 230V, 77 bepreiste Teile | 54,03 € |
| RFID-Board, NFC-Teil, 29 Teile | 7,77 € |
| MCU-Blatt, geplant | 16,29 € |
| Kabel und Stecker | 4,30 € |
| NFC-Blatt, noch generische Teile | 0,60 € |
| **Summe** | **83,00 €** |
| **30 Geräte** | **2.490 €** |

Nicht enthalten: **Display** (nicht bepreist, siehe Position 1),
Leiterplatten, Gehäuse, Bestückung.

Die acht teuersten Einzelposten tragen 54,99 € davon, also zwei Drittel:

| Teil | Menge | Stück | gesamt |
|---|---:|---:|---:|
| Lastrelais T92S11D12-24 | 1 | 20,15 € | 20,15 € |
| Feldklemmen WAGO 2604-1103 | 2 | 3,34 € | 6,68 € |
| NFC-Frontend CLRC66303HNY | 1 | 6,25 € | 6,25 € |
| AC/DC-Modul IRM-10-24 | 1 | 5,70 € | 5,70 € |
| Isolator ISOW7721DFMR | 1 | 5,56 € | 5,56 € |
| ESP32-S3-WROOM-1-N16R8 | 1 | 4,82 € | 4,82 € |
| 5-V-Regler R-78K5.0-2.0 | 1 | 4,43 € | 4,43 € |
| Gleichtaktdrossel RN102-1-02-3M0 | 1 | 1,40 € | 1,40 € |

Davon stehen 33,08 € — Relais, Klemmen und NFC-Frontend — unter „Nicht
verhandelbar", also 40 % der Materialkosten. Die LED-Leiste über dem
Display und die Statusliste machen zusammen 5,06 € aus, die Statusliste
allein 3,50 € mit ihren beiden fest verdrahteten Punkten.

**Ersparnis:** umgesetzt 1,55 €, offen **5,30 €** (Positionen 2, 3, 5 und
6). Das sind 6 % der 83 €. Die eine Position, die das Bild ändern würde,
ist das Display — und die hängt an den Anforderungen, nicht am Preis.

## Übersicht

| # | Position | heute | Alternative | Ersparnis | Status |
|---|---|---:|---|---:|---|
| 1 | Display | ~10 € | kleineres OLED, oder ganz weglassen | **~10 €** | Anforderungen A2, A5, B5 |
| 2 | Statusliste | 3,23 € | Zustände auf dem Display | 3,23 € | offen |
| 3 | 5-V-Regler U306 | 4,43 € | R-78E5.0-1.0, 1 A statt 2 A | 0,95 € | Strombudget prüfen |
| 4 | LED-Leiste | 3,72 € | 7 statt 12 LEDs | 1,55 € | **entschieden** |
| 5 | Isolator U300 | 5,56 € | ISOW7721**F**DFMR | 0,60 € | offen |
| 6 | ESP32-Modul U100 | 4,82 € | N16 ohne PSRAM | 0,52 € | Firmware prüfen |

Gesichert sind 1,55 €, sie stecken schon in den 83 € oben. Positionen 1
und 2 zusammen wären rund 13 € je Gerät — 400 € über die Serie — kosten aber
die lokale Rückmeldung.

**Die beiden größten Posten hängen zusammen.** Display und Statusliste sind
zwei Wege, denselben Zustand zu zeigen. Einer von beiden kann weg, nicht
beide: sonst bleibt als lokale Anzeige nur die siebenstellige LED-Leiste,
und die kann keinen Klarnamen.

## 1 — Display, rund 10 €

Das ER-TFT020-3 ist nicht bepreist — es kommt von EastRising, nicht von
Mouser oder Digi-Key, und steht deshalb in keiner Datenbank. Der
Größenordnung nach ist es nach dem Lastrelais der teuerste Posten, dazu
kommen der ZIF-Steckverbinder mit 1,57 €, das FPC, der gedruckte Träger und
die Plexiglasscheibe.

**Preis beschaffen und hier eintragen** — ohne Zahl lässt sich die Position
nicht bewerten.

Drei Stufen sind denkbar:

- **Zurück zum kleinen OLED.** Anforderung A2 nennt ohnehin ein OLED; das
  2-Zoll-TFT kam, weil das DT010ATFT zu klein war. Ein 1,3-Zoll-OLED über
  SPI kostet wenige Euro und zeigt einen Klarnamen in zwei Zeilen.
- **Nur auf der Werkstattversion.** Geräte an Maschinen ohne Publikum
  brauchen den Namen weniger dringend als eine FKS im Kursbetrieb.
- **Ganz weglassen.** Dann fallen A2, A5 und B5 — und damit die sichtbare
  Verantwortlichkeit, die der Kern des Geräts ist. Das ist eine
  Anforderungsentscheidung, keine Kostenentscheidung.

## 2 — Statusliste, 3,23 €

Neun RGB-LEDs à 0,26 € und der Treiber IS31FL3236A mit 0,89 € — zusammen
3,23 €. Die beiden fest verdrahteten Versorgungspunkte bleiben in jedem
Fall, sie kosten 0,26 € und sind hier nicht mitgerechnet.

Dagegen steht: **das Display kann dieselben Zustände zeigen.** Eine
Serviceseite mit 24 V, Insel, NFC, WLAN, Relais und Aux kostet kein
Bauteil. Was es nicht kann, ist der Fall „Firmware steht" — und genau den
decken die zwei fest verdrahteten LEDs ab, die bleiben.

Dafür spricht die Liste: ablesbar, ohne das Gehäuse zu öffnen, und ohne sich
durch eine Menüseite zu klicken, an einem Gerät, das am Netz hängt.

Entscheidet sich das Display gegen die Liste, spart die Liste. Entscheidet
sich die Liste gegen das Display, spart das Display mehr.

## 3 — 5-V-Regler, 0,95 €

Der R-78K5.0-2.0 liefert 2 A für 4,43 €, der R-78E5.0-1.0 liefert 1 A für
3,48 € bei gleicher Bauform und gleichem Pinout.

Zu prüfen ist das Strombudget der 5-V-Schiene: sieben LEDs der Leiste,
Tongeber, Display-Beleuchtung und der 3,3-V-Regler des MCU-Boards mit
630 mA Spitze. Das summiert sich auf knapp unter 1 A — zu knapp für ein
1-A-Modul ohne genaue Messung.

**Zum Vorschlag, 5 V und 3,3 V in einem Baustein zu erzeugen:** ein PMIC,
der beides aus 24 V kann, ist ein Automotive-Baustein; bei Mouser und
Digi-Key ist derzeit keiner gelistet, der passt. Mehrfach-Ausgangsmodule für
24 V Eingang gibt es praktisch nur als ±-Typen für Analogtechnik. Der Weg
wäre also ein diskreter Abwärtswandler plus LDO — rund 1,70 € an Bauteilen
statt 4,60 €, dafür eine zweite Störquelle im Steuerzweig, dessen Filter auf
150 kHz ausgelegt ist, und Layoutarbeit. Erst sinnvoll, wenn das Layout
steht.

Die beiden 3,3-V-Regler selbst sind keine Position: ein AP2114H kostet
0,17 €. Sie sitzen getrennt, weil der DC-DC des Isolators seinen Strom
sonst über 30 cm Kabel zöge.

## 4 — LED-Leiste, 1,55 € (entschieden)

Sieben adressierbare LEDs statt zwölf. Anforderung A1 nennt noch zwölf und
einen Ring, beides ist überholt.

## 5 — Isolator, 0,60 €

Der ISOW7721**F**DFMR kostet 0,60 € weniger — und ist der einzige der
beiden Typen, der überhaupt lagernd ist. Die Entscheidung hängt nicht am
Preis, sondern am Default-Pegel bei abgeschalteter Insel; sie steht als TODO
im [README](README.md).

## 6 — ESP32-Modul, 0,52 €

Das `-N16R8` bringt 8 MB PSRAM mit, die ohne Framebuffer niemand braucht;
das `-N16` kostet 4,30 € statt 4,82 €. Ein voller Framebuffer für 240 × 320
bei 16 bit sind 150 kB und passen knapp in den internen RAM, mit partiellem
Bildaufbau reicht weniger. Entscheidung der Firmware, das Footprint ist
identisch. Fällt das Display weg, fällt die Frage mit.

## Nicht verhandelbar

- **Lastrelais K300, 20,15 €.** Der teuerste Posten des Geräts, und das
  Ergebnis einer langen Suche: zweipolig für N und L, 30 A
  Einschaltvermögen, verstärkte Isolierung Spule/Kontakt mit 8 mm. Die
  Anforderungen dazu stehen fest. Billigere zweipolige Relais gibt es
  reichlich — sie erfüllen die Barriere nicht.
- **Feldklemmen J300/J301, 2 × 3,34 €.** Die Hebelklemme wird nicht gegen
  eine Schraubklemme getauscht. Sie ist ohne Werkzeug bedienbar, und am
  geöffneten Gerät berührsicher.
- **NFC-Frontend U200, 6,25 €.** Der MFRC63002HN wäre 1,70 € billiger. Der
  CLRC663 plus steht im Longevity-Programm und bringt die LPCD-Optionen für
  das zyklische Kartenlesen mit. Für 1,70 € gibt man die Laufzeit des
  Designs auf.

## Nicht bewertet

- **Gehäuse und Mechanik** — 3D-Druck, Plexiglas, Abdeckplatten. Liegt in
  Fusion, nicht in diesem Repo.
- **Leiterplatte und Bestückung** — die 70 µm Kupfer des 230-V-Boards und
  die vier Lagen des RFID-Boards sind Kostenträger, die hier fehlen.
