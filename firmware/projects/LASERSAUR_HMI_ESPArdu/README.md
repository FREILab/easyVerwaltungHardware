# LASERSAUR_HMI_ESPArdu

Lasersaur HMI-Firmware für **Mega2560 WiFi R3** (ATmega2560 + ESP8266 auf einer Platine).

Architektur: **ATmega2560 = Gehirn** (Sensoren, Display, Relais, Fehlerlogik), **ESP8266 = WiFi-Modem** (HTTP POST, Telnet-Debug, OTA).

Portierung von `LASERSAUR_HMI_legacy` (Mega2560 + Ethernet-Shield).

---

## Hardware

- **Board:** Mega2560 WiFi R3 (ATmega2560 + ESP8266, USB-TTL CH340)
- **Schalter 2** muss einmalig physisch auf **RXD3/TXD3** gestellt werden  
  → verbindet ESP8266 mit ATmega Serial3 (Pins 14/15) statt Serial0

## DIP-Schalter

| Modus | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|-------|---|---|---|---|---|---|---|
| **Normalbetrieb** | EIN | EIN | EIN | EIN | AUS | AUS | AUS |
| ATmega flashen | AUS | AUS | EIN | EIN | AUS | AUS | AUS |
| ESP flashen | AUS | AUS | AUS | AUS | EIN | EIN | EIN |

Im Normalbetrieb: Serial0 → USB-Debug, Serial3 → ESP — kein Konflikt.

---

## Einrichtung

**1. Secrets anlegen (nicht in Git):**

```bash
cp include/secret.h.example include/secret.h
cp platformio.secrets.ini.example platformio.secrets.ini
```

`include/secret.h` → WiFi-SSID und Passwort eintragen  
`platformio.secrets.ini` → Server-Host und Auth-Token eintragen

**2. ATmega flashen** (DIP 3+4 ON):
```bash
pio run -e laser_hmi_atmega_usb --target upload
```

**3. ESP flashen** (DIP 5+6+7 ON, einmalig):
```bash
pio run -e laser_hmi_esp_usb --target upload
```

**4. DIP auf Normalbetrieb** (1+2+3+4 ON)

**ESP danach per OTA updaten** (kein DIP-Wechsel nötig):
```bash
pio run -e laser_hmi_esp_ota --target upload
```

---

## Debugging

- **ATmega:** Serial Monitor 115200 Baud (`pio device monitor -e laser_hmi_atmega_usb`)
- **ESP:** Telnet auf `laser-hmi.local` Port 23 (`pio device monitor -e laser_hmi_esp_ota`)

---

## Protokoll ATmega ↔ ESP (Serial3, 9600 Baud)

| Richtung | Nachricht | Bedeutung |
|----------|-----------|-----------|
| ATmega → ESP | `POST:{json}\n` | HTTP POST ausführen |
| ATmega → ESP | `PONG\n` | Heartbeat-Antwort |
| ESP → ATmega | `ACK:200\n` | POST erfolgreich |
| ESP → ATmega | `NACK:WIFI\n` | Kein WiFi |
| ESP → ATmega | `NACK:503\n` | Server nicht erreichbar |
| ESP → ATmega | `NACK:TIMEOUT\n` | Keine Antwort vom Server |
| ESP → ATmega | `PING\n` | Heartbeat (alle 5 s) |

---

## Pin-Mapping ATmega2560

| Pin | Signal | Richtung |
|-----|--------|----------|
| 2 | Kompressor-Relais | Output |
| 3 | Treiber-Relais | Output |
| 4 | Kompressor-Taster | Input |
| 5 | Abluft-Sensor | Input |
| 6 | Switch Enable | Output |
| 7 | Wasserpfad Enable | Output |
| 8 | Wasserfluss RPM (ICP) | Input |
| 9 | RFID Enable | Input |
| 13 | Status-LED (OnBoard) | Output |
| 14/15 | Serial3 → ESP8266 | UART |
| 20/21 | I2C (SDA/SCL) | I2C |
| 62 (A8) | EEPROM Reset | Input |
| A1 | Wassertemperatur After | ADC |
| A2 | Wassertemperatur Before | ADC |
| A3 | Linsentemperatur | ADC |

**I2C-Mux TCA9548A (0x70):**

| Kanal | Gerät |
|-------|-------|
| 0 | MPL3115A2 Drucksensor 1 |
| 1 | MPL3115A2 Drucksensor 2 |
| 2 | MCP9808 Temp CH2 |
| 3 | MCP9808 Temp CH1 |
| 4 | MPL3115A2 Drucksensor 3 |
| 5 | MPL3115A2 Drucksensor 4 |
| 6 | MCP9808 Temp CH3 + CH4 |
| 7 | LCD 20×4 (0x27) |
