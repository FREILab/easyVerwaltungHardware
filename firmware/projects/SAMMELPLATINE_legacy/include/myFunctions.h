#ifndef MYFUNCTIONS_H
#define MYFUNCTIONS_H

#include <Wire.h>
extern "C" {
#include <utility/twi.h>  // from Wire library, so we can do bus scanning
}
#include <Adafruit_MPL3115A2.h>
#include "Adafruit_MCP9808.h"
#include <LiquidCrystal_I2C.h>
#include <Ethernet2.h>

//---------------------------------------------------------------------------------
//------------------------------------DEFINES--------------------------------------
//---------------------------------------------------------------------------------

//#define _DEBUG //used for debugging. Has to be deactived inside the Lasersaur, because otherwise the warning LED will not work
#ifdef _DEBUG
	#warning "Debug mode activated!"
#endif

//Number of LCD screens to roll trough
#define LCD_SCREENS 5

//LCD ticker period
#define LCD_TICKER_S 3

//I2C mux channel
#define TCAADDR 0x70

//Pressure sensors mux channel
#define PRESSURESENSOR4 5
#define PRESSURESENSOR3 4
#define PRESSURESENSOR2 1
#define PRESSURESENSOR1 0

//GPIO config
#define PIN_EEPROM_RESET 0
#define PIN_LED 1
#define PIN_RLY_KOMP 2
#define PIN_RLY_DRVBRD 3
#define PIN_KOMP 4
#define PIN_CHI_ABLUFT 5
#define PIN_SW_EN 6
#define PIN_WPTH_EN 7
#define PIN_WF_RPM 8
#define PIN_RFID_EN 9

//ADC config
#define ADC_FL 0
#define ADC_WT_AT 1
#define ADC_WT_BT 2
#define ADC_LT 3

//MCP9808
#define MCP9808_CH1 3
#define MCP9808_CH2 2
#define MCP9808_CH3 6
#define MCP9808_CH4 6+0x10

//I2C Mux channel for LCD
#define MUX_LCD 7

//Error definitions
#define ERR_WATERTEMP 		(1L<<0)
#define ERR_WATERFLOW		(1L<<1)
#define ERR_PRESSURE_DIFF	(1L<<2)
#define ERR_PRESSURE		(1L<<3)
#define ERR_LENSETEMP		(1L<<4)
#define ERR_RFID			(1L<<5)
#define ERR_RAINSENSE		(1L<<6)
#define ERR_LASERADJUSTMENT (1L<<7)

#define ERR_BASE			0xA0

#define MAX_ERRORS			8

#define MAX_WATER_TEMP		35		//°C
#define MAX_PRESSURE_DIFF	10000	//µBar
#define MIN_PRESSURE		500		//mBar
#define MAX_PRESSURE		1500	//mBar
#define MAX_LENSETEMP		40		//°C
#define MIN_WATERFLOW		2		//l/m
#define MAX_TEMPDIFF		5		//°C

#define WATERFLOW_TIMEOUT	2		//s
//---------------------------------------------------------------------------------
//------------------------------------DEFINITIONS----------------------------------
//---------------------------------------------------------------------------------

struct PresAndTemp
{
  float pressure;		//mBar
  float temperature;	//°C
};

struct MeasData
{
  PresAndTemp Data1Filter;
  PresAndTemp Data2Filter;
  PresAndTemp Data3Filter;
  PresAndTemp Data4Filter;
  float		PresFilter1;
  float		PresFilter2;
  float		PresFilter3;
  float     WaterTempBT;
  float     WaterTempAT;
  float		WaterFlow;
  float		LenseTemp;
  float		Temp1;
  float		Temp2;
  float		Temp3;
  float		Temp4;
};

//---------------------------------------------------------------------------------
//--------------------------------GLOBAL VARIABLES---------------------------------
//---------------------------------------------------------------------------------

extern struct MeasData MeasurementData;
extern uint16_t DisplayCounter, DisplayScreen;
extern uint16_t Errors;
extern volatile unsigned int Ticks;         // holds the pulse count as .5 us ticks
extern bool tempDiffTooHigh;

//---------------------------------------------------------------------------------
//--------------------------------FUNCTION PROTOTYPES------------------------------
//---------------------------------------------------------------------------------
void HsLoop(void);
void configIO(void);
void tcaselect(uint8_t channel);
void ScanI2CForDevices(void);
PresAndTemp ReadPressureAndTemperature(uint8_t channel);
float ReadTemperature(uint8_t channel);
void Mux_LCD_init(void);
void Mux_LCD_clear(void);
void stopSystem(void);
void LCD_Display();
float TempLookup(uint16_t AdcValue);
float FlowLookup(uint16_t freq);
void ErrorHandling();
uint32_t getTick();

#endif
