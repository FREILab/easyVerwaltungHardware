#include <Arduino.h>
#include <EEPROM.h>
#include <Ethernet2.h>
#include "myFunctions.h"
#include "ArduinoJson-v5.13.0.h"

uint16_t DisplayErrorScreen = 0;
uint32_t tempTicks = 0;

bool WaterFlowStopped = false;

uint16_t oldAbluftState = LOW;
bool firstPush = true;
uint16_t KompState = LOW;

float offsetPres[4] = {0.0, 0.0, 0.0, 0.0};
bool firstStart = true;

float oldTemp[4] = {1000.0, 1000.0, 1000.0, 1000.0};

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
IPAddress server(192,168,0,45);  // numeric IP for Google (no DNS)
//char server[] = "192.168.0.45";    // name address for Google (using DNS)

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

StaticJsonBuffer<300> jsonBuffer;

String JSONString;

unsigned int logCounter = 0;


void setup() {

//	Serial.begin(115200);

	MeasurementData.Temp1 = 0.0;
	MeasurementData.Temp2 = 0.0;
	MeasurementData.Temp3 = 0.0;
	MeasurementData.Temp4 = 0.0;

	configIO();

	Wire.setClock(1000);

	Mux_LCD_init();

	ScanI2CForDevices();    //if pressure sensors are not found, the system will be stopped here!

	// Timer0 is already used for millis() - we'll just interrupt somewhere
	// in the middle and call the "Compare A" function below
	OCR0A = 0xAF;
	TIMSK0 |= _BV(OCIE0A);

	TCCR1A = 0x00;         // COM1A1=0, COM1A0=0 => Disconnect Pin OC1 from Timer/Counter 1 -- PWM11=0,PWM10=0 => PWM Operation disabled
	TCCR1B = 0x05;         // 16MHz clock with prescaler means TCNT1 increments every 64 uS (cs12 and CS10 bit set)
	Ticks = 0;              // default value indicating no pulse detected
	TIMSK1 = _BV(ICIE1) | _BV(TOIE1);   // enable input capture interrupt for timer 1

	Mux_LCD_clear();

	MeasurementData.Temp1 = 0.0;
	MeasurementData.Temp2 = 0.0;
	MeasurementData.Temp3 = 0.0;
	MeasurementData.Temp4 = 0.0;
	MeasurementData.Data1Filter.pressure = 0.0;
	MeasurementData.Data2Filter.pressure = 0.0;
	MeasurementData.Data3Filter.pressure = 0.0;
	MeasurementData.Data4Filter.pressure = 0.0;

	Serial.println(EEPROM.read(0x10),DEC);

//	if(EEPROM.read(0x10) == 0xF0)
//		Errors |= ERR_LASERADJUSTMENT;

	  // start the Ethernet connection:
	  if (Ethernet.begin(mac) == 0) {
	    //Serial.println("Failed to configure Ethernet using DHCP");
	    // no point in carrying on, so do nothing forevermore:
	    // try to congifure using IP address instead of DHCP:
	    Ethernet.begin(mac, ip);
	  }

}

ISR(TIMER0_COMPA_vect)
{
	static int TempCounter = 0;

	if(DisplayCounter < (LCD_TICKER_S * 1000))
	{
		DisplayCounter++;
	}

	if(logCounter++ >= 60000)
	{
		  if (client.connect(server, 5000)) {
			  JsonObject& root = jsonBuffer.createObject();
			  root["WaterBT"] = MeasurementData.WaterTempBT;
			  root["WaterAT"] = MeasurementData.WaterTempAT;
			  root["PresF1"] = MeasurementData.PresFilter1;
			  root["PresF2"] = MeasurementData.PresFilter2;
			  root["PresF3"] = MeasurementData.PresFilter3;
			  root["WaterFlow"] = MeasurementData.WaterFlow;
			  root["T1"] = MeasurementData.Temp1;
			  root["T2"] = MeasurementData.Temp2;
			  root["T3"] = MeasurementData.Temp3;
			  root["T4"] = MeasurementData.Temp4;
			  client.println("POST /log HTTP/1.1");
			  client.println("Host: 192.168.0.45"); // or generate from your server variable to not hardwire
			  client.println("Content-Type: application/json");
			  client.println("Connection: close");
			  client.print("Content-Length: ");
			  client.println(JSONString.length());// number of bytes in the payload
			  client.println();// important need an empty line here
			  client.println(JSONString);// the payload
		  }
		  logCounter = 0;
	}

//	if(digitalRead(PIN_CHI_ABLUFT) == HIGH && oldAbluftState == LOW)
//	{
//		digitalWrite(PIN_RLY_KOMP,HIGH);
//		KompState = HIGH;
//	}
//	else if(digitalRead(PIN_CHI_ABLUFT) == LOW && oldAbluftState == HIGH)
//	{
//		digitalWrite(PIN_RLY_KOMP,LOW);
//		KompState = LOW;
//	}
//	else
//	{
//		if(digitalRead(PIN_KOMP) == LOW && firstPush)
//		{
//			KompState = !KompState;
//			digitalWrite(PIN_RLY_KOMP,KompState);
//			firstPush = false;
//		}
//		else if (digitalRead(PIN_KOMP) == HIGH && !firstPush)
//		{
//			firstPush = true;
//		}
//	}
//
//	oldAbluftState = digitalRead(PIN_CHI_ABLUFT);

	if(digitalRead(PIN_CHI_ABLUFT) == HIGH)
		digitalWrite(PIN_RLY_KOMP,HIGH);
	else
		digitalWrite(PIN_RLY_KOMP,LOW);

	if(TempCounter++ >= 1000)
	{
			if(MeasurementData.Temp1 - oldTemp[0] > MAX_TEMPDIFF)
				tempDiffTooHigh = true;
			if(MeasurementData.Temp2 - oldTemp[1] > MAX_TEMPDIFF)
				tempDiffTooHigh = true;
			if(MeasurementData.Temp3 - oldTemp[2] > MAX_TEMPDIFF)
				tempDiffTooHigh = true;
			if(MeasurementData.Temp4 - oldTemp[3] > MAX_TEMPDIFF)
				tempDiffTooHigh = true;

			Serial.println(MeasurementData.Temp1 - oldTemp[0],DEC);
			Serial.println(MeasurementData.Temp2 - oldTemp[1],DEC);
			Serial.println(MeasurementData.Temp3 - oldTemp[2],DEC);
			Serial.println(MeasurementData.Temp4 - oldTemp[3],DEC);

			oldTemp[0] = MeasurementData.Temp1;
			oldTemp[1] = MeasurementData.Temp2;
			oldTemp[2] = MeasurementData.Temp3;
			oldTemp[3] = MeasurementData.Temp4;

			TempCounter = 0;
	}
}

ISR(TIMER1_CAPT_vect){
  if( bit_is_set(TCCR1B ,ICES1)){       // was rising edge detected ?
      TCNT1 = 0;                        // reset the counter
      WaterFlowStopped = false;
  }
  else {                                // falling edge was detected
       Ticks = ICR1;
  }
  TCCR1B ^= _BV(ICES1);                 // toggle bit value to trigger on the other edge
}

ISR(TIMER1_OVF_vect)
{
	WaterFlowStopped = true;
}


void loop() {

//	if(digitalRead(PIN_EEPROM_RESET) == LOW)
//	{
//		EEPROM.write(0x10, 0x20);
//		Serial.println(EEPROM.read(0x10),DEC);
//		Errors &= ~ERR_LASERADJUSTMENT;
//	}

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
	{
		MeasurementData.WaterFlow = FlowLookup(10000000L / (2 * tempTicks * 64));
	}
	else
		MeasurementData.WaterFlow = 0;

	ErrorHandling();

	if(DisplayCounter >= (LCD_TICKER_S * 1000))
	{
		if(Errors == 0)
		{
			if(DisplayScreen < LCD_SCREENS-1)
			{
			  DisplayScreen++;
			}
			else
			  DisplayScreen = 0;
		}
		else
		{
			for(int i = 0; i < MAX_ERRORS; i++)
			{
				DisplayScreen = ERR_BASE + (Errors & (1<<DisplayErrorScreen));
				DisplayErrorScreen++;

				if(DisplayErrorScreen == MAX_ERRORS)
					DisplayErrorScreen = 0;

				if(DisplayScreen != ERR_BASE)
					break;
			}

		}

		DisplayCounter = 0;
		Mux_LCD_clear();
	}
	LCD_Display();

}
