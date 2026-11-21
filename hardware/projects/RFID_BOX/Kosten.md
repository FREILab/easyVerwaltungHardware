# Kosten je Gerät

Stand 2026-11-21. Ein Gerät = RFID-Board V1.0 (`rfid-board/v1.0`) + IO-Board V1.0
(`io-board/v1.0`) + Display. Nicht enthalten: Gehäuse, FFC-Kabel, Bestückung, die
Positionen ohne Preisdaten (unten).

## Gesamt mit Steuern, Zoll und Versand

Leiterplatten beider Boards je 5, 10 bzw. 20 Stück inklusive Schablonen. Für 1, 2 und 5
Geräte sind es dieselben 5 Leiterplatten je Board, nur die Bauteilmenge ändert sich. Für den
ersten Prototyp (1 Gerät) ist das Display schon vorhanden.

| | 1 Gerät | 2 Geräte | 5 Geräte | 10 Geräte | 20 Geräte |
|---|---:|---:|---:|---:|---:|
| Bauteile netto | 99,03 € | 190,64 € | 435,38 € | 787,63 € | 1.527,46 € |
| USt. 19 % auf Bauteile | 18,82 € | 36,22 € | 82,72 € | 149,65 € | 290,22 € |
| Versand Digi-Key (Bestellung < 50 €) | 21,42 € | 21,42 € | – | – | – |
| Leiterplatten + Schablonen, beide Boards, inkl. Steuern und Versand | 127,30 € | 127,30 € | 127,30 € | 139,00 € | 207,00 € |
| Display + Versand | vorhanden | 27,84 € | 39,60 € | 59,20 € | 98,40 € |
| Einfuhrumsatzsteuer + Zollpauschale Display | – | 8,29 € | 10,52 € | 14,25 € | 21,70 € |
| **Gesamt** | **266,57 €** | **411,71 €** | **695,53 €** | **1.149,73 €** | **2.144,77 €** |
| **je Gerät** | **266,57 €** | **205,86 €** | **139,11 €** | **114,97 €** | **107,24 €** |
| davon Steuern und Zoll auf Bauteile und Display | 18,82 € | 44,51 € | 93,25 € | 163,90 € | 311,91 € |

### Annahmen zu Steuern und Zoll

- Der Verein zieht keine Vorsteuer ab, die 19 % sind also Kosten.
- **Mouser, Digi-Key:** Lieferung DDP, Zoll im Preis, Umsatzsteuer an der Kasse. Bei Digi-Key
  gilt DDP nur mit UPS/FedEx, mit DHL fallen die Abgaben bei Zustellung an. Die WAGO-Klemmen
  2604-1103 führt nur Digi-Key; unter 50 € Bestellwert kostet der Versand dort 18 € netto
  (Forenangabe, nicht offiziell bestätigt).
- **Leiterplatten:** Angebotspreise vom 2026-11-21 für beide Boards zusammen, inklusive
  Schablonen, Versand und Steuern. Keine weiteren Abgaben bei der Einfuhr.
- **Display aus China:** 4,26 USD ≈ 3,92 € je Stück ([Kostenoptimierung.md](Kostenoptimierung.md)),
  Versand mit 20 € geschätzt. Einfuhrumsatzsteuer 19 % und seit 1. Juli 2026 eine Zollpauschale
  von 3 € je Tarifposition für Sendungen bis 150 € (laut DHL).

## Bauteile pro Board

Quelle: `MyCSLib/KiCad/parts_db.json`, Preisstand 2026-11-18. Menge je MPN über beide Boards
der Bestellung summiert, günstigerer Distributor (Mouser/Digi-Key), Mindestbestellmenge beachtet.

| Board | 5 Stück | 10 Stück | 20 Stück |
|---|---:|---:|---:|
| RFID-Board | 23,42 € | 20,51 € | 19,59 € |
| IO-Board | 63,65 € | 58,25 € | 56,78 € |

Größte Posten bei 10 Stück:

| RFID-Board | € | IO-Board | € |
|---|---:|---|---:|
| CLRC66303HNY | 7,11 | T92S11D12-24 (K300) | 20,15 |
| ESP32-S3-WROOM-1-N16R8 | 5,20 | 2 × WAGO 2604-1103 | 7,61 |
| 8 × IN-PI20TATPRPGPB | 2,49 | ISOW7721DFMR | 6,29 |
| PKLCS1212E40A1-R1 | 0,86 | IRM-10-24 | 5,83 |
| USB4105-GF-A | 0,58 | R-78K5.0-2.0 | 4,83 |
| | | MCP39F511AT-E/MQ | 3,09 |

Ohne Preisdaten (nicht in den Summen; das Display ist oben getrennt angesetzt):

- RFID-Board: Y200 XRCGB27M120F3M10R0,
  C100/C102 GRM21BR61A226ME51K, C200/C201 885012006052, C400 885342208024,
  R402/R403 CRCW06035K10FKEA, R404 CRCW12061M00FKEA, R501 CRCW080543R0FKEA
- IO-Board: R315 CRCW06035K10FKEA

Nicht lagernd bei Mouser und Digi-Key: ISOW7721DFMR (U301, IO-Board),
GRM31CR71E106KA12L (C210, RFID-Board).

## Vergleich: Leiterplatte + Schablone + Versand pro Board (PCBShopper)

Nicht in der Gesamtrechnung. Quelle: PCBShopper (pcbshopper.com), abgefragt 2026-11-21, Versand nach Deutschland,
Lieferung ≤ 14 Werktage. Gemeinsam: 1,6 mm, grün, Bestückungsdruck beidseitig, günstigste
bleifreie Oberfläche, Leiterbahn/Abstand 0,2 mm, kleinste Bohrung 0,3 mm.

**IO-Board** – 94 × 98,5 mm, 2 Lagen, 2 oz, Schablone einseitig

| Hersteller | 5 Stück | 10 Stück | 30 Stück |
|---|---:|---:|---:|
| NextPCB | 20,34 € | 10,78 € | 4,99 € |
| PCBgogo | 15,54 € | 9,22 € | 5,16 € |
| Elecrow | 20,19 € | 10,11 € | 4,88 € |
| AISLER (DE) | 24,58 € (6 Stück) | 13,24 € (12 Stück) | 6,08 € |
| PCBCART | 27,22 € | 14,62 € | 6,56 € |
| Eurocircuits | 33,47 € | 23,19 € | 12,48 € |
| AIVON | 4,92 €* | 3,01 €* | 2,04 € |

\* Neukundenangebot (0,91 € für die Platinen)

**RFID-Board** – 77,5 × 134 mm, 4 Lagen, 1 oz, Schablone beidseitig

| Hersteller | 5 Stück | 10 Stück | 30 Stück |
|---|---:|---:|---:|
| NextPCB | 15,96 € | 8,82 € | 4,29 € |
| AIVON | 11,12 € | 7,20 € | 4,07 € |
| Smart Prototyping | 20,46 € | 10,88 € | 4,83 € |
| Elecrow | 21,68 € | 12,59 € | 5,67 € |
| AISLER (DE) | 16,89 € (6 Stück) | 15,22 € (12 Stück) | 14,22 € |
| PCBgogo | 29,35 € | 16,05 € | 7,16 € |
| PCBCART | 30,92 € | 16,88 € | 8,19 € |
| Eurocircuits | 58,70 € | 41,07 € | 20,93 € |

## Einschränkungen

- JLCPCB und PCBWay haben auf PCBShopper kein Angebot für diese Kombination geliefert.
- PCBShopper trägt das Copyright 2022; Preise vor der Bestellung beim Hersteller prüfen.
- Impedanzkontrolle und Lagenaufbau JLC04161H-3313 (RFID-Board) sind bei PCBShopper nicht
  wählbar und können extra kosten.
