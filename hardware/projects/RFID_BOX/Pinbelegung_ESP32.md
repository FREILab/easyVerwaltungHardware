# Pinbelegung ESP32-S3 (IC100)

Stand 2026-11-09, abgeglichen mit `ECAD/RFID_Board/*.kicad_sch` und J302 des I/O-Boards.

## Belegung

Modulpin = Pin am ESP32-S3-WROOM-1. Seite: L = links (Pin 1–14), U = unten (15–26),
R = rechts (27–41), Antenne oben. Stand: ✓ = verdrahtet, ○ = offen.

| Modulpin | Seite | GPIO | Netz | Richtung | Funktion | Stand |
|---|---|---|---|---|---|---|
| 1 | L | — | GND | — | | ✓ |
| 2 | L | — | +3V3 | — | | ✓ |
| 3 | L | — | EN | in | Reset, R100/C101, SW100 | ✓ |
| 4 | L | 4 | V24_SENSE | analog | J102.6, ADC1_CH3, C105 100 nF | ✓ |
| 5 | L | 5 | ISO_EN | out | J102.5, Versorgung Messinsel | ✓ |
| 6 | L | 6 | relay_en_LS | out | J102.11, K300 Low-Side (Q301) | ✓ |
| 7 | L | 7 | aux_en | out | J102.10, Aux / E-Stop-Relais | ✓ |
| 8 | L | 15 | — | — | frei, nur digital (ADC2) | |
| 9 | L | 16 | EXT_IO | i/o | J102.12, Reserve Extension Board | ✓ |
| 10 | L | 17 | UART_MCU2MCP | out | J102.7, UART1 TX → U301 | ✓ |
| 11 | L | 18 | UART_MCP2MCU | in | J102.8, UART1 RX ← U301 | ✓ |
| 12 | L | 8 | relay_en_HS | out | J102.2, K300 High-Side (Q302 → Q300) | ✓ |
| 13 | L | 19 | ESP_USB_D- | — | J400 über R400 | ✓ |
| 14 | L | 20 | ESP_USB_D+ | — | J400 über R401 | ✓ |
| 15 | U | 3 | — | — | frei, Strapping (JTAG-Quelle) | |
| 16 | U | 46 | — | — | frei, Strapping | |
| 17 | U | 9 | LCD_RS | out | M500.7 | ✓ |
| 18 | U | 10 | SPI_CS_LCD | out | M500.12 | ✓ |
| 19 | U | 11 | SPI_MOSI | out | SPI2 IOMUX, M500.8 + U200.28 | ✓ |
| 20 | U | 12 | SPI_SCK | out | SPI2 IOMUX, M500.9 + U200.29 | ✓ |
| 21 | U | 13 | SPI_MISO | in | SPI2 IOMUX, U200.30 | ✓ |
| 22 | U | 14 | SPI_CS_NFC | out | U200.31, Pull-up R200 | ✓ |
| 23 | U | 21 | NFC_IRQ | in | U200.32 | ✓ |
| 24 | U | 47 | NFC_PDOWN | out | U200.21, Pull-up R201 | ✓ |
| 25 | U | 48 | LCD_RST# | out | M500.6 | ✓ |
| 26 | U | 45 | Buzzer_HS_en | out | IC101.5 (Gatter 2) | ✓ |
| 27 | R | 0 | — | in | Strapping, R101 gegen GND nicht bestückt | ✓ |
| 28–30 | R | 35–37 | — | — | Octal-PSRAM (N16R8) — nicht nutzbar | |
| 31 | R | 38 | STOP | in | R103 1 kΩ → STOP_BTN (J101.1, Pull-up R102, C104 100 nF) | ✓ |
| 32 | R | 39 | — | — | frei, reserviert für I²C SCL (Statusliste) | |
| 33 | R | 40 | — | — | frei, reserviert für I²C SDA (Statusliste) | |
| 34 | R | 41 | LCD_BL_PWM | out | Gate Q500, Pulldown R500; R501 43 Ω an 5 V → ≈ 46 mA | ✓ |
| 35 | R | 42 | Buzzer_LS_en | out | IC101.9 (Gatter 3), gegenphasig | ✓ |
| 36 | R | 44 | UART_PC2MCU | in | J100.3, UART0 | ✓ |
| 37 | R | 43 | UART_MCU2PC | out | J100.2, UART0 | ✓ |
| 38 | R | 2 | RGB_LED_Data | out | IC101.2 (Gatter 1), RMT | ✓ |
| 39 | R | 1 | V3V3_IO_SENSE | analog | J102.9, ADC1_CH0, C107 100 nF | ✓ |
| 40–41 | R | — | GND | — | inkl. Thermal-Pad | ✓ |

## Kabelstecker J102 (RFID-Board)

FFC 1,0 mm, 14-polig (SFW14S-2STE1LF). J302 auf dem I/O-Board ist gespiegelt belegt:
J302-Pin = 15 − J102-Pin.

| J102 | Netz | J302 |
|---|---|---|
| 1, 14 | GND | 14, 1 |
| 2 | relay_en_HS | 13 |
| 3, 4 | +5V (RFID-Board: über D100 auf +5V) | 12, 11 |
| 5 | ISO_EN | 10 |
| 6 | V24_SENSE | 9 |
| 7 | UART_MCU2MCP | 8 |
| 8 | UART_MCP2MCU | 7 |
| 9 | V3V3_IO_SENSE | 6 |
| 10 | aux_en | 5 |
| 11 | relay_en_LS | 4 |
| 12 | EXT_IO | 3 |
| 13 | — (Reserve) | 2 |

Teiler auf dem I/O-Board: V24_SENSE R314 47 kΩ / R315 5,1 kΩ (24 V → 2,35 V),
V3V3_IO_SENSE R318 / R319 je 10 kΩ (3,3 V → 1,65 V).

## Regeln der Zuordnung

- Relaisfreigaben (GPIO 6, 8) liegen auf Pins ohne internen Pull-up und ohne Aktivität
  beim Booten, nicht nebeneinander am Modul. Der Ruhepegel kommt von den Pulldowns auf dem
  I/O-Board.
- Analoge Signale nur auf ADC1 (GPIO 1–10); ADC2 ist bei aktivem WLAN nicht nutzbar.
- Display und NFC teilen SPI2 auf den IOMUX-Pins (Display bis 80 MHz, CLRC663 bis
  10 MHz, getrennte CS).
- GPIO 45 muss beim Booten LOW sein (VDD_SPI 3,3 V). Der interne Pulldown hält
  Buzzer_HS_en dort, der Piezo bleibt still.
- UART1 für die Messinsel; UART0 bleibt Konsole an J100.
- JTAG nur über USB-Serial-JTAG an J400; GPIO 39–42 sind normale GPIOs.

## Offen

- **Isolator F-Typ:** Wird U301 als ISOW7721FDFMR bestückt, sieht UART1-RX bei
  `ISO_EN` aus dauerhaft LOW — Empfänger in der Firmware abschalten.
