# LASERSAUR HMI — Legacy

Arduino-Firmware für das HMI-Board des Lasersaur-Laserplotters. Liest Sensoren aus, zeigt Werte auf einem LCD an und meldet Messdaten per HTTP an das Backend.

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | Arduino Mega (Ethernet-Shield) |
| Netzwerk | Ethernet via EthernetClient |
| Temperatursensoren | MCP9808 (I2C, 4×) |
| Drucksensoren | MPL3115A2 (I2C) |
| Display | LCD via I2C-Mux (LiquidCrystal_I2C) |
| Kommunikation | HTTP POST mit JSON (ArduinoJson v5) |

## Funktion

Erfasst Temperatur- und Druckwerte aus dem Lasersaur-System (Wasser, Abluft, Kompressor) und überträgt diese periodisch an das Backend. Zeigt Systemstatus auf einem LCD an und stoppt bei Sensor-Ausfall.

## Abgrenzung

Dieses Projekt nutzt die **alte HTTP-POST-API** mit statischer IP-Konfiguration. Die Weiterentwicklung findet in `LASERSAUR_HMI_easyVerwaltung` statt.

## Abhängigkeiten

- `ArduinoJson v5` (lokal eingebunden)
- `Ethernet2`
- `LiquidCrystal_I2C`
