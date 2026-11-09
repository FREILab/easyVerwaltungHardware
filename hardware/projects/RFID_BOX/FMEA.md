# Machine Node / RFID_BOX — FMEA

Betrachtet werden Einzelfehler, die nach aussen wirken — Brandgefahr,
Netzspannung auf berührbaren Teilen oder ein Schaltausgang, der sich nicht
mehr abschalten lässt. Reiner Funktionsverlust ohne Gefährdung ist nicht
aufgeführt.

Neue Schaltungsteile werden gegen diese Tabelle geprüft, bevor sie ins
Layout gehen — so steht es in der Sicherheitsleitlinie der
[README](README.md).

| Bauteil | Fehlerart | Auswirkung ohne Maßnahme | Maßnahme |
|---|---|---|---|
| C305/C308 (Y2) | Kurzschluss | Netzspannung auf PE → FI löst aus, Gerät und Maschine sind freigeschaltet | Y2-Klasse: für Netz gegen PE zugelassen (IEC 60384-14) und bauartbedingt offen versagend. Keine berührbaren Teile betroffen, da PE geerdet ist |
| F300 (Lastpfad) | Kurzschluss hinter der Sicherung | Prospektiver Strom mehrere hundert Ampere; über dem Ausschaltvermögen bleibt der Lichtbogen stehen → Brandgefahr | Bel 5HT 10-R, Keramik, **1500 A** bei 250 VAC (IEC 60127-2 Blatt 5). Vorher 100 A |
| F302 (Steuerzweig) | wie oben, MRT trennt nur bis 35 A | wie oben | **R303 begrenzt auf 25 A.** Sicherungswiderstand mit definierter Abschaltcharakteristik, flammgeschützt, versagt offen |
| C300 (X2) | Entladekette offen | bis 325 V auf den Steckerstiften | 100 nF an der Ausnahmegrenze, Ladung 5,3 mJ. R300/R301/R302 als zweite Ebene: unter 60 V in 0,17 s |
| Q300 (High-Side) | Linearbetrieb bei eingebrochener 24 V | Verlustleistung über P_tot 350 mW → Überhitzung | Gate-Teiler stellt V_GS = −V_CC/2, an der Schwelle nur 6 mW. U300 im Hiccup-Modus |
| U301 (ISOW7721) | Isolationsdurchschlag | Netzpotential auf der SELV-Seite | verstärkte Isolierung (VDE 0884-17, 5 kVrms nach UL 1577) **und** 8 mm Barriere auf der Leiterplatte — zwei unabhängige Ebenen |
| Relaistreiber | ein Schalter durchlegiert | Relais dauerhaft angezogen, Firmware kann nicht mehr abschalten | **Zwei Schalter in Reihe in der Spule:** High-Side-PMOS (Q300) und Low-Side-NMOS (Q301), an getrennten GPIOs. Ein durchlegierter Schalter lässt sich über den anderen abschalten. Auf J302 trennt GND die beiden Enables, eine Lötbrücke hebt die Verriegelung nicht auf |
| Relaistreiber | beide Schalter durchlegiert | wie oben | Doppelfehler, nicht abgedeckt — Erkennung über die Lastmessung |
| K300 | Kontakt verschweisst | Schaltausgang bleibt aktiv | Lastmessung als Plausibilitätsprüfung: Strom bei kommandiertem AUS ist ein Fehler. Abschalten nicht möglich, siehe unten |
| Gate-Pulldown/Pull-up | offen | Gate floatet bei Reset/Boot, Relaiszustand undefiniert | Pulldown R316 an Q301 und R322 an Q302, Pull-up R320 an Q300, alle nahe am Gate; Prüfpunkt im Abnahmetest |

Die Erkennung über die Lastmessung funktioniert, weil R307 **vor** dem
Relais sitzt und in Reihe dazu liegt: bei offenem Relais fliesst kein Strom.
Ein gemessener Strom im Zustand AUS ist damit eindeutig ein Fehler.

Beim Treiber kann die Firmware darauf reagieren — sie öffnet den jeweils
anderen Schalter, und das Relais fällt ab. Beim verschweissten Kontakt kann
sie nur melden: K300 lässt sich dann nur extern freischalten. Ein zweites
Lastrelais in Reihe wäre die Lösung, ist aber auf 100x100 mm nicht
unterzubringen und steht zurück — wie der fehlende Hardware-Bypass am
E-Stop-Relais (BR3).
