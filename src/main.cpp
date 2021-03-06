/*
MIT License

Copyright (c) 2018 esp-rfid Community
Copyright (c) 2017 Ömer Şiar Baysal

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#define VERSION "1.4.2"

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TimeLib.h>
#include <Ticker.h>
#include "Ntp.h"
#include <AsyncMqttClient.h>
#include <Bounce2.h>
#include "magicnumbers.h"

 // #define DEBUG

int numRelays=1;

#ifdef OFFICIALBOARD

#include <Wiegand.h>

WIEGAND wg;
int relayPin[MAX_NUM_RELAYS] = {13};
bool activateRelay [MAX_NUM_RELAYS]= {false};
bool deactivateRelay [MAX_NUM_RELAYS]= {false};

#endif

#ifndef OFFICIALBOARD

#include <MFRC522.h>
#include "PN532.h"
#include <Wiegand.h>
#include "rfid125kHz.h"
#include <MCP23017.h>

MFRC522 mfrc522 = MFRC522();
PN532 pn532;
WIEGAND wg;
RFID_Reader RFIDr;

int rfidss;
int readertype;

// relay specific variables

int relayPin[MAX_NUM_RELAYS];
bool activateRelay [MAX_NUM_RELAYS]= {false,false,false,false,false,false};
bool deactivateRelay [MAX_NUM_RELAYS]= {false,false,false,false,false,false};
bool activateRelayOutput [MAX_NUM_RELAYS]= {false,false,false,false,false,false};
char *relayName[MAX_NUM_RELAYS] = {"1","2","3","4","5","6"};
#endif

int lockType[MAX_NUM_RELAYS];
int relayType[MAX_NUM_RELAYS];
unsigned long activateTime[MAX_NUM_RELAYS];


// these are from vendors
#include "webh/glyphicons-halflings-regular.woff.gz.h"
#include "webh/required.css.gz.h"
#include "webh/required.js.gz.h"

// these are from us which can be updated and changed
#include "webh/esprfid.js.gz.h"
#include "webh/esprfid.htm.gz.h"
#include "webh/index.html.gz.h"

#ifdef ESP8266
extern "C" {
	#include "user_interface.h"
}
#endif

NtpClient NTP;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
WiFiEventHandler wifiDisconnectHandler, wifiConnectHandler, wifiOnStationModeGotIPHandler;
Bounce openLockButton;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long blink_ = millis();
bool wifiFlag = false;
bool configMode = false;
int wmode;
uint8_t wifipin = 255;

uint8_t doorstatpin = 255;
uint8_t lastDoorState = 0;

// door stat specific variable for 6 doors
uint8_t doorstatusPinNew[MAX_NUM_RELAYS]= {255,255,255,255,255,255};
uint8_t lastDoorStatusNew[MAX_NUM_RELAYS]= {0,0,0,0,0,0};
uint8_t doorStatusNew[MAX_NUM_RELAYS] = {0,0,0,0,0,0};
uint8_t doorStatusFirstRun[MAX_NUM_RELAYS] = {0,0,0,0,0,0};
int doorStatusType[MAX_NUM_RELAYS];
bool waitForInvertDoorStatus[MAX_NUM_RELAYS]= {false,false,false,false,false,false};


uint8_t openlockpin = 255;

uint8_t doorbellpin = 255;
uint8_t lastDoorbellState = 0;

#define LEDoff HIGH
#define LEDon LOW

// IO Expander MCP23017
#define MCP23017_ADDR 0x20
MCP23017 mcp = MCP23017(MCP23017_ADDR);


/*
// OLED Display Setup
// Include the correct display library

// For a connection via I2C using the Arduino Wire include:
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"

// Initialize the OLED display using Arduino Wire:
SSD1306Wire display(0x3c, SDA, SCL);   // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h

#define DISPLAY_PAGE_DURATION 3000
typedef void (*Demo)(void);
*/
int demoMode = 0;
int counter = 1;
String StatusOLED1 = "";
String StatusOLED2 = "";
String StatusOLED3 = "";



// Variables for whole scope
const char *http_username = "admin";
char *http_pass = NULL;
unsigned long previousMillis = 0;
unsigned long previousLoopMillis = 0;
unsigned long currentMillis = 0;
unsigned long cooldown = 0;
unsigned long keyTimer = 0;
String currentInput = "";
unsigned long deltaTime = 0;
unsigned long uptime = 0;
bool shouldReboot = false;
bool inAPMode = false;
bool isWifiConnected = false;
unsigned long autoRestartIntervalSeconds = 0;

bool wifiDisabled = true;
bool doDisableWifi = false;
bool doEnableWifi = false;
bool timerequest = false;
bool formatreq = false;
unsigned long wifiTimeout = 0;
unsigned long wiFiUptimeMillis = 0;
char *deviceHostname = NULL;

int mqttenabled = 0;
char *mqttTopic = NULL;
char *mhs = NULL;
char *muser = NULL;
char *mpas = NULL;
int mport;

int timeZone;

unsigned long nextbeat = 0;

unsigned long interval 	= 180;  // Add to GUI & json config
bool mqttEvents 		= false; // Sends events over MQTT disables SPIFFS file logging


#include "log.esp"
#include "mqtt.esp"
#include "helpers.esp"
#include "wsResponses.esp"
#include "rfid.esp"
#include "wifi.esp"
#include "config.esp"
#include "websocket.esp"
#include "webserver.esp"
#include "door.esp"
#include "doorbell.esp"

//#include "oled.esp"

// ######################### OLED
// Screen dimensions
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128 // Change this to 96 for 1.27" OLED.

// You can use any (4 or) 5 pins 
#define SCLK_PIN 16 // D0//SCL //D5
#define MOSI_PIN 14 // D5//SDA //D7
#define DC_PIN   12 // D6
#define CS_PIN   15 // D8//D8
#define RST_PIN  13//-1 // D6

// Color definitions
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0  
#define WHITE           0xFFFF

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
//#include <SPI.h>
#include <Adafruit_I2CDevice.h>

// Option 1: use any pins but a little slower
// SSD1327
Adafruit_SSD1351 tft = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, CS_PIN, DC_PIN, MOSI_PIN, SCLK_PIN, RST_PIN);

const long intervalBlink = 1000;           // interval at which to blink (milliseconds)
bool TonDisplayState = false;
unsigned long previousMillisDisplay = 0;

#include <oled-ssd1331-functions.esp>
// ######################### OLED



void ICACHE_FLASH_ATTR setup(void)
{

/*
	#ifdef DEBUG
		Serial.println(F("[ WARN ] Factory reset initiated..."));
#endif
		SPIFFS.end();
		ws.enable(false);
		SPIFFS.format();
		ESP.restart();
*/

#ifdef OFFICIALBOARD
	// Set relay pin to LOW signal as early as possible
	pinMode(13, OUTPUT);
	digitalWrite(13, LOW);
	delay(200);
#endif

#ifdef DEBUG
	Serial.begin(9600);
	Serial.println();

	Serial.print(F("[ INFO ] ESP RFID v"));
	Serial.println(VERSION);

	uint32_t realSize = ESP.getFlashChipRealSize();
	uint32_t ideSize = ESP.getFlashChipSize();
	FlashMode_t ideMode = ESP.getFlashChipMode();
	Serial.printf("Flash real id:   %08X\n", ESP.getFlashChipId());
	Serial.printf("Flash real size: %u\n\n", realSize);
	Serial.printf("Flash ide  size: %u\n", ideSize);
	Serial.printf("Flash ide speed: %u\n", ESP.getFlashChipSpeed());
	Serial.printf("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
	if (ideSize != realSize)
	{
		Serial.println("Flash Chip configuration wrong!\n");
	}
	else
	{
		Serial.println("Flash Chip configuration ok.\n");
	}
#endif

	if (!SPIFFS.begin())
	{
#ifdef DEBUG
		Serial.print(F("[ WARN ] Formatting filesystem..."));
#endif
		if (SPIFFS.format())
		{
			writeEvent("WARN", "sys", "Filesystem formatted", "");

#ifdef DEBUG
			Serial.println(F(" completed!"));
#endif
		}
		else
		{
#ifdef DEBUG
			Serial.println(F(" failed!"));
			Serial.println(F("[ WARN ] Could not format filesystem!"));
#endif
		}
	}
	wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
	wifiConnectHandler = WiFi.onStationModeConnected(onWifiConnect);
	wifiOnStationModeGotIPHandler = WiFi.onStationModeGotIP(onWifiGotIP);
	
	configMode = loadConfiguration();
	if (!configMode)
	{
		fallbacktoAPMode();
		configMode = false;
	}
	else {
		configMode = true;
	}
	setupWebServer();
	writeEvent("INFO", "sys", "System setup completed, running", "");


	// Setup Wire MCP IO-Expander
	writeEvent("INFO", "sys", "loading MCP23017 IO-Expander", "");
	Wire.begin(SDA,SCL); //SDA = GPIO4 / SCL = GPIO5
    mcp.init();
	mcp.portMode(MCP23017Port::A, 0b11111111);          //Port A as input rechts
    mcp.portMode(MCP23017Port::B, 0); 					//Port B as output links
    mcp.writeRegister(MCP23017Register::IPOL_A, 0x00); 	// Reset Port A
    mcp.writeRegister(MCP23017Register::IPOL_B, 0x00);	// Reset Port B






/*
	// Initialising the UI will init the display too.
	display.init();

	display.flipScreenVertically();
	display.setFont(ArialMT_Plain_10);
	
*/
	// INIT OLED
  tft.begin();
  tft.fillRect(0, 0, 128, 128, BLACK);
  delay(100);

   lcdTestPattern();
  delay(500);
  tft.fillScreen(BLACK);


}

void ICACHE_RAM_ATTR loop()

{

	currentMillis = millis();
	deltaTime = currentMillis - previousLoopMillis;
	uptime = NTP.getUptimeSec();
	previousLoopMillis = currentMillis;

	openLockButton.update();
	if (openLockButton.fell())
	{
#ifdef DEBUG
		Serial.println("Button has been pressed");
#endif
		writeLatest("", "(used open/close button)", 1);
		activateRelay[0] = true;
	}

	if (wifipin != 255 && configMode && !wmode)
	{
		if (!wifiFlag)
		{
			if ((currentMillis - blink_) > 500)
			{
				blink_ = currentMillis;
				digitalWrite(wifipin, !digitalRead(wifipin));
			}
		}
		else
		{
			if (!(digitalRead(wifipin)==LEDon)) digitalWrite(wifipin, LEDon);
		}
	}

	if (doorstatusPinNew[0] != 255)
	{
		doorStatusFunction();
		delayMicroseconds(500);
	}
	else if (doorstatpin != 255)
	{
		doorStatus();
		delayMicroseconds(500);
	}
	

	if (doorbellpin != 255)
	{
		doorbellStatus();
		delayMicroseconds(500);
	}


// OLED LOOP
	//oled();
	mainpage();
	delayMicroseconds(500);


	if (currentMillis >= cooldown)
	{
		rfidloop();
	}

	// Save Relay Output for Display
	for (int currentRelay = 0; currentRelay < numRelays ; currentRelay++){
		if (activateRelay[currentRelay]) {
			activateRelayOutput[currentRelay] = true;
#ifdef DEBUG
			Serial.printf("Output activating relay %d now\n",currentRelay);
#endif
		}
	
	}

	// Continuous relay mode

	for (int currentRelay = 0; currentRelay < numRelays ; currentRelay++){
	  	if (lockType[currentRelay] == LOCKTYPE_CONTINUOUS)
		{
			if (activateRelay[currentRelay])
			{
				// currently OFF, need to switch ON
				if (digitalRead(relayPin[currentRelay]) == !relayType[currentRelay]) // Todo mit MCP ergänzen für continous
				{
#ifdef DEBUG
					Serial.print("mili : ");
					Serial.println(millis());
					Serial.printf("activating relay %d now\n",currentRelay);
					Serial.printf("relayPIN: %d \n",relayPin[currentRelay]);
#endif
					if (relayPin[currentRelay] < MCPPORT_IO)
					{
						digitalWrite(relayPin[currentRelay], relayType[currentRelay]);
					}
					else
					{
						#ifdef DEBUG
							Serial.printf("MCP23017 PORT: %d \n",relayPin[currentRelay]-MCPPORT_IO);
						#endif	
						mcp.digitalWrite(relayPin[currentRelay]- MCPPORT_IO,relayType[currentRelay]);
					}
	
				}
				else	// currently ON, need to switch OFF
				{
#ifdef DEBUG
					Serial.print("mili : ");
					Serial.println(millis());
					Serial.printf("deactivating relay %d now\n",currentRelay);
					Serial.printf("relayPIN: %d \n",relayPin[currentRelay]);
	#endif
					if (relayPin[currentRelay] < MCPPORT_IO)
					{
						digitalWrite(relayPin[currentRelay], !relayType[currentRelay]);
					}
					else
					{
#ifdef DEBUG
						Serial.printf("MCP23017 PORT: %d \n",relayPin[currentRelay]-MCPPORT_IO);
#endif	
						mcp.digitalWrite(relayPin[currentRelay]- MCPPORT_IO, !relayType[currentRelay]);
					}
				}
				activateRelay[currentRelay] = false;

				activateRelayOutput[currentRelay] = false;
#ifdef DEBUG			
			Serial.printf("Output deactivate relay %d now\n",currentRelay);
#endif
			}
		}
		// Momentary relay mode
		else if (lockType[currentRelay] == LOCKTYPE_MOMENTARY)	// momentary relay mode
		{
			if (activateRelay[currentRelay])
			{
#ifdef DEBUG
				Serial.print("mili : ");
				Serial.println(millis());
				Serial.printf("activating relay %d now\n",currentRelay);
				Serial.printf("relayPIN: %d \n",relayPin[currentRelay]);
#endif
				if (relayPin[currentRelay] < MCPPORT_IO)
				{
					digitalWrite(relayPin[currentRelay], relayType[currentRelay]);
				}
				else
				{
#ifdef DEBUG
					Serial.printf("MCP23017 PORT: %d \n",relayPin[currentRelay]-MCPPORT_IO);
#endif	
					mcp.digitalWrite(relayPin[currentRelay]- MCPPORT_IO, relayType[currentRelay]);
				}
				previousMillis = millis();
				activateRelay[currentRelay] = false;
				deactivateRelay[currentRelay] = true;
			}
			else if ((currentMillis - previousMillis >= activateTime[currentRelay]) && (deactivateRelay[currentRelay]))
			{
#ifdef DEBUG
				Serial.println(currentMillis);
				Serial.println(previousMillis);
				Serial.println(activateTime[currentRelay]);
				Serial.println(activateRelay[currentRelay]);
				Serial.println("deactivate relay after this");
				Serial.print("mili : ");
				Serial.println(millis());
#endif
				if (relayPin[currentRelay] < MCPPORT_IO)
				{
					digitalWrite(relayPin[currentRelay], !relayType[currentRelay]);
				}
				else
				{
#ifdef DEBUG
					Serial.printf("MCP23017 PORT: %d \n",relayPin[currentRelay]-MCPPORT_IO);
#endif	
					mcp.digitalWrite(relayPin[currentRelay]- MCPPORT_IO, !relayType[currentRelay]);
				}
				deactivateRelay[currentRelay] = false;

				activateRelayOutput[currentRelay] = false;
#ifdef DEBUG			
				Serial.printf("Output deactivate relay %d now\n",currentRelay);
#endif
			}
		}
		// Doorstatus relay mode
		else if (lockType[currentRelay] == LOCKTYPE_DOORSTATUS)	// Doorstatus relay mode
		{
			if (activateRelay[currentRelay])
			{
#ifdef DEBUG
				Serial.print("mili : ");
				Serial.println(millis());
				Serial.printf("activating relay %d now\n",currentRelay);
				Serial.printf("relayPIN: %d \n",relayPin[currentRelay]);
#endif
				if (relayPin[currentRelay] < MCPPORT_IO)
				{
					digitalWrite(relayPin[currentRelay], relayType[currentRelay]);
				}
				else
				{
	#ifdef DEBUG
					Serial.printf("MCP23017 PORT: %d \n",relayPin[currentRelay]-MCPPORT_IO);
	#endif	
					mcp.digitalWrite(relayPin[currentRelay]- MCPPORT_IO, relayType[currentRelay]);
				}
				
				activateRelay[currentRelay] = false;
				waitForInvertDoorStatus[currentRelay] = true;
				Serial.println("Wait for DoorStatus Invert");
				StatusOLED1 = "Tür auf: " + String(currentRelay)+1;
				
			}
			else if (doorStatusNew[currentRelay] == doorStatusType[currentRelay] && waitForInvertDoorStatus[currentRelay])
			{
				previousMillis = millis();
								
				waitForInvertDoorStatus[currentRelay] = false;
				deactivateRelay[currentRelay] = true;
				
			}	
			else if ((currentMillis - previousMillis >= activateTime[currentRelay]) && (deactivateRelay[currentRelay]))
			{
	#ifdef DEBUG
				Serial.println(currentMillis);
				Serial.println(previousMillis);
				Serial.println(activateTime[currentRelay]);
				Serial.println(activateRelay[currentRelay]);
				Serial.println("deactivate relay after this");
				Serial.print("mili : ");
				Serial.println(millis());
	#endif
				if (relayPin[currentRelay] < MCPPORT_IO)
				{
					digitalWrite(relayPin[currentRelay], !relayType[currentRelay]);
				}
				else
				{
	#ifdef DEBUG
					Serial.printf("MCP23017 PORT: %d \n",relayPin[currentRelay]-MCPPORT_IO);
	#endif	
					mcp.digitalWrite(relayPin[currentRelay]- MCPPORT_IO, !relayType[currentRelay]);
				}
				deactivateRelay[currentRelay] = false;

				activateRelayOutput[currentRelay] = false;
#ifdef DEBUG			
				Serial.printf("Output deactivate relay %d now\n",currentRelay);
#endif

			}
		}
	}

	if (formatreq)
	{
#ifdef DEBUG
		Serial.println(F("[ WARN ] Factory reset initiated..."));
#endif
		SPIFFS.end();
		ws.enable(false);
		SPIFFS.format();
		ESP.restart();
	}

	if (timerequest)
	{
		timerequest = false;
		sendTime();
	}

	if (autoRestartIntervalSeconds > 0 && uptime > autoRestartIntervalSeconds * 1000)
	{
		writeEvent("INFO", "sys", "System is going to reboot", "");
#ifdef DEBUG
		Serial.println(F("[ WARN ] Auto restarting..."));
#endif
		shouldReboot = true;
	}

	if (shouldReboot)
	{
		writeEvent("INFO", "sys", "System is going to reboot", "");
#ifdef DEBUG
		Serial.println(F("[ INFO ] Rebooting..."));
#endif
		ESP.restart();
	}

	if (isWifiConnected)
	{
		wiFiUptimeMillis += deltaTime;
	}

	if (wifiTimeout > 0 && wiFiUptimeMillis > (wifiTimeout * 1000) && isWifiConnected == true)
	{
		writeEvent("INFO", "wifi", "WiFi is going to be disabled", "");
		doDisableWifi = true;
	}

	if (doDisableWifi == true)
	{
		doDisableWifi = false;
		wiFiUptimeMillis = 0;
		disableWifi();
	}
	else if (doEnableWifi == true)
	{
		writeEvent("INFO", "wifi", "Enabling WiFi", "");
		doEnableWifi = false;
		if (!isWifiConnected)
		{
			wiFiUptimeMillis = 0;
			enableWifi();
		}
	}

	if (mqttenabled == 1)
	{
		if (mqttClient.connected())
		{
			if ((unsigned)now() > nextbeat)
			{
				mqtt_publish_heartbeat(now());
				nextbeat = (unsigned)now() + interval;
#ifdef DEBUG
				Serial.print("[ INFO ] Nextbeat=");
				Serial.println(nextbeat);
#endif
			}
		}
	}
	// Keep an eye on timeout waiting for keypress
	// Clear code and timer when timeout is reached
	if (keyTimer > 0 && millis() - keyTimer >= KEYBOARD_TIMEOUT_MILIS)
	{
#ifdef DEBUG
		Serial.println("[ INFO ] Keycode timeout");
#endif
		keyTimer = 0;
		currentInput = "";
	}

}
