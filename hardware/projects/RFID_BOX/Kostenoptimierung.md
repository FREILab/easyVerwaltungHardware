# Kostenoptimierung

Laufendes Kapitel. Hier steht, wo das Gerät teuer ist und was eine
Alternative kosten würde — sortiert nach Ersparnis je Gerät, die größte
zuerst.

Preise aus `MyCSLib/KiCad/parts_db.json`, Stand **2026-11-04**, jeweils der
günstigere Distributor bei **25 Stück**. Die Stückzahl der ersten Serie ist
30 Geräte.

> **Sicherheit geht vor Kosten.** Keine der Positionen hier hebt eine
> Festlegung aus [Anforderungen.md](Anforderungen.md) oder dem Regelwerk im
> [README](README.md) auf. Wo eine Ersparnis an eine Bedingung geknüpft ist,
> steht sie dabei — ungeprüft gilt sie nicht.

## Übersicht

| # | Position | heute | Alternative | Ersparnis | Status |
|---|---|---:|---|---:|---|
| 1 | Lastrelais K300 | 20,15 € | TE/Schrack RT424024 | **17,37 €** | zu prüfen |
| 2 | Feldklemmen J300/J301 | 6,68 € | Schraubklemme | 4,70 € | verworfen |
| 3 | 5-V-Regler U306 | 4,43 € | diskreter Abwärtswandler | 3,20 € | zu prüfen |
| 4 | NFC-Frontend U200 | 6,25 € | MFRC63002HN | 1,70 € | verworfen |
| 5 | LED-Leiste | 3,72 € | 7 statt 12 LEDs | 1,55 € | **entschieden** |
| 6 | Isolator U300 | 5,56 € | ISOW7721**F**DFMR | 0,60 € | offen |
| 7 | ESP32-Modul U100 | 4,82 € | N16 ohne PSRAM | 0,52 € | zu prüfen |

Gesichert sind davon bisher 1,55 €. Die Positionen 1 und 3 zusammen wären
20,57 € je Gerät, über 30 Geräte 617 €.

## 1 — Lastrelais, 17,37 €

Das T92S11D12-24 ist mit Abstand der teuerste Posten des Geräts: 20,15 €,
mehr als das Dreifache des nächstteuren Bauteils. Das TE/Schrack RT424024
kostet 2,78 € bei 26.847 Stück Lagerbestand und ist zweipolig mit 8 A.

Vor einem Wechsel zu prüfen, in dieser Reihenfolge:

- **8 mm Luft- und Kriechstrecke Spule/Kontakt.** Das ist die bindende
  Festlegung, nicht der Normwert von 5 mm. Das Datenblatt muss sie belegen,
  nicht nur eine Prüfspannung in kV nennen.
- **Einschaltvermögen.** Das T92 wurde wegen 30 A und AgCdO-Kontakten
  gewählt; die Nachlaufprofile nennen Maschinen mit Motoranlauf. Trägt der
  RT das, oder braucht es dann doch eine Einschaltstrombegrenzung — die das
  README heute bewusst nicht vorsieht?
- **Bauhöhe und Fläche** im Schacht, der Deckelabstand liegt bei 35 mm.

Ergibt die Prüfung, dass nur für Motorlasten ein größeres Relais nötig ist,
wäre eine zweite Bestückvariante denkbar: RT für ohmsche Lasten, T92 für
Maschinen mit Anlauf.

## 2 — Feldklemmen, 4,70 € (verworfen)

Zwei WAGO 2604-1103 kosten 6,68 €, eine Schraubklemme läge bei einem Euro.
Die Anforderungen verlangen aber eine **berührsichere Bauform** an allen
Stellen, die beim geöffneten Gerät freiliegen, und die Hebelklemme erfüllt
das ohne Werkzeug am Leiter. Bleibt.

## 3 — 5-V-Regler, 3,20 €

Der R-78K5.0-2.0 ist ein fertiges Schaltreglermodul für 4,43 €. Ein
diskreter Abwärtswandler mit Spule und Diode liegt bei rund 1,20 €.

Dagegen steht: das Modul bringt Unterspannungs- und Kurzschlussschutz mit,
braucht keine EMV-Arbeit und keine Fläche für die Spule. Der Steuerzweig
hängt an einem Filter, das auf 150 kHz ausgelegt ist — ein eigener Wandler
schiebt eine zweite Störquelle dazwischen. Erst prüfen, wenn das Layout
steht und Platz übrig ist.

## 4 — NFC-Frontend, 1,70 € (verworfen)

Der MFRC63002HN kostet 4,55 € gegen 6,25 € für den CLRC66303HNY. Er ist
derselben Familie, aber der CLRC663 **plus** wurde bewusst gewählt: aktiv im
Longevity-Programm und mit den LPCD-Optionen, die das zyklische Kartenlesen
im Always-on-Modus billig machen. Für 1,70 € gibt man die Laufzeit des
Designs auf. Bleibt.

## 5 — LED-Leiste, 1,55 € (entschieden)

Die Leiste über dem Display kommt mit **sieben** adressierbaren LEDs statt
zwölf aus. Anforderung A1 nennt noch zwölf und einen Ring; beides ist
überholt.

## 6 — Isolator, 0,60 €

Der ISOW7721**F**DFMR kostet 0,60 € weniger als der heute eingesetzte D-Typ
— und ist der einzige der beiden, der überhaupt lagernd ist. Die Entscheidung
hängt nicht am Preis, sondern am Default-Pegel; sie steht als TODO im
[README](README.md).

## 7 — ESP32-Modul, 0,52 €

Das `-N16R8` bringt 8 MB PSRAM mit, die ohne Framebuffer niemand braucht.
Das `-N16` ohne PSRAM kostet 4,30 € statt 4,82 €. Zu prüfen: ein voller
Framebuffer für 240 × 320 bei 16 bit sind 150 kB und passen damit knapp in
den internen RAM — mit partiellem Bildaufbau reicht weniger. Die Entscheidung
gehört in die Firmware, nicht ins Layout; das Footprint ist identisch.

## Nicht bewertet

- **Display ER-TFT020-3** — noch kein Bibliotheksteil, kein Preis in der
  Datenbank. Dem Bauteilwert nach gehört es in die Spitzengruppe.
- **Gehäuse und Mechanik** — 3D-Druck, Plexiglas, Abdeckplatten. Liegt in
  Fusion, nicht in diesem Repo.
- **Bestückung und Leiterplatte** selbst. Die 70 µm Kupfer des 230-V-Boards
  und die vier Lagen des RFID-Boards sind Kostenträger, die hier noch
  fehlen.
