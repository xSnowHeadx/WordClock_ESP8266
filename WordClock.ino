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

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <NTPClient.h>			  // https://github.com/arduino-libraries/NTPClient
#include <Timezone.h>    		  // https://github.com/JChristensen/Timezone

#include <errno.h>

// begin of individual settings

#define DATA_PIN 	 14			  // output-pin for LED-data (current D5)
#define COLOR_ORDER RGB			  // byte order in the LEDs data-stream, adapt to your LED-type

// configure your timezone rules here how described on https://github.com/JChristensen/Timezone
TimeChangeRule myDST =
{ "CEST", Last, Sun, Mar, 2, +120 };    //Daylight time = UTC + 2 hours
TimeChangeRule mySTD =
{ "CET", Last, Sun, Oct, 2, +60 };      //Standard time = UTC + 1 hours
Timezone myTZ(myDST, mySTD);

// end of individual settings

#define NUM_LEDS 	121			  // 11x11 LED, don't change
#define NUM_ROWS	 11
#define NUM_COLS	 11

#define EEPROM_SIZE 128			  // size of NV-memory

#define FPM_SLEEP_MAX_TIME 0xFFFFFFF
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#define CONSTRAIN(a, l, r)    (MIN(MAX((l), (a)), (r)))

// words for log output
static const char twords[][12] =
{ "", "Eins", "Zwei", "Drei", "Vier", "Fuenf", "Sechs", "Sieben", "Acht", "Neun", "Zehn", "Elf", "Zwoelf", "Ein",
		"Zwanzig", "vor", "nach", "um", "viertel", "halb", "dreiviertel", "Fuenf", "Zehn", "Es", "ist", "etwa", "genau",
		"Uhr" };

// time-word indices
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
 O	G	E	N	A	U	B	F	Ü	N	F		109 <- 099
 	 	 	 	 	 	 	 	 	 	 	 	 	 	|
 Z	E	H	N	Z	W	A	N	Z	I	G		088 -> 098
 	 	 	 	 	 	 	 	 	 	 	 	 |
 D	R	E	I	V	I	E	R	T	E	L		087 <- 077
 	 	 	 	 	 	 	 	 	 	 	 	 	 	|
 T	G	N	A	C	H	V	O	R	U	M		066 -> 076
 	 	 	 	 	 	 	 	 	 	 	 	 |
 H	A	L	B	Q	Z	W	Ö	L	F	P		065 <- 055
 	 	 	 	 	 	 	 	 	 	 	 	 	 	|
 Z 	W	E	I	N	S	I	E	B	E	N		044 -> 054
 	 	 	 	 	 	 	 	 	 	 	 	 |
 K	D	R	E	I	R	H 	F	Ü	N	F		043 <- 033
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
		{   0,   0 },	// placeholder
		{  46,  49 }, 	// EINS
		{  44,  47 }, 	// ZWEI
		{  39,  42 }, 	// DREI
		{  29,  32 }, 	// VIER
		{  33,  36 },	// FUENF
		{   1,   5 },	// SECHS
		{  49,  54 },	// SIEBEN
		{  17,  20 },	// ACHT
		{  25,  28 },	// NEUN
		{  13,  16 },	// ZEHN
		{  22,  24 },	// ELF
		{  56,  60 },	// ZWOELF
		{  46,  48 },	// EIN
		{  92,  98 },	// ZWANZIG_M
		{  72,  74 },	// VOR
		{  68,  71 },	// NACH
		{  75,  76 },	// UM
		{  77,  83 },	// VIERTEL
		{  62,  65 },	// HALB
		{  77,  87 },	// DREIVIERTEL
		{  99, 102 },	// FUENF_M
		{  88,  91 },	// ZEHN_M
		{ 110, 111 },	// ES
		{ 113, 115 },	// IST
		{ 117, 120 },	// ETWA
		{ 104, 108 },	// GENAU
		{   8,  10 } };	// UHR

// translation from minutes to word expressions (east and west german)
static const int CALC[2][12][4] =
// first word	second word third word		use following hours word
{
{// east german
{ 0, 			0,			_UM_, 			0 },
{ _FUENF_M_,	 _NACH_, 	0, 				0 },
{ _ZEHN_M_, 	_NACH_, 	0, 				0 },
{ 0, 			0, 			_VIERTEL_, 		1 },
{ _ZEHN_M_, 	_VOR_, 		_HALB_, 		1 },
{ _FUENF_M_, 	_VOR_, 		_HALB_, 		1 },
{ 0, 			0, 			_HALB_, 		1 },
{ _FUENF_M_, 	_NACH_, 	_HALB_, 		1 },
{ _ZEHN_M_, 	_NACH_, 	_HALB_, 		1 },
{ 0, 			0, 			_DREIVIERTEL_, 	1 },
{ _ZEHN_M_, 	_VOR_, 		0, 				1 },
{ _FUENF_M_,	_VOR_, 		0, 				1 } },
{// west german
{ 0, 			0, 			_UM_, 			0 },
{ _FUENF_M_, 	_NACH_, 	0, 				0 },
{ _ZEHN_M_, 	_NACH_, 	0, 				0 },
{ _VIERTEL_,	_NACH_, 	0, 				0 },
{ _ZWANZIG_M_,	_NACH_, 	0, 				0 },
{ _FUENF_M_, 	_VOR_, 		_HALB_, 		1 },
{ 0, 			0, 			_HALB_, 		1 },
{ _FUENF_M_, 	_NACH_, 	_HALB_, 		1 },
{ _ZWANZIG_M_, 	_VOR_, 		0, 				1 },
{ _VIERTEL_, 	_VOR_, 		0, 				1 },
{ _ZEHN_M_, 	_VOR_, 		0, 				1 },
{ _FUENF_M_, 	_VOR_, 		0, 				1 }
}
};

// structure holds the parameters to save nonvolatile
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

// float parameters for moodlight-mode
float mood_offset = 0.0, mood_step = 0.002;

// counter for time-measurement
static unsigned long amicros, umicros = 0;

char outstr[512] = "";

// LED-array for transfer to FastLED
CRGB leds[NUM_LEDS];

// WiFi-objects
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
WiFiManager wifiManager;

// the web-server-object
ESP8266WebServer server(80);   //instantiate server at port 80 (http port)

// string to hold the last displayed word-sequence
char tstr2[128] = "";

// own fmod() because of issue in the libm.a ( see https://github.com/esp8266/Arduino/issues/612 )
float xfmod(float numer, float denom)
{
	int ileft;

	if (!denom)
		return numer;

	ileft = numer / denom;
	return numer - (ileft * denom);
}

// dynamic generation of the web-servers response
void page_out(void)
{
	if(strlen(outstr))
	{
		server.sendContent(outstr);
		*outstr = 0;
	}
	else
	{
		server.sendContent(
				"<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html><head><meta http-equiv=\"content-type\"content=\"text/html; charset=ISO-8859-1\"><title>ESP8266_WordClock_Server</title></head>\r\n<body><h1 style=\"text-align: center; width: 504px;\" align=\"left\"><span style=\"color: rgb(0, 0, 153); font-weight: bold;\">ESP8266 WordClock-Server</span></h1><h3 style=\"text-align: center; width: 504px;\" align=\"left\"><span style=\"color: rgb(0, 0, 0); font-weight: bold;\">");
		server.sendContent(tstr2);
		server.sendContent(
				"</span></h3><form method=\"get\"><table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" height=\"144\" width=\"457\"><tbody><tr><td width=\"120\"><b><big>Anzeigemodus</big></b></td><td width=\"50\"></td><td><select name=\"MODE\"><option value=\"0\"");
		if (wordp->mode == 0)
			server.sendContent(" selected");
		server.sendContent(">Clock Ostdeutsch</option><option value=\"1\"");
		if (wordp->mode == 1)
			server.sendContent(" selected");
		server.sendContent(">Clock Westdeutsch</option><option value=\"2\"");
		if (wordp->mode == 2)
			server.sendContent(" selected");
		server.sendContent(">Moodlight diagonal</option><option value=\"3\"");
		if (wordp->mode == 3)
			server.sendContent(" selected");
		server.sendContent(
				">Moodlight fl&#228;chig</option></select></td></tr><tr><td><b><big>Pr&#228;zision</big></b></td><td></td><td><input name=\"PREC_ETWA\" type=\"Checkbox\" mode=\"submit\"");
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
				"\"> %</td></tr></tbody></table><br><table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" height=\"30\" width=\"99\"><tbody><tr><td><input name=\"SEND\" value=\"  Senden  \" type=\"submit\"></td></tr></tbody></table></form><table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" height=\"30\" width=\"99\"><tbody><tr><td><form method=\"get\"><input name=\"SAVE\" value=\"Speichern\" type=\"submit\"></form></td></tr></tbody></table></body></html>\r\n");
	}
}

// conversion from hsl- to RGB-color-scheme
void hsl_to_rgb(int hue, int sat, int lum, int* r, int* g, int* b)
{
	int v;

	v = (lum < 128) ? (lum * (256 + sat)) >> 8 : (((lum + sat) << 8) - lum * sat) >> 8;
	if (v <= 0)
	{
		*r = *g = *b = 0;
	}
	else
	{
		int m;
		int sextant;
		int fract, vsf, mid1, mid2;

		m = lum + lum - v;
		hue *= 6;
		sextant = hue >> 8;
		fract = hue - (sextant << 8);
		vsf = v * fract * (v - m) / v >> 8;
		mid1 = m + vsf;
		mid2 = v - vsf;
		switch (sextant)
		{
		case 0:
			*r = v;
			*g = mid1;
			*b = m;
			break;
		case 1:
			*r = mid2;
			*g = v;
			*b = m;
			break;
		case 2:
			*r = m;
			*g = v;
			*b = mid1;
			break;
		case 3:
			*r = m;
			*g = mid2;
			*b = v;
			break;
		case 4:
			*r = mid1;
			*g = m;
			*b = v;
			break;
		case 5:
			*r = v;
			*g = m;
			*b = mid2;
			break;
		}
	}
}

// find x/y-position on the screen of a given LED-index
int map_output_to_point(int output, int width, int height, int* x, int* y)
{
	int ret = -1, px, py;

	if (output < NUM_LEDS)
	{
		ret = 0;
		py = output / NUM_COLS;
		px = output % NUM_ROWS;
		if (py & 0x01)
			px = NUM_COLS - px - 1;

		*x = (int) CONSTRAIN((float )px * ((float )width) / (float )NUM_COLS, 0, width);
		*y = (int) CONSTRAIN((float )py * ((float )height) / (float )NUM_ROWS, 0, height);
	}
	else
	{
		*x = *y = -1;
	}

	return ret;
}

// generate next moodlight-color
void update_moodlight(void)
{
	int i;

	mood_offset = xfmod(mood_offset + mood_step, 1.0);

	switch (wordp->mode)
	{
		case 2:	// diagonal moodlight
		{
			for (i = 0; i < NUM_LEDS; i++)
			{
				int x, y, r, g, b;
				float f;

				// get x/y-position of this LED-index
				map_output_to_point(i, 1024, 1024, &x, &y);

				// calculate color for this position
				f = CONSTRAIN((x / 1024.0 + y / 1024.0) / 2.0, 0.0, 1.0);
				f = xfmod(f + mood_offset, 1.0);

				// convert calculated hsl-color to RGB
				hsl_to_rgb(255 * f, 255, 128, &r, &g, &b);

				// set color in wished brightness
				leds[i] = CRGB((r * (int) wordp->brightness) / 100, (g * (int) wordp->brightness) / 100,
						(b * (int) wordp->brightness) / 100);
			}
		}
		break;

		case 3:	// fullscreen-moodlight
		{
			int r, g, b;

			// calulate color for this loop
			hsl_to_rgb(255 * mood_offset, 255, 128, &r, &g, &b);

			// generate color-structure for wished display-color
			CRGB actcolor = CRGB(((int) wordp->r * (int) wordp->brightness) / 40,
					((int) wordp->g * (int) wordp->brightness) / 40, ((int) wordp->b * (int) wordp->brightness) / 40);

			// fill LED-field with calculated color
			for (i = 0; i < NUM_LEDS; i++)
			{
				leds[i] = actcolor;
			}
		}
		break;
	}
}

// system initalization
void setup()
{
	unsigned int i;
	unsigned char *eptr;

	pinMode(BUILTIN_LED, OUTPUT);   // Initialize the BUILTIN_LED pin as an output
	digitalWrite(BUILTIN_LED, HIGH);
	Serial.begin(115200);

	wifiManager.setTimeout(180);

	// initialize FastLED, all LEDs off
	FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
	memset(leds, 0, NUM_LEDS);
	FastLED.show();

	// read stored parameters
	EEPROM.begin(EEPROM_SIZE);
	eptr = (unsigned char*) wordp;
	for (i = 0; i < sizeof(wordclock_word_processor_struct); i++)
		*(eptr++) = EEPROM.read(i);

	// EEPROM-parameters invalid, use default-values and store them
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

		eptr = (unsigned char*) wordp;
		for (i = 0; i < sizeof(wordclock_word_processor_struct); i++)
			EEPROM.write(i, *(eptr++));
		EEPROM.commit();
	}

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

	// if you get here you have connected to the WiFi
	Serial.println("connected...yeey :)");

	// get time from internet
	timeClient.update();

	// parsing function for the commands of the web-server
	server.on("/", []()
	{
		int i;
		unsigned int j;
		long pval;

		*outstr = 0;

		if(server.args())
		{
			for(i = 0; i < server.args(); i++)
			{
				if(server.argName(i) == "SEND")
				{
					wordp->precise = 0;
					wordp->trailer = 0;
					*outstr = 0;
				}
			}
			for(i = 0; i < server.args(); i++)
			{
				errno = 0;
				if(server.argName(i) == "SAVE")
				{
					unsigned char *eptr = (unsigned char*)wordp;

					for(j = 0; j < sizeof(wordclock_word_processor_struct); j++)
						EEPROM.write(j, *(eptr++));
					EEPROM.commit();
					if(!server.arg(i).length())
						sprintf(outstr + strlen(outstr), "OK");
				}
				else if(server.argName(i) == "MODE")
				{
					pval = strtol(server.arg(i).c_str(), NULL, 10);
					if(server.arg(i).length() && !errno)
					{
						wordp->mode = pval;
						if(wordp->mode > 3)
							wordp->mode = 3;
					}
					else
						sprintf(outstr + strlen(outstr), "MODE=%d\r\n", wordp->mode);
				}
				else if(server.argName(i) == "BRIGHT")
				{
					pval = strtol(server.arg(i).c_str(), NULL, 10);
					if(server.arg(i).length() && !errno)
					{
						wordp->brightness = pval;
						if(wordp->brightness > 100)
						wordp->brightness = 100;
					}
					else
						sprintf(outstr + strlen(outstr), "BRIGHT=%d\r\n", wordp->brightness);
				}
				else if(server.argName(i) == "RED")
				{
					pval = strtol(server.arg(i).c_str(), NULL, 10);
					if(server.arg(i).length() && !errno)
					{
						wordp->r = pval;
						if(wordp->r > 100)
							wordp->r = 100;
					}
					else
						sprintf(outstr + strlen(outstr), "RED=%d\r\n", wordp->r);
				}
				else if(server.argName(i) == "GREEN")
				{
					pval = strtol(server.arg(i).c_str(), NULL, 10);
					if(server.arg(i).length() && !errno)
					{
						wordp->g = pval;
						if(wordp->g > 100)
						wordp->g = 100;
					}
					else
						sprintf(outstr + strlen(outstr), "GREEN=%d\r\n", wordp->g);
				}
				else if(server.argName(i) == "BLUE")
				{
					pval = strtol(server.arg(i).c_str(), NULL, 10);
					if(server.arg(i).length() && !errno)
					{
						wordp->b = pval;
						if(wordp->b > 100)
						wordp->b = 100;
					}
					else
						sprintf(outstr + strlen(outstr), "BLUE=%d\r\n", wordp->b);
				}
				else if(server.argName(i) == "PREC_ETWA")
				{
					if(server.arg(i) == "on")
						wordp->precise |= 0x02;
					else if(server.arg(i) == "off")
						wordp->precise &= ~0x02;
					else
						sprintf(outstr + strlen(outstr), "PREC_ETWA=%s\r\n", (wordp->precise &= ~0x02)?"on":"off");
				}
				else if(server.argName(i) == "PREC_GENAU")
				{
					if(server.arg(i) == "on")
						wordp->precise |= 0x01;
					else if(server.arg(i) == "off")
						wordp->precise &= ~0x01;
					else
						sprintf(outstr + strlen(outstr), "PREC_GENAU=%s\r\n", (wordp->precise &= ~0x01)?"on":"off");
				}
				else if(server.argName(i) == "TRAILER")
				{
					if(server.arg(i) == "on")
						wordp->trailer = 1;
					else if(server.arg(i) == "off")
						wordp->trailer = 0;
					else
						sprintf(outstr + strlen(outstr), "TRAILER=%s\r\n", (wordp->trailer)?"on":"off");
				}
			}
			*tstr2 = 0;
			update_outputs();
		}
		page_out();
	});
	// start web-server
	server.begin();
	Serial.println("Web server started!");
}


void update_outputs(void)
{
	int i, j, emin, min, hr;
	time_t rawtime, loctime, atime;
	int lvals[10];
	char tstr[128];

	if (wordp->mode < 2)								// wordclock-mode
	{
		if (timeClient.update())						// NTP-update
		{
			umicros = amicros;
			rawtime = timeClient.getEpochTime();		// get NTP-time
			loctime = myTZ.toLocal(rawtime);			// calc local time

			emin = minute(loctime);
			rawtime += 150;
			atime = myTZ.toLocal(rawtime);				// calc local time

			min = minute(atime) / 5;					// get minutes-index
			hr = (hour(atime) + CALC[wordp->mode][min][3]) % 12; // get hours-index
			if (!hr)
				hr = 12;

			j = 0;
			lvals[j++] = _ES_;							// first two words
			lvals[j++] = _IST_;
			if (emin % 5)								// minutes are not a multiple of 5?
			{
				if (wordp->precise & 0x02)				// add "ETWA" if activated
					lvals[j++] = _ETWA_;
			}
			else										// minutes are a multiple of 5?
			{
				if (wordp->precise & 0x01)				// add "GENAU" if activated
					lvals[j++] = _GENAU_;
			}

			// generate word indices according to minutes
			for (i = 0; i < 3; i++)
			{
				if (CALC[wordp->mode][min][i])
				{
					lvals[j++] = CALC[wordp->mode][min][i];
				}
			}

			// generate word index according to hour if trailer is activated use "EIN" instead of "EINS"
			lvals[j++] = (hr == 1) ? ((wordp->trailer) ? _EIN_ : _EINS_) : hr;

			//  add idex for "UHR" if trailer is activated
			if (wordp->trailer)
				lvals[j++] = _UHR_;

			lvals[j] = 0;								// end of words chain

			// generate color-structure for wished display-color
			CRGB actcolor = CRGB(((int) wordp->r * (int) wordp->brightness) / 40,
					((int) wordp->g * (int) wordp->brightness) / 40, ((int) wordp->b * (int) wordp->brightness) / 40);

			// clear LED-buffer
			memset(leds, 0, sizeof(leds));

			// create LED-field and text-string according to calculated word-indices
			*tstr = 0;
			for (i = 0; lvals[i]; i++)
			{
				for (j = tleds[lvals[i]][0]; j <= tleds[lvals[i]][1] && j < NUM_LEDS; j++)
				{
					leds[j] = actcolor;
				}
				sprintf(tstr + strlen(tstr), "%s ", twords[lvals[i]]);
			}

			// time-string changed since last loop?
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

		}
		delay(150);
	}
	else												// moodlight-mode
	{
		update_moodlight();
		delay(100);
	}

	FastLED.show();										// update LEDs
	*tstr = 0;
}

void loop()
{
	for (;;)
	{
		amicros = micros();
		update_outputs();
		server.handleClient();							// handle HTTP-requests
		if (((amicros - umicros) / 1000000L) > 3600)	// if no sync for more than one hour
			digitalWrite(BUILTIN_LED, HIGH);			// switch off BUILTIN_LED
	}
}

