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
| Display ER-TFT020-3 | 3,92 € |
| **Summe** | **86,92 €** |
| **30 Geräte** | **2.608 €** |

Das Display kostet **4,26 USD** beim Hersteller, hier mit 0,92 €/USD
gerechnet. Es kommt nicht über Mouser oder Digi-Key, steht deshalb in
keiner Preisdatenbank, und Versand sowie Einfuhr kommen oben drauf — die
Distributorpreise daneben sind Nettopreise ab Lager.

Nicht enthalten: Leiterplatten, Gehäuse, Bestückung.

Die acht teuersten Einzelposten tragen 57,51 € davon, also zwei Drittel:

| Teil | Menge | Stück | gesamt |
|---|---:|---:|---:|
| Lastrelais T92S11D12-24 | 1 | 20,15 € | 20,15 € |
| Feldklemmen WAGO 2604-1103 | 2 | 3,34 € | 6,68 € |
| NFC-Frontend CLRC66303HNY | 1 | 6,25 € | 6,25 € |
| AC/DC-Modul IRM-10-24 | 1 | 5,70 € | 5,70 € |
| Isolator ISOW7721DFMR | 1 | 5,56 € | 5,56 € |
| ESP32-S3-WROOM-1-N16R8 | 1 | 4,82 € | 4,82 € |
| 5-V-Regler R-78K5.0-2.0 | 1 | 4,43 € | 4,43 € |
| Display ER-TFT020-3 | 1 | 3,92 € | 3,92 € |

Davon stehen 33,08 € — Relais, Klemmen und NFC-Frontend — unter „Nicht
verhandelbar", also 38 % der Materialkosten. Die LED-Leiste über dem
Display und die Statusliste machen zusammen 5,06 € aus, die Statusliste
allein 3,50 € mit ihren beiden fest verdrahteten Punkten.

**Ersparnis:** umgesetzt 1,55 €, offen **5,30 €**. Das sind 6 % der
86,92 €. Mehr gibt die Stückliste nicht her, solange Relais, Klemmen und
NFC-Frontend stehen — und das Display mit 3,92 € ist zu billig, um die
Anforderungen dafür aufzugeben.

## Übersicht

| # | Position | heute | Alternative | Ersparnis | Status |
|---|---|---:|---|---:|---|
| 1 | Statusliste | 3,23 € | Zustände auf dem Display | **3,23 €** | offen |
| 2 | LED-Leiste | 3,72 € | 7 statt 12 LEDs | 1,55 € | **entschieden** |
| 3 | 5-V-Regler U306 | 4,43 € | R-78E5.0-1.0, 1 A statt 2 A | 0,95 € | Strombudget prüfen |
| 4 | Isolator U300 | 5,56 € | ISOW7721**F**DFMR | 0,60 € | offen |
| 5 | ESP32-Modul U100 | 4,82 € | N16 ohne PSRAM | 0,52 € | Firmware prüfen |

Gesichert sind 1,55 €, sie stecken schon in den 86,92 € oben. Mehr als
5,30 € sind aus der Stückliste nicht zu holen.

**Seit der Displaypreis bekannt ist, ist die Statusliste die schwächere der
beiden Anzeigen.** Beide zeigen denselben Zustand, beide kosten etwa
gleich viel — aber das Display ist durch A2, A5 und B5 ohnehin gesetzt und
kann zusätzlich den Klarnamen. Die Liste wäre der Teil, der geht.

## 1 — Statusliste, 3,23 €

Neun RGB-LEDs à 0,26 € und der Treiber IS31FL3236A mit 0,89 € — zusammen
3,23 €. Die beiden fest verdrahteten Versorgungspunkte bleiben in jedem
Fall, sie kosten 0,26 € und sind hier nicht mitgerechnet.

Dagegen steht: **das Display kann dieselben Zustände zeigen.** Eine
Serviceseite mit 24 V, Insel, NFC, WLAN, Relais und Aux kostet kein
Bauteil. Was es nicht kann, ist der Fall „Firmware steht" — und genau den
decken die zwei fest verdrahteten LEDs ab, die bleiben.

Dafür spricht die Liste: ablesbar, ohne das Gehäuse zu öffnen, und ohne sich
durch eine Menüseite zu klicken, an einem Gerät, das am Netz hängt.

Seit der Displaypreis bekannt ist, ist die Frage einseitig: das Display
bleibt ohnehin, die Liste ist die zusätzliche Anzeige. Wer sie streicht,
verliert die Ablesbarkeit ohne Menü und spart 3,23 € — die größte einzelne
Position, die überhaupt noch zur Wahl steht.

## 2 — LED-Leiste, 1,55 € (entschieden)

Sieben adressierbare LEDs statt zwölf. Anforderung A1 nennt noch zwölf und
einen Ring, beides ist überholt.

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

## 4 — Isolator, 0,60 €

Der ISOW7721**F**DFMR kostet 0,60 € weniger — und ist der einzige der
beiden Typen, der überhaupt lagernd ist. Die Entscheidung hängt nicht am
Preis, sondern am Default-Pegel bei abgeschalteter Insel; sie steht als TODO
im [README](README.md).

## 5 — ESP32-Modul, 0,52 €

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
- **Display ER-TFT020-3, 3,92 €.** Mit dem Herstellerpreis ist die Frage
  entschieden: für knapp vier Euro gibt es keine Anzeige, die A2, A5 und B5
  gleichwertig erfüllt. Ein kleineres OLED spart davon höchstens zwei Euro
  und kann den Klarnamen schlechter. Bleibt.
- **NFC-Frontend U200, 6,25 €.** Der MFRC63002HN wäre 1,70 € billiger. Der
  CLRC663 plus steht im Longevity-Programm und bringt die LPCD-Optionen für
  das zyklische Kartenlesen mit. Für 1,70 € gibt man die Laufzeit des
  Designs auf.

## Nicht bewertet

- **Gehäuse und Mechanik** — 3D-Druck, Plexiglas, Abdeckplatten. Liegt in
  Fusion, nicht in diesem Repo.
- **Leiterplatte und Bestückung** — die 70 µm Kupfer des 230-V-Boards und
  die vier Lagen des RFID-Boards sind Kostenträger, die hier fehlen.
