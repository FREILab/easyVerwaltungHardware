#include "myFunctions.h"
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 20 chars and 4 line display
struct MeasData MeasurementData;
uint16_t DisplayCounter = 0xFFFF, DisplayScreen = 0xFF;
uint16_t Errors = 0;
volatile unsigned int Ticks = 0;
bool tempDiffTooHigh = false;




/////////////////////////////////////////////////////////////////////////////////////////////////////
//  configIO()
//  Configure the IOs of the Arduino
//  Input: none
/////////////////////////////////////////////////////////////////////////////////////////////////////

void configIO() {
  pinMode(PIN_LED,OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_RLY_KOMP,OUTPUT);
  digitalWrite(PIN_RLY_KOMP, LOW);

  pinMode(PIN_RLY_DRVBRD,OUTPUT);
  digitalWrite(PIN_RLY_DRVBRD, LOW);

  pinMode(PIN_KOMP,INPUT);
  digitalWrite(PIN_KOMP,HIGH);
  pinMode(PIN_CHI_ABLUFT,INPUT);

  pinMode(PIN_SW_EN,OUTPUT);
  digitalWrite(PIN_SW_EN, LOW);

  pinMode(PIN_WPTH_EN,OUTPUT);
  digitalWrite(PIN_WPTH_EN, LOW);

  pinMode(PIN_WF_RPM,INPUT);
  pinMode(PIN_RFID_EN,INPUT);

  pinMode(PIN_EEPROM_RESET,INPUT);
  digitalWrite(PIN_EEPROM_RESET,HIGH);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  tcaselect(uint8_t i)
//  Changes the I2C multiplexer to the chosen channel
//  Input: uint8_t channel -> I2C channel
/////////////////////////////////////////////////////////////////////////////////////////////////////

void tcaselect(uint8_t channel) {
  if (channel > 7) return;

  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  ScanI2CForDevices(void)
//  Scans all channels of the multiplexer for I2C devices and checks if pressure sensor are present
//  Input: none
/////////////////////////////////////////////////////////////////////////////////////////////////////
void ScanI2CForDevices(void)
{
  uint8_t devices[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  bool sensorAvailable[4] = {false, false, false, false};
  bool allSensorsAvailable = false;

  Wire.begin();
  #ifdef _DEBUG
  Serial.begin(115200);
  Serial.println("\nTCAScanner ready!");
  #endif

  for (uint8_t t=0; t<8; t++) {
    tcaselect(t);
    #ifdef _DEBUG
    Serial.print("TCA Port #"); Serial.println(t);
    #endif

    for (uint8_t addr = 0; addr<=127; addr++) {
      if (addr == TCAADDR) continue;

      uint8_t data;
      if (! twi_writeTo(addr, &data, 0, 1, 1)) {
         #ifdef _DEBUG
         Serial.print("Found I2C 0x");  Serial.println(addr,HEX);
         #endif
         devices[t] = addr;
      }
    }
  }

  tcaselect(MUX_LCD);

  lcd.clear();

  lcd.setCursor(0,0);
  if(devices[PRESSURESENSOR1] == 0x60)
  {
    lcd.write(byte(0));
    sensorAvailable[0] = true;
  }
  else
  {
    lcd.write(byte(1));
    sensorAvailable[0] = false;
  }
  lcd.print(" Drucksensor 1");

  lcd.setCursor(0,1);
  if(devices[PRESSURESENSOR2] == 0x60)
  {
    lcd.write(byte(0));
    sensorAvailable[1] = true;
  }
  else
  {
    lcd.write(byte(1));
    sensorAvailable[1] = false;
  }
  lcd.print(" Drucksensor 2");

  lcd.setCursor(0,2);
  if(devices[PRESSURESENSOR3] == 0x60)
  {
    lcd.write(byte(0));
    sensorAvailable[2] = true;
  }
  else
  {
    lcd.write(byte(1));
    sensorAvailable[2] = false;
  }
  lcd.print(" Drucksensor 3");

  lcd.setCursor(0,3);
  if(devices[PRESSURESENSOR4] == 0x60)
  {
    lcd.write(byte(0));
    sensorAvailable[3] = true;
  }
  else
  {
    lcd.write(byte(1));
    sensorAvailable[3] = false;
  }
  lcd.print(" Drucksensor 4");

  delay(2000);

  allSensorsAvailable = sensorAvailable[0] & sensorAvailable[1] & sensorAvailable[2] & sensorAvailable[3];

  #ifdef _DEBUG
  allSensorsAvailable = true;
  #endif

  if(!allSensorsAvailable)
  {
    lcd.clear();
    lcd.setCursor(6,0);
    lcd.print("FEHLER");
    lcd.setCursor(0,1);
    lcd.print("Min. ein Drucksensor");
    lcd.setCursor(2,2);
    lcd.print("nicht gefunden!");
    lcd.setCursor(2,3);
    lcd.print("System gestoppt!");

    digitalWrite(PIN_LED, HIGH);

    stopSystem();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  ReadPressureAndTemperature(uint8_t channel)
//  Reads the pressure in mBar and temperature in °C from the chosen I2C mux channel from the MPL3115A2
//  Input: uint8_t i -> I2C channel
/////////////////////////////////////////////////////////////////////////////////////////////////////
PresAndTemp ReadPressureAndTemperature(uint8_t channel)
{
  static int errorCounter[6] = {0,0,0,0,0,0};
  static PresAndTemp oldTemp[6];
  Adafruit_MPL3115A2 baro = Adafruit_MPL3115A2();
  tcaselect(channel);
  if (baro.begin())
  {
	  PresAndTemp temp;

	  temp.pressure = baro.getPressure()*0.01;
	  temp.temperature = baro.getTemperature();

	  if(temp.pressure < MIN_PRESSURE || temp.pressure > MAX_PRESSURE)
	  {
		  errorCounter[channel]++;
	  }
	  else
	  {
		  errorCounter[channel] = 0;
		  oldTemp[channel].pressure = temp.pressure;
		  oldTemp[channel].temperature = temp.temperature;
	  }

	  return oldTemp[channel];
  }
  else if(errorCounter[channel]++ < 10 )
  {
	  return oldTemp[channel];
  }
  else
  {
	  oldTemp[channel].pressure = 0.0;
	  oldTemp[channel].temperature = 0.0;

	  return oldTemp[channel];
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  ReadTemperature(uint8_t channel)
//  Reads the pressure temperature in °C from the chosen channel from the MCP9808
//  Input: uint8_t i -> channel (0-3)
/////////////////////////////////////////////////////////////////////////////////////////////////////
float ReadTemperature(uint8_t channel)
{
  Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
  static int errorCounter[MCP9808_CH4+1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};;
  static float oldTemp[MCP9808_CH4+1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  unsigned int address = 0x00;
  float temperature = 0;
  switch(channel)
  {
  	  case MCP9808_CH1:
  	  case MCP9808_CH2:
  	  case MCP9808_CH3:
  		  tcaselect(channel);
  		  address = 0x18;
  		  break;

  	  case MCP9808_CH4:
  		  tcaselect(channel-0x10);
  		  address = 0x1C;
  		  break;

  }

  if (tempsensor.begin(address))
  {
	  temperature = tempsensor.readTempC();

	  errorCounter[channel] = 0;

	  oldTemp[channel] = temperature;

	  return oldTemp[channel];

  }
  else if(errorCounter[channel]++ < 30 )
  {
	  return oldTemp[channel];
  }
  else
  {
	  oldTemp[channel] = 0.0;
	  return oldTemp[channel];
  }
}



/////////////////////////////////////////////////////////////////////////////////////////////////////
//  Mux_LCD_init()
//  Initialization of the display in conjunction with the I2C multiplexer
//  Input: none
/////////////////////////////////////////////////////////////////////////////////////////////////////
void Mux_LCD_init()
{
  tcaselect(MUX_LCD);
  lcd.init();
  lcd.backlight();

  byte check[8] = {
    0b00000,
    0b00000,
    0b00001,
    0b00010,
    0b10100,
    0b01000,
    0b00000,
    0b00000
  };

  byte cross[8] = {
    0b00000,
    0b00000,
    0b10001,
    0b01010,
    0b00100,
    0b01010,
    0b10001,
    0b00000
  };

  byte degree[8] = {
	0b00110,
	0b01001,
	0b01001,
	0b00110,
	0b00000,
	0b00000,
	0b00000,
	0b00000
};

  byte oe_small[8] = {
  	0b00000,
  	0b01010,
  	0b00000,
  	0b01110,
  	0b10001,
  	0b10001,
  	0b01110,
  	0b00000
  };

  byte micro[8] = {
  	0b00000,
  	0b10001,
  	0b10001,
  	0b11001,
  	0b10110,
  	0b10000,
  	0b10000,
  	0b00000
  };

  lcd.createChar(0,check);
  lcd.createChar(1,cross);
  lcd.createChar(2,degree);
  lcd.createChar(3,oe_small);
  lcd.createChar(4,micro);

  lcd.setCursor(5,0);
  lcd.print("Lasersaur");
  lcd.setCursor(6,1);
  lcd.print("FREILab");
  lcd.setCursor(7,2);
  lcd.print("v1.0");

  delay(2000);

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  Mux_LCD_init()
//  Clearing of LCD
//  Input: none
/////////////////////////////////////////////////////////////////////////////////////////////////////
void Mux_LCD_clear()
{
  tcaselect(MUX_LCD);
  lcd.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  stopSystem()
//  Stops the system
//  Input: none
/////////////////////////////////////////////////////////////////////////////////////////////////////
void stopSystem()
{
  while(1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  LCD_Display()
//  Handling of Display
//  Input: none
/////////////////////////////////////////////////////////////////////////////////////////////////////
void LCD_Display()
{
    tcaselect(MUX_LCD);
    switch(DisplayScreen)
    {
      case 0:
		  lcd.setCursor(0, 0);
		  lcd.print("-------Wasser-------");
		  lcd.setCursor(0, 1);
		  lcd.print(" Vor R");
		  lcd.write(byte(3));
		  lcd.print("hre: ");
		  lcd.print(MeasurementData.WaterTempBT,1);
		  lcd.write(byte(2));
		  lcd.print("C");
		  lcd.setCursor(0, 2);
		  lcd.print("Nach R");
		  lcd.write(byte(3));
		  lcd.print("hre: ");
		  lcd.print(MeasurementData.WaterTempAT,1);
		  lcd.write(byte(2));
		  lcd.print("C");
		  lcd.setCursor(0, 3);
		  lcd.print("Durchfl.: ");
          lcd.print(MeasurementData.WaterFlow,1);
          lcd.print("l/min");
		  break;

      case 1:
          lcd.setCursor(0, 0);
          lcd.print("----Druck (");
          lcd.write(byte(4));
          lcd.print("Bar)----");
		  lcd.setCursor(0, 1);
		  lcd.print("Grob: ");
		  lcd.print(fabs(MeasurementData.PresFilter1),2);
		  lcd.setCursor(0, 2);
		  lcd.print("Fein: ");
		  lcd.print(fabs(MeasurementData.PresFilter2),2);
		  lcd.setCursor(0, 3);
		  lcd.print("Kohle: ");
		  lcd.print(fabs(MeasurementData.PresFilter3),2);
		  break;

      case 2:
          lcd.setCursor(0, 0);
          lcd.print("--------Luft--------");
		  lcd.setCursor(0, 1);
		  lcd.print("Temperatur: ");
		  lcd.print(MeasurementData.Data1Filter.temperature,1);
		  lcd.write(byte(2));
		  lcd.print("C");
		  break;

      case 3:
          lcd.setCursor(0, 0);
          lcd.print("-------Linse--------");
		  lcd.setCursor(0, 1);
		  lcd.print("Temperatur: ");
		  lcd.print(MeasurementData.LenseTemp,1);
		  lcd.write(byte(2));
		  lcd.print("C");
		  break;

      case 4:
          lcd.setCursor(0, 0);
          lcd.print("------Temp (");
          lcd.write(byte(2));
          lcd.print("C)-----");
		  lcd.setCursor(0, 1);
		  lcd.print("T1: ");
		  lcd.print(MeasurementData.Temp1,1);
		  lcd.setCursor(0, 2);
		  lcd.print("T2: ");
		  lcd.print(MeasurementData.Temp2,1);
		  lcd.setCursor(11, 1);
		  lcd.print("T3: ");
		  lcd.print(MeasurementData.Temp3,1);
		  lcd.setCursor(11,2);
		  lcd.print("T4: ");
		  lcd.print(MeasurementData.Temp4,1);
		  break;


	  //Error Screens
      case ERR_BASE + ERR_WATERTEMP:
    	  lcd.setCursor(0, 0);
          lcd.print("-------FEHLER-------");
          lcd.setCursor(0, 1);
          lcd.print("Wassertemperatur");
          lcd.setCursor(0, 2);
          lcd.print("zu hoch!");
          lcd.setCursor(0, 3);
          lcd.print(MeasurementData.WaterTempBT,1);
		  lcd.write(byte(2));
		  lcd.print("C, ");
          lcd.print(MeasurementData.WaterTempAT,1);
		  lcd.write(byte(2));
		  lcd.print("C");
          break;

      case ERR_BASE + ERR_WATERFLOW:
    	  lcd.setCursor(0, 0);
          lcd.print("-------FEHLER-------");
          lcd.setCursor(0, 1);
          lcd.print("Wasserfluss");
          lcd.setCursor(0, 2);
          lcd.print("zu gering!");
          lcd.setCursor(0, 3);
          lcd.print(MeasurementData.WaterFlow,1);
          lcd.print("l/min");
          break;

      case ERR_BASE + ERR_PRESSURE_DIFF:
    	  lcd.setCursor(0, 0);
          lcd.print("-------FEHLER-------");
          lcd.setCursor(0, 1);
          lcd.print("Druckunterschied");
          lcd.setCursor(0, 2);
          lcd.print("zu hoch!");
          lcd.setCursor(0, 3);
		  lcd.print(MeasurementData.PresFilter1);
		  lcd.print(", ");
		  lcd.print(MeasurementData.PresFilter2);
		  lcd.print(", ");
		  lcd.print(MeasurementData.PresFilter3);
          break;

      case ERR_BASE + ERR_PRESSURE:
    	  lcd.setCursor(0, 0);
          lcd.print("-------FEHLER-------");
          lcd.setCursor(0, 1);
          lcd.print("Messwerte Drucksen-");
          lcd.setCursor(0, 2);
          lcd.print("soren unplausibel!");
          lcd.setCursor(0, 3);
		  lcd.print(MeasurementData.Data1Filter.pressure,0);
		  lcd.print(", ");
		  lcd.print(MeasurementData.Data2Filter.pressure,0);
		  lcd.print(", ");
		  lcd.print(MeasurementData.Data3Filter.pressure,0);
		  lcd.print(", ");
		  lcd.print(MeasurementData.Data4Filter.pressure,0);
          break;

      case ERR_BASE + ERR_LENSETEMP:
    	  lcd.setCursor(0, 0);
          lcd.print("-------FEHLER-------");
          lcd.setCursor(0, 1);
          lcd.print("Linsentemperatur");
          lcd.setCursor(0, 2);
          lcd.print("zu hoch!");
          lcd.setCursor(0, 3);
          lcd.print(MeasurementData.LenseTemp,1);
		  lcd.write(byte(2));
		  lcd.print("C");
          break;

      case ERR_BASE + ERR_RFID:
    	  lcd.setCursor(0, 0);
          lcd.print("-------FEHLER-------");
          lcd.setCursor(0, 1);
          lcd.print("RFID");
          lcd.setCursor(0, 2);
          lcd.print("Authentifizierung");
          lcd.setCursor(0, 3);
          lcd.print("fehlgeschlagen!");
          break;

      case ERR_BASE + ERR_LASERADJUSTMENT:
		  lcd.setCursor(0, 0);
		  lcd.print("-------FEHLER-------");
		  lcd.setCursor(0, 1);
		  lcd.print("Laser eventuell");
		  lcd.setCursor(0, 2);
		  lcd.print("nicht mehr");
		  lcd.setCursor(0, 3);
		  lcd.print("ausgerichtet!");
		  break;

        default:
              break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  TempLookup()
//  NTC temperature lookup
//  Input: ADC value
/////////////////////////////////////////////////////////////////////////////////////////////////////
float TempLookup(uint16_t AdcValue)
{
  return -0.1107 * AdcValue + 103.27;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  FlowLookup()
//  NTC temperature lookup
//  Input: ADC value
/////////////////////////////////////////////////////////////////////////////////////////////////////
float FlowLookup(uint16_t freq)
{
  return (0.3931 * freq + 0.8322) * 0.155844;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  ErrorHandling()
//  Error Handling
//  Input: none
/////////////////////////////////////////////////////////////////////////////////////////////////////
void ErrorHandling()
{
	if(MeasurementData.WaterTempBT >= MAX_WATER_TEMP || MeasurementData.WaterTempAT >= MAX_WATER_TEMP)
		Errors |= ERR_WATERTEMP;
	else
		Errors &= ~ERR_WATERTEMP;

	if(MeasurementData.Data1Filter.pressure < MIN_PRESSURE || MeasurementData.Data2Filter.pressure < MIN_PRESSURE || MeasurementData.Data3Filter.pressure < MIN_PRESSURE || MeasurementData.Data4Filter.pressure < MIN_PRESSURE ||
	   MeasurementData.Data1Filter.pressure > MAX_PRESSURE || MeasurementData.Data2Filter.pressure > MAX_PRESSURE || MeasurementData.Data3Filter.pressure > MAX_PRESSURE || MeasurementData.Data4Filter.pressure > MAX_PRESSURE)
	{
		Errors |= ERR_PRESSURE;
	}
	else
		Errors &= ~ERR_PRESSURE;

	if(fabs(MeasurementData.PresFilter1) >= MAX_PRESSURE_DIFF || fabs(MeasurementData.PresFilter2) >= MAX_PRESSURE_DIFF || fabs(MeasurementData.PresFilter3) >= MAX_PRESSURE_DIFF)
	{
		Errors |= ERR_PRESSURE_DIFF;
	}
	else
		Errors &= ~ERR_PRESSURE_DIFF;

	if(MeasurementData.WaterFlow < MIN_WATERFLOW)
		Errors |= ERR_WATERFLOW;
	else
		Errors &= ~ERR_WATERFLOW;

	if(MeasurementData.LenseTemp > MAX_LENSETEMP)
		Errors |= ERR_LENSETEMP;
	else
		Errors &= ~ERR_LENSETEMP;

//	if(tempDiffTooHigh)
//	{
//		Errors |= ERR_LASERADJUSTMENT;
//		EEPROM.write(0x10, 0xF0);
//	}

	if(!digitalRead(PIN_RFID_EN))
	{
		Errors |= ERR_RFID;
		digitalWrite(PIN_RLY_DRVBRD, LOW);
	}
	else
	{
		Errors &= ~ERR_RFID;
		digitalWrite(PIN_RLY_DRVBRD, HIGH);
	}

	#ifdef _DEBUG
	Errors = 0;
	#endif

	if(Errors == 0)
	{
		digitalWrite(PIN_LED, LOW);
		digitalWrite(PIN_SW_EN, HIGH);
		digitalWrite(PIN_WPTH_EN, HIGH);
	}
	else
	{
		digitalWrite(PIN_LED, HIGH);
		digitalWrite(PIN_SW_EN, LOW);
		digitalWrite(PIN_WPTH_EN, LOW);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//  getTick()
//  Returns number of ticks for the Input Capture
//  Input: none
/////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t getTick()
{
	uint32_t akaTick;       // holds a copy of the tick count so we can return it after re-enabling interrupts
	cli();             //disable interrupts
	akaTick = Ticks;
	sei();             // enable interrupts
	return akaTick;
}
