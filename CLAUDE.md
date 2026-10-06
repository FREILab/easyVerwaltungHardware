# easyVerwaltungHardware

## KiCad: eigene Bibliothek und Preisdaten liegen in MyCSLib

Die Boards unter `hardware/projects/` benutzen die persönliche Bauteilbibliothek aus dem
Nachbar-Repo `/Users/marius/Github/MyCSLib`. Dort liegen nicht nur Symbole und Footprints,
sondern auch fertige Werkzeuge und eine Preisdatenbank:

| Was | Wo |
|---|---|
| Symbolbibliothek | `MyCSLib/KiCad/aa_MyParts.kicad_sym` |
| Preise, Bestand, MOQ, Status je Bauteil | `MyCSLib/KiCad/parts_db.json` (+ `.csv`) |
| Werkzeuge und deren Doku | `MyCSLib/KiCad/tools/`, `README.md` |

### Kosten schätzen

**Für Materialkosten `MyCSLib/KiCad/parts_db.json` lesen — nicht selbst bei Mouser oder
Digi-Key anfragen.** Die Datei enthält je MPN die vollständige Preisstaffel beider
Distributoren plus `best` mit dem günstigeren Anbieter je Stückzahl.

Regeln, die dabei gelten:

- Stückpreis bei Menge N = Preis der höchsten Staffelstufe mit `quantity <= N`.
- `min_order` beachten — eine Rolle mit MOQ 4000 ist für fünf Prototypen keine Option.
- Mengen **je MPN über alle BOM-Zeilen summieren**, bevor die Staffelstufe bestimmt wird.
  Dieselbe MPN steht oft in mehreren Zeilen (z.B. Ferrite als `FB300-FB302` und
  `FB303-FB307`), und die Summe landet in einer günstigeren Stufe.
- Rund die Hälfte der BOM-Zeilen (Kondensatoren, Widerstände, Stecker) hat keine
  Bestelldaten. Diese Positionen **separat ausweisen**, nicht stillschweigend mit 0 werten.
- `n/A` in einer Bestellnummer heißt „wird dort nicht geführt" — dann gewinnt der andere
  Distributor automatisch.
- `generated` in der Datenbank nennt das Alter der Preise. Beim Rechnen dazusagen.

Sind die Preise veraltet: in MyCSLib `KiCad/tools/Preise_aktualisieren.command` ausführen
(braucht die API-Schlüssel in `MyCSLib/KiCad/tools/scripts/DISTRIBUTOR_SECRETS.h`).

### Bauteilfelder in Schaltplänen

Symbole aus `aa_MyParts` tragen `Manufacturer`, `Manufacturer Part Number`,
`Distributor 1`/`Distributor 2` samt Bestellnummern. Diese Felder sind die Verbindung zur
Preisdatenbank — beim Kopieren von Symbolen prüfen, dass sie zum neuen Bauteil passen und
nicht vom Original stehen bleiben.
