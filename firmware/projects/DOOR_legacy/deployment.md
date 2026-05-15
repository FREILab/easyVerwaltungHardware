# Deployment (UNO R3 WiFi Clone: ATmega328P + ESP8266)

Diese Datei beschreibt die DIP-Switch-Konfiguration und eine robuste Flash-Prozedur
fuer Boards vom Typ UNO R3 WiFi (ATmega328P + ESP8266 + CH340G, 8-DIP-Block).

## Wichtiger Hinweis

Das aktuelle Projekt ist in `platformio.ini` auf `uno_r4_wifi` (Renesas) konfiguriert.
Fuer einen echten UNO-R3-WiFi-Clone brauchst du eine AVR-kompatible Build-Konfiguration
oder einen passenden Legacy-Branch fuer ATmega328P.

## Build-Ziele (wie bei RFID-Boxen)

Es gibt ein klares Paar aus USB und OTA:

- USB Build/Upload: `door_usb`
- OTA Build/Upload: `door_ota`

Kommandos:

```bash
pio run -e door_usb --target upload
pio run -e door_ota --target upload

# spezifisch je Tuer
pio run -e door_01_ota --target upload
pio run -e door_02_ota --target upload
```

Geraete-spezifische Varianten bleiben verfuegbar (`door_01_*`, `door_02_*`).

## DIP-Switch Matrix (1..7)

| Modus | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| ATmega328P <-> ESP8266 | ON | ON | OFF | OFF | OFF | OFF | OFF |
| USB <-> ATmega328P (Sketch upload) | OFF | OFF | ON | ON | OFF | OFF | OFF |
| USB <-> ESP8266 (Firmware upload) | OFF | OFF | OFF | OFF | ON | ON | ON |
| USB <-> ESP8266 (Kommunikation) | OFF | OFF | OFF | OFF | ON | ON | OFF |
| Alles unabhaengig | OFF | OFF | OFF | OFF | OFF | OFF | OFF |

## Flash-Prozedur (ATmega + ESP)

1. USB-Treiber pruefen
- CH340G Treiber muss installiert sein.
- Serielles Geraet muss im System sichtbar sein.

2. ATmega328P Sketch flashen
- DIP auf `USB <-> ATmega328P` setzen: `3=ON, 4=ON`, Rest `OFF`.
- Flash ueber IDE/PlatformIO fuer AVR-Board durchfuehren.
- Kurz testen (Serieller Monitor, Grundfunktion).

3. ATmega <-> ESP koppeln
- DIP auf `ATmega328P <-> ESP8266`: `1=ON, 2=ON`, Rest `OFF`.
- Endtest: Auth-Flow, WLAN-Verbindung und Stabilitaet.

## OTA-Betrieb (Empfehlung)

1. Erst immer USB-Erstflash validieren.
2. Danach OTA nur mit stabiler Stromversorgung nutzen.
3. `upload_port` fuer OTA ist standardmaessig auf DNS-Hostnamen gesetzt
	(`tuer-01.intern`, `tuer-02.intern`).
4. Keine DHCP-Reservation erforderlich: Ziel ist OTA nur ueber Netznamen.
5. Falls DNS temporaer ausfaellt, Upload einmalig mit direkter IP starten:

```bash
pio run -e door_01_ota --target upload --upload-port 10.30.0.106
pio run -e door_02_ota --target upload --upload-port 10.30.0.107
```

## DNS-Strategie fuer 2 Tueren

1. Pro Tuer einen stabilen DNS-Namen vergeben (z. B. `tuer-01.intern`, `tuer-02.intern`).
2. Keine statischen Reservations oder Server-seitigen Sonderregeln voraussetzen.
3. In PlatformIO standardmaessig die DNS-Hosts verwenden; IP nur als Notfall-Override.

## Troubleshooting (Kurz)

- Upload auf ATmega funktioniert nicht: DIP 3/4 pruefen, anderes USB-Kabel testen.
- Keine Kommunikation AVR<->ESP: DIP 1/2 pruefen, Baudrate und UART-Verdrahtung vergleichen.
