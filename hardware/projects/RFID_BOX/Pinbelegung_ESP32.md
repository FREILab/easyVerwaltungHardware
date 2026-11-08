# Pinbelegung ESP32-S3 (IC100) — vorläufig

Stand 2026-11-08, abgeglichen mit `ECAD/RFID_Board/*.kicad_sch` und J302 des I/O-Boards.
Netznamen aus dem Schaltplan; noch nicht vergebene Namen sind *kursiv*.

## Belegung

Modulpin = Pin am ESP32-S3-WROOM-1. Seite: L = links (Pin 1–14), U = unten (15–26),
R = rechts (27–41), Antenne oben.

| GPIO | Modulpin | Seite | Netz | Richtung | Funktion |
|---|---|---|---|---|---|
| 0 | 27 | R | — | in | Strapping, R101 gegen GND nicht bestückt (Recovery) |
| 1 | 39 | R | *EXT_IO* | i/o | Reserve Extension Board, ADC1_CH0 |
| 2 | 38 | R | RGB_LED_Data | out | LED-Leiste → IC101 Gatter 1 (1A, Pin 2), RMT |
| 3 | 15 | U | — | — | frei, Strapping (JTAG-Quelle) |
| 4 | 4 | L | *V24_SENSE* | analog | 24-V-Teilersignal vom I/O-Board, ADC1_CH3 |
| 5 | 5 | L | ISO_EN | out | Versorgung Messinsel |
| 6 | 6 | L | relay_en_LS | out | K300 Low-Side (Q300) |
| 7 | 7 | L | *ESTOP_EN* | out | E-Stop Extension Board (K3) |
| 8 | 12 | L | relay_en_HS | out | K300 High-Side (Q301) |
| 9 | 17 | U | LCD_RS | out | Display Daten/Befehl |
| 10 | 18 | U | LCD_CS | out | Display Chip Select |
| 11 | 19 | U | SPI_MOSI | out | SPI2 IOMUX (FSPID), Display + NFC |
| 12 | 20 | U | SPI_SCK | out | SPI2 IOMUX (FSPICLK), Display + NFC |
| 13 | 21 | U | SPI_MISO | in | SPI2 IOMUX (FSPIQ), nur NFC |
| 14 | 22 | U | SPI_CS_NFC | out | CLRC663 NSS, Pull-up R200 |
| 15 | 8 | L | — | — | Y100 über R102 — belegt |
| 16 | 9 | L | — | — | Y100 über R103 — belegt |
| 17 | 10 | L | UART_MCU2MCP | out | UART1 TX → U300 → MCP39F511A |
| 18 | 11 | L | UART_MCP2MCU | in | UART1 RX ← U300 |
| 19 | 13 | L | ESP_USB_D- | — | J103 über R107 — belegt |
| 20 | 14 | L | ESP_USB_D+ | — | J103 über R108 — belegt |
| 21 | 23 | U | NFC_IRQ | in | CLRC663 IRQ |
| 35–37 | 28–30 | R | — | — | Octal-PSRAM (N16R8) — nicht nutzbar |
| 38 | 31 | R | *STOP* | in | Stopp-Taster gegen GND |
| 39 | 32 | R | *I2C_SCL* | out | IS31FL3236A (IC104) |
| 40 | 33 | R | *I2C_SDA* | i/o | IS31FL3236A (IC104) |
| 41 | 34 | R | LCD_BL_PWM | out | Gate Q100, LEDC-PWM |
| 42 | 35 | R | Buzzer_HS_en | out | Piezo → IC101 Gatter 2 (2A, Pin 5) |
| 43 | 37 | R | TXD0 | out | J100, Konsole/Recovery — belegt |
| 44 | 36 | R | RXD0 | in | J100 — belegt |
| 45 | 26 | U | Buzzer_LS_en | out | Piezo → IC101 Gatter 3 (3A, Pin 9), gegenphasig |
| 46 | 16 | U | — | — | frei, Strapping |
| 47 | 24 | U | NFC_PDOWN | out | CLRC663 Power-Down, Pull-up R201 |
| 48 | 25 | U | LCD_RST# | out | Display Reset |

## Regeln der Zuordnung

- Relaisfreigaben (GPIO 6, 8) liegen auf Pins ohne internen Pull-up und ohne Aktivität
  beim Booten, nicht nebeneinander am Modul. Der Ruhepegel kommt von den Pulldowns auf dem
  I/O-Board.
- Analoge Signale nur auf ADC1 (GPIO 1–10); ADC2 ist bei aktivem WLAN nicht nutzbar.
- Display und NFC teilen SPI2 auf den IOMUX-Pins (Display bis 80 MHz, CLRC663 bis
  10 MHz, getrennte CS).
- GPIO 45 muss beim Booten LOW sein (VDD_SPI 3,3 V). Der interne Pulldown hält
  Buzzer_LS_en dort, der Piezo bleibt still.
- UART1 für die Messinsel; UART0 bleibt Konsole an J100.
- JTAG nur über USB-Serial-JTAG an J103; GPIO 39–42 sind normale GPIOs.
- IS31FL3236A ohne SDB-Leitung: SDB per Pull-up auf 3V3, Abschalten per Register.

## Offen

- **Display-Bus falsch benannt:** Pin 8 und 9 des Displays (M?) hängen an `I2C_SDA` /
  `I2C_SCK`. Das Display ist 4-Wire SPI; die Pins heißen im Controller SDA/SCL, sind aber
  MOSI und Takt. Auf `SPI_MOSI` / `SPI_SCK` legen. Die Namen `I2C_*` bleiben für IC104.
- **Q100** braucht einen Gate-Pulldown (100 kΩ), sonst floatet die Beleuchtung bis zur
  Initialisierung.
- **Display-Pegel:** Das Display läuft auf 2,8 V (IC103), der ESP32 treibt 3,3 V. Gegen
  das Datenblatt des ER-TFT020-3 prüfen; sonst Display-I/O auf 3V3.
- **Kabel J104/J302:** Pinreihenfolge noch nicht festgelegt. Belegt: 5 V, GND, ISO_EN,
  UART ×2, relay_en_HS/LS, V24_SENSE, ESTOP_EN, EXT_IO. Ob die zweite Messleitung neben
  V24_SENSE gebraucht wird, ist offen; GPIO 1 kann analog lesen.
- **Isolator F-Typ:** Wird U300 als ISOW7721FDFMR bestückt, sieht UART1-RX bei
  `ISO_EN` aus dauerhaft LOW — Empfänger in der Firmware abschalten.
