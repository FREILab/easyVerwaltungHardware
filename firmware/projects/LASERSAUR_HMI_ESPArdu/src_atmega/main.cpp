#include <Arduino.h>
#include <EEPROM.h>
#include "ArduinoJson-v5.13.0.h"
#include "myFunctions.h"

uint16_t DisplayErrorScreen = 0;
uint32_t tempTicks = 0;

bool WaterFlowStopped = false;

uint16_t oldAbluftState = LOW;
float oldTemp[4] = {1000.0, 1000.0, 1000.0, 1000.0};

float offsetPres[4] = {0.0, 0.0, 0.0, 0.0};
bool firstStart = true;

unsigned int logCounter = 0;

// Flag wird im ISR gesetzt, Senden passiert im loop()
volatile bool postPending = false;

// Puffer für eingehende ESP-Nachrichten
static char espBuf[16];
static uint8_t espPos = 0;


// ─────────────────────────────────────────────────────────
// JSON-Payload via Serial3 an ESP senden
// ─────────────────────────────────────────────────────────

void sendLogToESP() {
    StaticJsonBuffer<300> jb;
    JsonObject& root = jb.createObject();
    root["WaterBT"]   = MeasurementData.WaterTempBT;
    root["WaterAT"]   = MeasurementData.WaterTempAT;
    root["PresF1"]    = MeasurementData.PresFilter1;
    root["PresF2"]    = MeasurementData.PresFilter2;
    root["PresF3"]    = MeasurementData.PresFilter3;
    root["WaterFlow"] = MeasurementData.WaterFlow;
    root["T1"]        = MeasurementData.Temp1;
    root["T2"]        = MeasurementData.Temp2;
    root["T3"]        = MeasurementData.Temp3;
    root["T4"]        = MeasurementData.Temp4;

    Serial3.print("POST:");
    root.printTo(Serial3);
    Serial3.println();
}

// ─────────────────────────────────────────────────────────
// Eingehende ESP-Nachrichten auswerten (nicht-blockierend)
// ─────────────────────────────────────────────────────────

void processSerial3() {
    while (Serial3.available()) {
        char c = Serial3.read();
        if (c == '\n') {
            espBuf[espPos] = '\0';
            espPos = 0;
            if (strcmp(espBuf, "PING") == 0) {
                Serial3.println("PONG");
            } else if (strncmp(espBuf, "ACK:", 4) == 0) {
                Serial.print("[ESP] ACK: ");
                Serial.println(espBuf + 4);
            } else if (strncmp(espBuf, "NACK:", 5) == 0) {
                Serial.print("[ESP] NACK: ");
                Serial.println(espBuf + 5);
            }
        } else if (c != '\r' && espPos < sizeof(espBuf) - 1) {
            espBuf[espPos++] = c;
        }
    }
}


// ─────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);   // USB-Debug via CH340 (Serial0)
    Serial3.begin(9600);    // ESP8266-Kommunikation via RXD3/TXD3

    MeasurementData.Temp1 = 0.0;
    MeasurementData.Temp2 = 0.0;
    MeasurementData.Temp3 = 0.0;
    MeasurementData.Temp4 = 0.0;

    configIO();

    Wire.setClock(1000);

    Mux_LCD_init();

    ScanI2CForDevices();    // falls Drucksensoren fehlen, stoppt das System hier

    // Timer0 für ~1kHz Ticker (Display-Rotation + Log-Intervall)
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);

    // Timer1: Input Capture für Wasserfluss-Messung
    TCCR1A = 0x00;
    TCCR1B = 0x05;          // 16MHz / 1024 → TCNT1 inkrementiert alle 64 µs
    Ticks = 0;
    TIMSK1 = _BV(ICIE1) | _BV(TOIE1);

    Mux_LCD_clear();

    MeasurementData.Temp1 = 0.0;
    MeasurementData.Temp2 = 0.0;
    MeasurementData.Temp3 = 0.0;
    MeasurementData.Temp4 = 0.0;
    MeasurementData.Data1Filter.pressure = 0.0;
    MeasurementData.Data2Filter.pressure = 0.0;
    MeasurementData.Data3Filter.pressure = 0.0;
    MeasurementData.Data4Filter.pressure = 0.0;

    Serial.println(EEPROM.read(0x10), DEC);

//  if(EEPROM.read(0x10) == 0xF0)
//      Errors |= ERR_LASERADJUSTMENT;

    Serial.println("[ATmega] Bereit");
}


// ─────────────────────────────────────────────────────────
// Timer0 ISR (~1kHz)
// ─────────────────────────────────────────────────────────

ISR(TIMER0_COMPA_vect)
{
    static int TempCounter = 0;

    if(DisplayCounter < (LCD_TICKER_S * 1000))
        DisplayCounter++;

    // Alle 60 Sekunden: Flag setzen, Senden passiert im loop()
    if(logCounter++ >= 60000)
    {
        postPending = true;
        logCounter = 0;
    }

    if(digitalRead(PIN_CHI_ABLUFT) == HIGH)
        digitalWrite(PIN_RLY_KOMP, HIGH);
    else
        digitalWrite(PIN_RLY_KOMP, LOW);

    if(TempCounter++ >= 1000)
    {
        if(MeasurementData.Temp1 - oldTemp[0] > MAX_TEMPDIFF) tempDiffTooHigh = true;
        if(MeasurementData.Temp2 - oldTemp[1] > MAX_TEMPDIFF) tempDiffTooHigh = true;
        if(MeasurementData.Temp3 - oldTemp[2] > MAX_TEMPDIFF) tempDiffTooHigh = true;
        if(MeasurementData.Temp4 - oldTemp[3] > MAX_TEMPDIFF) tempDiffTooHigh = true;

        oldTemp[0] = MeasurementData.Temp1;
        oldTemp[1] = MeasurementData.Temp2;
        oldTemp[2] = MeasurementData.Temp3;
        oldTemp[3] = MeasurementData.Temp4;

        TempCounter = 0;
    }
}

// ─────────────────────────────────────────────────────────
// Timer1 ISR — Wasserfluss Input Capture
// ─────────────────────────────────────────────────────────

ISR(TIMER1_CAPT_vect) {
    if(bit_is_set(TCCR1B, ICES1)) {    // steigende Flanke
        TCNT1 = 0;
        WaterFlowStopped = false;
    } else {                            // fallende Flanke
        Ticks = ICR1;
    }
    TCCR1B ^= _BV(ICES1);
}

ISR(TIMER1_OVF_vect) {
    WaterFlowStopped = true;
}


// ─────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────

void loop() {

//  if(digitalRead(PIN_EEPROM_RESET) == LOW)
//  {
//      EEPROM.write(0x10, 0x20);
//      Serial.println(EEPROM.read(0x10),DEC);
//      Errors &= ~ERR_LASERADJUSTMENT;
//  }

    MeasurementData.WaterTempBT = TempLookup(analogRead(ADC_WT_BT));
    MeasurementData.WaterTempAT = TempLookup(analogRead(ADC_WT_AT));
    MeasurementData.Data1Filter = ReadPressureAndTemperature(PRESSURESENSOR1);
    MeasurementData.Data2Filter = ReadPressureAndTemperature(PRESSURESENSOR2);
    MeasurementData.Data3Filter = ReadPressureAndTemperature(PRESSURESENSOR3);
    MeasurementData.Data4Filter = ReadPressureAndTemperature(PRESSURESENSOR4);

    if(firstStart)
    {
        offsetPres[0] = MeasurementData.Data2Filter.pressure - MeasurementData.Data1Filter.pressure;
        offsetPres[1] = MeasurementData.Data3Filter.pressure - MeasurementData.Data2Filter.pressure;
        offsetPres[2] = MeasurementData.Data4Filter.pressure - MeasurementData.Data3Filter.pressure;
        firstStart = false;
    }

    MeasurementData.PresFilter1 = (MeasurementData.Data2Filter.pressure - MeasurementData.Data1Filter.pressure - offsetPres[0]) * 1000;
    MeasurementData.PresFilter2 = (MeasurementData.Data3Filter.pressure - MeasurementData.Data2Filter.pressure - offsetPres[1]) * 1000;
    MeasurementData.PresFilter3 = (MeasurementData.Data4Filter.pressure - MeasurementData.Data3Filter.pressure - offsetPres[2]) * 1000;

    MeasurementData.Temp1 = ReadTemperature(MCP9808_CH1);
    MeasurementData.Temp2 = ReadTemperature(MCP9808_CH2);
    MeasurementData.Temp3 = ReadTemperature(MCP9808_CH3);
    MeasurementData.Temp4 = ReadTemperature(MCP9808_CH4);

    tempTicks = getTick();
    if(tempTicks != 0 && !WaterFlowStopped)
        MeasurementData.WaterFlow = FlowLookup(10000000L / (2 * tempTicks * 64));
    else
        MeasurementData.WaterFlow = 0;

    ErrorHandling();

    // ISR hat postPending gesetzt → jetzt senden (außerhalb ISR-Kontext)
    if(postPending)
    {
        sendLogToESP();
        postPending = false;
    }

    // Eingehende ESP-Antworten verarbeiten (PING → PONG, ACK/NACK loggen)
    processSerial3();

    if(DisplayCounter >= (LCD_TICKER_S * 1000))
    {
        if(Errors == 0)
        {
            if(DisplayScreen < LCD_SCREENS - 1)
                DisplayScreen++;
            else
                DisplayScreen = 0;
        }
        else
        {
            for(int i = 0; i < MAX_ERRORS; i++)
            {
                DisplayScreen = ERR_BASE + (Errors & (1 << DisplayErrorScreen));
                DisplayErrorScreen++;
                if(DisplayErrorScreen == MAX_ERRORS) DisplayErrorScreen = 0;
                if(DisplayScreen != ERR_BASE) break;
            }
        }

        DisplayCounter = 0;
        Mux_LCD_clear();
    }

    LCD_Display();
}
