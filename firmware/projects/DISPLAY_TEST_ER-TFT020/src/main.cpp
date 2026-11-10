// Testbild RFID_BOX auf ER-TFT020-3 (ST7789V2, 240x320) am NUCLEO-L433RC-P
// Querformat: Name, Relaisstatus mit Laufbalken, Dauer seit Aktivierung, Leistung.
// Dauer und Leistung sind simuliert.

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

constexpr uint8_t PIN_CS  = D10;  // PA11
constexpr uint8_t PIN_DC  = D9;   // PA8, am Display "RS"
constexpr uint8_t PIN_RST = D8;   // PB6

constexpr uint32_t SPI_HZ   = 8000000;  // für fliegende Verdrahtung niedrig
constexpr uint8_t  ROTATION = 1;        // Querformat; 3 = um 180° gedreht

const char *USER_NAME = "Max Mustermann";

Adafruit_ST7789 tft(&SPI, PIN_CS, PIN_DC, PIN_RST);

// Farben
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
const uint16_t C_BG        = rgb(12, 16, 26);
const uint16_t C_HEADER    = rgb(32, 64, 120);
const uint16_t C_TEXT      = ST77XX_WHITE;
const uint16_t C_MUTED     = rgb(140, 150, 165);
const uint16_t C_GREEN     = rgb(46, 204, 96);
const uint16_t C_ON_GREEN  = rgb(8, 40, 20);
const uint16_t C_TRACK     = rgb(30, 150, 70);
const uint16_t C_RUNNER    = rgb(220, 255, 230);

// Layout (320 x 240)
constexpr int16_t W = 320;
constexpr int16_t HEADER_H = 40;

constexpr int16_t STATUS_X = 10, STATUS_Y = 48, STATUS_W = 300, STATUS_H = 88;

constexpr int16_t BAR_X = STATUS_X + 16, BAR_Y = STATUS_Y + STATUS_H - 16;
constexpr int16_t BAR_W = STATUS_W - 32, BAR_H = 5;
constexpr int16_t RUN_W = 56;     // Länge des laufenden Segments
constexpr int16_t RUN_STEP = 4;   // Pixel pro Schritt
constexpr uint32_t RUN_MS = 25;   // Schrittzeit

constexpr int16_t COL_L = 16, COL_R = 168;
constexpr int16_t LABEL_BASE = 168;
constexpr int16_t VAL_Y = 178, VAL_W = 140, VAL_H = 40;

GFXcanvas16 valCanvas(VAL_W, VAL_H);  // flimmerfreie Wertefelder

// Breite nach Schrittweite (xAdvance), nicht nach Pixelgrenzen: hält Ziffern ortsfest
static int16_t advanceWidth(const GFXfont *f, const char *s) {
  int16_t w = 0;
  for (; *s; s++) {
    if (*s >= f->first && *s <= f->last) w += f->glyph[*s - f->first].xAdvance;
  }
  return w;
}

static void drawStatic() {
  tft.fillScreen(C_BG);

  // Kopfzeile mit Name
  tft.fillRect(0, 0, W, HEADER_H, C_HEADER);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(C_TEXT);
  tft.setCursor(14, 28);
  tft.print(USER_NAME);

  // Statusblock
  tft.fillRoundRect(STATUS_X, STATUS_Y, STATUS_W, STATUS_H, 12, C_GREEN);
  const char *status = "Relais aktiv";
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextColor(C_ON_GREEN);
  tft.setCursor(STATUS_X + (STATUS_W - advanceWidth(&FreeSansBold24pt7b, status)) / 2,
                STATUS_Y + 50);
  tft.print(status);
  tft.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, C_TRACK);

  // Beschriftungen
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(C_MUTED);
  tft.setCursor(COL_L, LABEL_BASE);
  tft.print("Dauer");
  tft.setCursor(COL_R, LABEL_BASE);
  tft.print("Leistung");
}

static void drawValue(int16_t x, const char *text) {
  valCanvas.fillScreen(C_BG);
  valCanvas.setFont(&FreeSansBold18pt7b);
  valCanvas.setTextColor(C_TEXT);
  valCanvas.setCursor(0, 32);
  valCanvas.print(text);
  tft.drawRGBBitmap(x, VAL_Y, valCanvas.getBuffer(), VAL_W, VAL_H);
}

// Laufbalken: Segment wandert durch die Spur und tritt rechts aus, links wieder ein.
// Pro Schritt nur Kopf und Schwanz neu zeichnen.
static void stepRunner() {
  static int16_t pos = -RUN_W;  // linke Kante des Segments relativ zu BAR_X
  auto span = [](int16_t from, int16_t len, uint16_t c) {
    int16_t a = max<int16_t>(from, 0), b = min<int16_t>(from + len, BAR_W);
    if (b > a) tft.fillRect(BAR_X + a, BAR_Y, b - a, BAR_H, c);
  };
  span(pos, RUN_STEP, C_TRACK);              // Schwanz löschen
  span(pos + RUN_W, RUN_STEP, C_RUNNER);     // Kopf zeichnen
  pos += RUN_STEP;
  if (pos >= BAR_W) pos = -RUN_W;
}

static void formatDuration(char *buf, size_t len, uint32_t s) {
  snprintf(buf, len, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
}

static void formatPower(char *buf, size_t len, uint32_t watt) {
  if (watt < 1000) {
    snprintf(buf, len, "%lu W", watt);
  } else {
    const uint32_t tenth = (watt + 50) / 100;  // auf 0,1 kW gerundet
    snprintf(buf, len, "%lu,%lu kW", tenth / 10, tenth % 10);
  }
}

uint32_t activeSince;
int32_t powerW = 1300;

void setup() {
  Serial.begin(115200);
  tft.init(240, 320);
  tft.setSPISpeed(SPI_HZ);
  tft.setRotation(ROTATION);
  drawStatic();
  activeSince = millis();
  randomSeed(analogRead(A0));
}

void loop() {
  static uint32_t lastRun = 0;
  if (millis() - lastRun >= RUN_MS) {
    lastRun += RUN_MS;
    stepRunner();
  }

  static uint32_t lastS = UINT32_MAX;
  const uint32_t s = (millis() - activeSince) / 1000;
  if (s == lastS) return;
  lastS = s;

  // Simulierte Last: Zufallsbewegung um 1,3 kW
  powerW = constrain(powerW + random(-60, 61), 1150, 1450);

  char buf[16];
  formatDuration(buf, sizeof buf, s);
  drawValue(COL_L, buf);
  formatPower(buf, sizeof buf, powerW);
  drawValue(COL_R, buf);
}
