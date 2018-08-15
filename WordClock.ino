extern "C"
{
#include "user_interface.h"  	  // Required for wifi_station_connect() to work
}

#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino
#include <WiFiUdp.h>
#include <time.h>

#include <EEPROM.h>
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>
#include <fastled_config.h>
#include <fastled_delay.h>
#include <fastled_progmem.h>
#include <fastpin.h>
#include <fastspi.h>
#include <fastspi_bitbang.h>
#include <fastspi_dma.h>
#include <fastspi_nop.h>
#include <fastspi_ref.h>
#include <fastspi_types.h>
#include <hsv2rgb.h>
#include <led_sysdefs.h>
#include <lib8tion.h>
#include <noise.h>
#include <pixelset.h>
#include <pixeltypes.h>
#include <platforms.h>
#include <power_mgt.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <NTPClient.h>			  // https://github.com/arduino-libraries/NTPClient
#include <Timezone.h>    		  // https://github.com/JChristensen/Timezone

#define FPM_SLEEP_MAX_TIME 0xFFFFFFF

#define NUM_LEDS 121
#define DATA_PIN 14
#define COLOR_ORDER RGB
#define EEPROM_SIZE 128

// configure your timezone rules here how described on https://github.com/JChristensen/Timezone
TimeChangeRule myDST =
{ "CEST", Last, Sun, Mar, 2, +120 };    //Daylight time = UTC + 2 hours
TimeChangeRule mySTD =
{ "CET", Last, Sun, Oct, 2, +60 };      //Standard time = UTC + 1 hours
Timezone myTZ(myDST, mySTD);

// words for log output
static const char twords[][12] =
{ "", "Eins", "Zwei", "Drei", "Vier", "Fuenf", "Sechs", "Sieben", "Acht", "Neun", "Zehn", "Elf", "Zwoelf", "Ein",
		"Zwanzig", "vor", "nach", "um", "viertel", "halb", "dreiviertel", "Fuenf", "Zehn", "Es", "ist", "etwa", "genau",
		"Uhr" };

// word indices
enum
{
	_NULL_,
	_EINS_,
	_ZWEI_,
	_DREI_,
	_VIER_,
	_FUENF_,
	_SECHS_,
	_SIEBEN_,
	_ACHT_,
	_NEUN_,
	_ZEHN_,
	_ELF_,
	_ZWOELF_,
	_EIN_,
	_ZWANZIG_M_,
	_VOR_,
	_NACH_,
	_UM_,
	_VIERTEL_,
	_HALB_,
	_DREIVIERTEL_,
	_FUENF_M_,
	_ZEHN_M_,
	_ES_,
	_IST_,
	_ETWA_,
	_GENAU_,
	_UHR_
};

// display layout and led order
/*
 E	S	K	I	S	T	L	E	T	W	A		110 -> 120
 |
 O	G	E	N	A	U	B	F	�	N	F		109 <- 099
 |
 Z	E	H	N	Z	W	A	N	Z	I	G		088 -> 098
 |
 D	R	E	I	V	I	E	R	T	E	L		087 <- 077
 |
 T	G	N	A	C	H	V	O	R	U	M		066 -> 076
 |
 H	A	L	B	Q	Z	W	�	L	F	P		065 <- 055
 |
 Z 	W	E	I	N	S	I	E	B	E	N		044 -> 054
 |
 K	D	R	E	I	R	H 	F	�	N	F		043 <- 033
 |
 E	L	F	N	E	U	N	V	I	E	R		022 -> 032
 |
 W	A	C	H	T	Z	E	H	N	R	S		021 <- 011
 |
 B	S	E	C	H	S	F	M	U	H	R		000 -> 010
 */

// word positions by led numbers {from, to}
static const int tleds[][2] =
{
{ 0, 0 },
{ 46, 49 }, 	// EINS
		{ 44, 47 }, 	// ZWEI
		{ 39, 42 }, 	// DREI
		{ 29, 32 }, 	// VIER
		{ 33, 36 },		// FUENF
		{ 1, 5 },		// SECHS
		{ 49, 54 },		// SIEBEN
		{ 17, 20 },		// ACHT
		{ 25, 28 },		// NEUN
		{ 13, 16 },		// ZEHN
		{ 22, 24 },		// ELF
		{ 56, 60 },		// ZWOELF
		{ 46, 48 },		// EIN
		{ 92, 98 },		// ZWANZIG_M
		{ 72, 74 },		// VOR
		{ 68, 71 },		// NACH
		{ 75, 76 },		// UM
		{ 77, 83 },		// VIERTEL
		{ 62, 65 },		// HALB
		{ 77, 87 },		// DREIVIERTEL
		{ 99, 102 },	// FUENF_M
		{ 88, 91 },		// ZEHN_M
		{ 110, 111 },	// ES
		{ 113, 115 },	// IST
		{ 117, 120 },	// ETWA
		{ 104, 108 },	// GENAU
		{ 8, 10 } };		// UHR

// translation from minutes to word expressions (east and west german)
static const int CALC[2][12][4] =
{
{
{ 0, 0, _UM_, 0 },
{ _FUENF_M_, _NACH_, 0, 0 },
{ _ZEHN_M_, _NACH_, 0, 0 },
{ 0, 0, _VIERTEL_, 1 },
{ _ZEHN_M_, _VOR_, _HALB_, 1 },
{ _FUENF_M_, _VOR_, _HALB_, 1 },
{ 0, 0, _HALB_, 1 },
{ _FUENF_M_, _NACH_, _HALB_, 1 },
{ _ZEHN_M_, _NACH_, _HALB_, 1 },
{ 0, 0, _DREIVIERTEL_, 1 },
{ _ZEHN_M_, _VOR_, 0, 1 },
{ _FUENF_M_, _VOR_, 0, 1 } },
{
{ 0, 0, _UM_, 0 },
{ _FUENF_M_, _NACH_, 0, 0 },
{ _ZEHN_M_, _NACH_, 0, 0 },
{ _VIERTEL_, _NACH_, 0, 0 },
{ _ZWANZIG_M_, _NACH_, 0, 0 },
{ _FUENF_M_, _VOR_, _HALB_, 1 },
{ 0, 0, _HALB_, 1 },
{ _FUENF_M_, _NACH_, _HALB_, 1 },
{ _ZWANZIG_M_, _VOR_, 0, 1 },
{ _VIERTEL_, _VOR_, 0, 1 },
{ _ZEHN_M_, _VOR_, 0, 1 },
{ _FUENF_M_, _VOR_, 0, 1 } } };

typedef struct
{
	int mode;
	int precise;
	int trailer;
	unsigned char r, g, b;
	unsigned char brightness;
	unsigned char valid;
} wordclock_word_processor_struct;

wordclock_word_processor_struct wordclock_word_processor;
wordclock_word_processor_struct *wordp = &wordclock_word_processor;

CRGB leds[NUM_LEDS];

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
WiFiManager wifiManager;

void update_outputs(void);

ESP8266WebServer server(80);   //instantiate server at port 80 (http port)

char tstr2[128] = "";

void page_out(void)
{
	char tstr[16];

	server.sendContent(
			"<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html><head><meta http-equiv=\"content-type\"content=\"text/html; charset=ISO-8859-1\"><title>ESP8266_WordClock_Server</title></head><body><h1 style=\"text-align: center; width: 504px;\" align=\"left\"><span style=\"color: rgb(0, 0, 153); font-weight: bold;\">ESP8266 WordClock-Server</span></h1><h3 style=\"text-align: center; width: 504px;\" align=\"left\"><span style=\"color: rgb(0, 0, 0); font-weight: bold;\">");
	server.sendContent(tstr2);
	server.sendContent(
			"</span></h3><form method=\"get\"><table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" height=\"144\" width=\"457\"><tbody><tr><td width=\"120\"><b><big>Anzeigemodus</big></b></td><td width=\"50\"></td><td><select name=\"MODE\"><option value=\"0\"");
	if (!wordp->mode)
		server.sendContent(" selected");
	server.sendContent(">Ostdeutsch></option><option value=\"1\"");
	if (wordp->mode)
		server.sendContent(" selected");
	server.sendContent(
			">Westdeutsch></option></select></td></tr><tr><td><b><big>Pr&#228;zision</big></b></td><td></td><td><input name=\"PREC_ETWA\" type=\"Checkbox\" mode=\"submit\"");
	if (wordp->precise & 0x02)
		server.sendContent(" checked");
	server.sendContent("> etwa <input name=\"PREC_GENAU\" type=\"Checkbox\" mode=\"submit\"");
	if (wordp->precise & 0x01)
		server.sendContent(" checked");
	server.sendContent(
			"> genau </td></tr><tr><td><b><big>Trailer</big></b></td><td></td> <td><input name=\"TRAILER\" type=\"Checkbox\" mode=\"submit\"");
	if (wordp->trailer)
		server.sendContent(" checked");
	server.sendContent(
			"> Uhr</td></tr><tr><td height=\"30\"></td><td></td><td></td></tr><tr><td><b><big>Helligkeit</big></b></td><td></td><td><input maxlength=\"3\" size=\"3\" name=\"BRIGHT\" value=\"");
	server.sendContent(String(wordp->brightness));
	server.sendContent(
			"\"> %</td></tr><tr><td><b><big><font color=\"#cc0000\">Rot</font></big></b></td><td></td><td><input maxlength=\"3\" size=\"3\" name=\"RED\" value=\"");
	server.sendContent(String(wordp->r));
	server.sendContent(
			"\"> %</td></tr><tr><td><b><big><font color=\"#006600\">Gr&#252;n</font></big></b></td><td></td><td><input maxlength=\"3\" size=\"3\" name=\"GREEN\" value=\"");
	server.sendContent(String(wordp->g));
	server.sendContent(
			"\"> %</td></tr><tr><td><b><big><font color=\"#000099\">Blau</font></big></b></td><td></td><td><input maxlength=\"3\" size=\"3\" name=\"BLUE\" value=\"");
	server.sendContent(String(wordp->b));
	server.sendContent(
			"\"> %</td></tr></tbody></table><br><table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" height=\"30\" width=\"99\"><tbody><tr><td><input name=\"SEND\" value=\"  Senden  \" type=\"submit\"></td></tr></tbody></table></form><table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" height=\"30\" width=\"99\"><tbody><tr><td><form method=\"get\"><input name=\"SAVE\" value=\"Speichern\" type=\"submit\"></form></td></tr></tbody></table></body></html>");
}

void setup()
{
	int i;
	unsigned char *eptr;

	pinMode(BUILTIN_LED, OUTPUT);   // Initialize the BUILTIN_LED pin as an output
	digitalWrite(BUILTIN_LED, HIGH);
	Serial.begin(115200);

	wifiManager.setTimeout(180);

	FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
	memset(leds, 0, NUM_LEDS);
	FastLED.show();

	EEPROM.begin(EEPROM_SIZE);
	eptr = (unsigned char*) wordp;
	for (i = 0; i < sizeof(wordclock_word_processor_struct); i++)
		*(eptr++) = EEPROM.read(i);
	if (wordp->valid != 0xA5)
	{
		wordp->mode = 0;
		wordp->precise = 0;
		wordp->trailer = 0;
		wordp->r = 0;
		wordp->g = 79;
		wordp->b = 100;
		wordp->brightness = 80;
		wordp->valid = 0xA5;
	}
	eptr = (unsigned char*) wordp;
	for (i = 0; i < sizeof(wordclock_word_processor_struct); i++)
		EEPROM.write(i, *(eptr++));
	EEPROM.commit();

	//fetches ssid and pass and tries to connect
	//if it does not connect it starts an access point with the specified name
	//here  "NixieAP"
	//and goes into a blocking loop awaiting configuration

	if (!wifiManager.autoConnect("WordClockAP"))
	{
		Serial.println("failed to connect and hit timeout");
		delay(3000);
		//reset and try again, or maybe put it to deep sleep
		ESP.reset();
		delay(5000);
	}

	//if you get here you have connected to the WiFi
	Serial.println("connected...yeey :)");

	timeClient.update();

	server.on("/", []()
	{
		if(server.args())
		{
			for(int i = 0; i < server.args(); i++)
			{
				if(server.argName(i) == "SEND")
				{
					wordp->precise = 0;
					wordp->trailer = 0;
				}
			}
			for(int i = 0; i < server.args(); i++)
			{
				if(server.argName(i) == "SAVE")
				{
					unsigned char *eptr = (unsigned char*)wordp;

					for(int i = 0; i < sizeof(wordclock_word_processor_struct); i++)
					EEPROM.write(i, *(eptr++));
					EEPROM.commit();
				}
				else if(server.argName(i) == "MODE")
				{
					wordp->mode = abs(atoi(server.arg(i).c_str()));
					if(wordp->mode > 1)
					wordp->mode = 1;
				}
				else if(server.argName(i) == "BRIGHT")
				{
					wordp->brightness = abs(atoi(server.arg(i).c_str()));
					if(wordp->brightness > 100)
					wordp->mode = 100;
				}
				else if(server.argName(i) == "RED")
				{
					wordp->r = abs(atoi(server.arg(i).c_str()));
					if(wordp->r > 100)
					wordp->r = 100;
				}
				else if(server.argName(i) == "GREEN")
				{
					wordp->g = abs(atoi(server.arg(i).c_str()));
					if(wordp->g > 100)
					wordp->g = 100;
				}
				else if(server.argName(i) == "BLUE")
				{
					wordp->b = abs(atoi(server.arg(i).c_str()));
					if(wordp->b > 100)
					wordp->b = 100;
				}
				else if(server.argName(i) == "PREC_ETWA")
				{
					if(server.arg(i) == "on")
						wordp->precise |= 0x02;
					else
						wordp->precise &= ~0x02;
				}
				else if(server.argName(i) == "PREC_GENAU")
				{
					if(server.arg(i) == "on")
						wordp->precise |= 0x01;
					else
						wordp->precise &= ~0x01;
				}
				else if(server.argName(i) == "TRAILER")
				{
					if(server.arg(i) == "on")
					wordp->trailer = 1;
				}
			}
			*tstr2 = 0;
			update_outputs();
		}
		page_out();
	});
	server.begin();
	Serial.println("Web server started!");
}

static unsigned long amicros, umicros = 0;

void update_outputs(void)
{
	char tstr[128] = "";
	unsigned int i, j, emin, min, hr;
	time_t rawtime, loctime, atime;
	int lvals[10];

	if (timeClient.update())						// NTP-update
	{
		umicros = amicros;
		rawtime = timeClient.getEpochTime();		// get NTP-time
		loctime = myTZ.toLocal(rawtime);			// calc local time

		emin = minute(loctime);
		rawtime += 150;
		atime = myTZ.toLocal(rawtime);			// calc local time

		min = minute(atime) / 5;
		hr = (hour(atime) + CALC[wordp->mode][min][3]) % 12;
		if (!hr)
			hr = 12;

		j = 0;
		lvals[j++] = _ES_;
		lvals[j++] = _IST_;
		if (emin % 5)
		{
			if (wordp->precise & 0x02)
				lvals[j++] = _ETWA_;
		}
		else
		{
			if (wordp->precise & 0x01)
				lvals[j++] = _GENAU_;
		}
		for (i = 0; i < 3; i++)
		{
			if (CALC[wordp->mode][min][i])
			{
				lvals[j++] = CALC[wordp->mode][min][i];
			}
		}
		lvals[j++] = (hr == 1) ? ((wordp->trailer) ? _EIN_ : _EINS_) : hr;
		if (wordp->trailer)
			lvals[j++] = _UHR_;
		lvals[j] = 0;

//			if ((!second(loctime)) || firstrun)			// full minute or first cycle
		{
			CRGB actcolor = CRGB(((int) wordp->r * (int) wordp->brightness) / 40,
					((int) wordp->g * (int) wordp->brightness) / 40, ((int) wordp->b * (int) wordp->brightness) / 40);

			memset(leds, 0, sizeof(leds));
			*tstr = 0;
			for (i = 0; lvals[i]; i++)
			{
				for (j = tleds[lvals[i]][0]; j <= tleds[lvals[i]][1] && j < NUM_LEDS; j++)
				{
					leds[j] = actcolor;
				}
				sprintf(tstr + strlen(tstr), "%s ", twords[lvals[i]]);
			}
			if (strcmp(tstr, tstr2))
			{
				digitalWrite(BUILTIN_LED, HIGH);		// blink for sync
				strcpy(tstr2, tstr);
				sprintf(tstr + strlen(tstr), " (%02d:%02d:%02d)\n", hour(loctime), minute(loctime), second(loctime));
				Serial.println(tstr);					// send to console
				delay(100);
				digitalWrite(BUILTIN_LED, LOW);
			}
			else
				delay(100);

			FastLED.show();
//				delay(58000 - ((micros() - amicros) / 1000) - (second(loctime) * 1000)); // wait for end of minute
//				firstrun = 0;
		}
	}
}

void loop()
{
	for (;;)
	{
		amicros = micros();
		update_outputs();
		delay(150);
		server.handleClient();
		if (((amicros - umicros) / 1000000L) > 3600)	// if no sync for more than one hour
			digitalWrite(BUILTIN_LED, HIGH);			// switch off LED
	}
}

