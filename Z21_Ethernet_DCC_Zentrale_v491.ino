/**************************************************************
 * Z21 Ethernet DCC Command Station
 * Copyright (c) 2015-2021 by Philipp Gahtow
***************************************************************
Unterstützte Funktionen/Protokolle:
 * NMRA DCC output (with Railcom and POM)
 * Z21 Ethernet over LAN and/or WLAN
 * S88N feedback
 * XpressNet with AUTO switch MASTER-SLAVE-MODE
 * LocoNet with MASTER-MODE (Slotserver) or SLAVE-MODE
 * NMRA DCC Input (legacy)
 * support for ATmega 328/644/1280/1284/2560 (UNO, MEGA, Sanguino)
 * support for ARM (DUE) (in parts only)
 * --> a use of all functions/interfaces is only awayable on Arduino MEGA!!!

This is a simple dcc command station that receive commands via Ethernet, XpressNet or LocoNet.
It base on the Z21 ethernet protocol of ROCO!

***************************************************************

- DCC Master Interface with Timer 2 by modifired CmdrArduino library by Philipp Gahtow
- Z21 LAN Protokoll mit W5100 Ethernet Shield with z21.h library
- LAN HTTP Website on Port 80 to configure ethernet IP and S88 bus length
- ESP8266 WiFi Z21 LAN Untersützung with z21.h library
- fast S88N feedback
- legacy:(DCC input, to read signal from other Central Station via Interrupt 0 and Timer 4)
- LocoNet at MEGA with Timer 5, normal Timer1 with Loconet.h library
- XpressNet (RS485) via LOOP-Function with XpressNetMaster.h library
- Relais for seperate program track
- Global Railcom Detector for MEGA on Serial3
- continus DCC for S88N and LocoNet-B
- support external LocoNet Booster over LocoNet-B Line

***************************************************************

Softwareversion: */
#define Z21mobileSwVer 491
/*
---------------------------------------------------------------
changes:
15.04.2015  Abschaltung S88 Interface per Define (S88N)
16.04.2015  Aktualisierung Z21 LAN Protokoll V1.05 & Firmware-Version 1.26
17.04.2015  LN OPC_INPUT_REP msg von Belegmeldern über LAN_LOCONET_DETECTOR
20.04.2015  kurze/Lange DCC Adressen (1..99 kurz, ab 100 lang)
22.04.2015  Add in DCC Lib Function support F13 - F28
            Add Power Button with Reset (press at startup)
23.04.2015  Add LocoNet set Railpower (OPC_GPOFF, OPC_GPON, OPC_IDLE)
            Add LocoNet Slot Write (OPC_WR_SL_DATA)
            New Broadcast Msg (8 Bit) Z21 Protokoll V1.05 (Include LocoNet)
            Add LocoNet OPC_RQ_SL_DATA, OPC_UHLI_FUN, OPC_SW_REQ, OPC_SW_REP, OPC_SW_ACK, OPC_SW_STATE
28.04.2015  Add DCC CV Write and Decoder Reset Packet before CV-Programming            
04.07.2015  Add Support Sanguino (ATmega644p and ATmega1284p) =>MCU_config.h
10.07.2015  Change Timer for DCC Interface and S88 to support LocoNet for all MCU
            Add second Booster support (intenal/external)
21.07.2015  S88 max Module define hinzu und S88 HTTP Ausgabe angepasst
30.07.2015  Versionsnummer für Debug definiert
02.08.2015  DCC Accessory Befehl korrigiert
            PowerButton Input geändert von Pin 51 nach Pin 47
03.08.2015  DCC Decoder Funktion korrigiert
17.09.2015  S88 Timer Auswahl (MEGA = Timer3)
18.09.2015  ESP8266 WiFi Support; Z21 LAN über externe Library
23.09.2015  Überarbeitung LAN_LOCONET_DETECTOR
            Neues Kommando OPC_MULTI_SENSE
            DCC Dekoder ohne Timer4!
            Optionale Lok-Event-Informationen im LocoNet (reduzierung der Sendedaten)
03.10.2015  S88 verbessert -> Fehler in der S88 Modulanzahl korrigiert (Überlauf der Zählervariale)       
            LocoNet TX/RX Packetverarbeitung verbessert  
04.10.2015  ROCO EXT Booster Short mit Transistor (invertiert!) 
            Optimierung S88 Timer (Rechenoperationen und Seicherbedarf)              
10.10.2015  Anzeigen Reset Zentrale mittels binkenden LEDs   
13.10.2015  Rückmelder über LocoNet
            Senden von DCC Weichenschaltmeldungen auch über LocoNet         
            LAN Webseite angepasst für Smartphone Display
14.10.2015  Einstellung der Anzahl von S88 Modulen über WiFi
            Verbesserung der Kommunikation mit dem ESP    
04.11.2015  LocoNet Master- oder Slave-Mode auswählbar
19.12.2015  Support kombinierte UDP Paket für WLAN und LAN            
26.12.2015  Add Track-Power-Off after Service Mode 
20.02.2016  Speicherreduzierung wenn kein WLAN und LAN genutzt wird
            LocoNet Client Modus Kommunikation mit IB verbessert
            Extra Serial Debug Option für XpressNet
27.02.2016  Änderung Dekodierung DCC14 und DCC28
            Invertierung Fahrtrichtung DCC Decoder DIRF            
            LocoNet Slave-Mode ignoriere Steuerbefehle, wenn Slot = 0
02.06.2016  Baud für Debug und WiFi einstellbar
            Software Serial für WiFi wählbar (zB. für Arduino UNO)
            -> WiFi Modul Firmware ab v2.5
17.07.2016 Fix Network UDP Antwortport - Sende Pakete an Quellport zurück
25.07.2016 add busy message for XpressNet (MultiMaus update screen now)
Aug.2016   add Railcom Support and update DCCInterfaceMaster and Booster Hardware,
           support POM read over I2C with external MCU (GLOBALDETECTOR)
26.08.2016 add DHCP for Ethernet Shield      
21.11.2016 DCC: fix Railcom - still Problem with Startup: Analog-Power on the rails - Hardware change needed!
26.11.2016 LocoNet: add Uhlenbrock Intellibox-II F13 to F28 support
27.11.2016 LocoNet: fix Speed DIR in OPC_SL_RD_DATA in data byte to 0x80 = B10000000 and OPC_LOCO_DIRF remove invert
27.12.2016 Z21 add CV lesen am Programmiergleis
01.02.2017 add negative DCC output option and seperate this feature from RAILCOM
15.03.2017 fix narrowing conversation inside LNInterface.h
28.03.2017 external Booster active in ServiceMode when no internal Booster
24.04.2017 fix data lost on loconet - s88 timer3 block packets - deactivated
28.04.2017 add MultiMaus support for F13 to F20 without fast flashing
10.05.2017 add XpressNet information for loco speed and function and switch position change
11.05.2017 add internal Booster Short Detection over Current Sence Resistor
25.05.2017 add RailCom Global Reader for Arduino MEGA on Serial3 (POM CV read only)
19.06.2017 fix problems with Arduino UNO compiling
09.07.2017 fix problems when using without XpressNet
23.07.2017 add support for Arduino DUE
26.08.2017 add default speed step setting
09.01.2018 add POM Bit write
21.01.2018 optimize LocoNet Slot system - reduce RAM use
18.08.2018 add support for Arduino DUE XpressNet
02.11.2018 adjust Z21 WiFi data communication and rise up baud rate
22.11.2018 add support for Arduino ESP8266 (WiFi, DCC extern and intern without seperate prog track)
09.06.2019 add extra DCC-Output for S88 and LocoNet without Power-OFF and RailCom
12.04.2020 adjust problems with the Serial communication to ESP8266 WiFi
22.06.2020 remove DUE XpressNet statememts
28.07.2020 change Input Pins VoltInPin and TempPin set only to INPUT Mode.
29.07.2020 add ENC28J60 module - instead of w5100 Shield for MEGA only.
30.07.2020 central startup Railpower sync for all interfaces      
04.08.2020 fix size of data type "RailcomCVAdr"
24.10.2020 fix error inside LocoNetInterface with Master-Mode
29.10.2020 reduce timeout for LAN DHCP challange
30.10.2020 add Z21 Interface config file with template 
04.12.2020 fix LocoNet Client Mode, don't answer on Master commands!
           remove Ethernet LAN DHCP timeout when using UIP Library
05.01.2021 fix WiFi RX problem with long UDP Packet and zero Packets (LAN_LOCONET_DETECTOR)
           fix wrong Adr in LocoNet Slot data response, when doing a DISPATCH Adr > 127
06.01.2021 add new AREF with 1.1 Volt
07.07.2021 fix EEPROM Problem with ESP32 and ESP8266 when doing a commit
08.01.2021 use ESP DNS library only for ESP32 and set a seperate one for ESP8266
02.03.2021 fix Ethernet S88 Module change if DHCP is on
03.03.2021 fix big UDP ethernet receive when package is empty
17.03.2021 fix error with inactiv Debug on HTTP reading S88Module at Debug.print
           change EEPROM storage for ESP
           add firmware to EEPROM and full EEPROM reset when no firmware is found or when POWER pressed on start
           Serialnumber in Z21-APP show now the firmware version
06.04.2021 fix report LocoNet sensor messages to Z21-APP for display
18.05.2021 add LocoNet2 library to support LocoNet on ESP32
20.05.2021 fix some mistakes in LocoNet packet working
------------------------------------

toDo:
-> Rückmelder via XpressNet
-> Programmieren von CVs im LocoNet -> works with Z21mobile APP over LocoNet Tunnel!
*/

//**************************************************************
//Individual Z21 Configuration:*/
#include "config.h"   //own Z21 configurationfile

//**************************************************************
//Firmware store in EEPROM:
#define EEPROMSwVerMSB 0
#define EEPROMSwVerLSB 1

//**************************************************************
//Setup up PIN-Configuration for different MCU (UNO/MEGA/Sanduino)
#include "MCU_config.h"

/**************************************************************
Serial Debug Output:*/
#ifdef DebugBaud
  #ifndef Debug
    #define Debug Serial  //Interface for Debugging
  #endif
#endif

/**************************************************************
ESP OTA Upload:*/
#if defined(ESP_OTA)
#include <ArduinoOTA.h>
#include "ESP_OTA.h"
#endif

/**************************************************************
XpressNet Library:*/
#ifdef XPRESSNET
#include <XpressNetMaster.h>
#endif

/**************************************************************
LocoNet Library:*/
#ifdef LOCONET
#if defined(ESP32_MCU)  //use LocoNet2 Library
#include "LNSerial.h"
LocoNetBus bus;
#include <LocoNetESP32.h>
LocoNetESP32 locoNetPhy(&bus, LNRxPin, LNTxPin, 0);
LocoNetDispatcher parser(&bus);
#else
#include <LocoNet.h>
#endif

/* default: keine Lok-Ereignisse ins LocoNet senden (Master-Mode only!) */
#ifndef TXAllLokInfoOnLN
#define TXAllLokInfoOnLN false
#endif

#endif

/**************************************************************
BOOSTER EXT:*/
#ifdef BOOSTER_EXT
#define BOOSTER_EXT_ON HIGH
#define BOOSTER_EXT_OFF LOW
#endif

/**************************************************************
BOOSTER INT:*/
#ifdef BOOSTER_INT
//#define BOOSTER_INT_ON LOW    //only for old Mode without RAILCOM support over NDCC!
//#define BOOSTER_INT_OFF HIGH  //only for old Mode without RAILCOM support over NDCC!
#define BOOSTER_INT_NDCC    //for new RAILCOM Booster3R - GoIntPin activate inverted booster signal
#endif

/**************************************************************
DCC Master to create a DCC Signal:*/
#include <DCCPacketScheduler.h>   //DCC Interface Library

/* Set default SwitchFormat for accessory use */
#ifndef SwitchFormat
#define SwitchFormat ROCO
#endif

//**************************************************************
#if defined(Z21VIRTUAL)  
#include <SoftwareSerial.h>
SoftwareSerial SoftSerial(TXvWiFi, RXvWiFi); // init Soft Serial
#undef LAN    //LAN nicht zulassen - Doppelbelegung der Signalleitungen!
#undef HTTPCONF
#undef LOCONET
#endif

//**************************************************************
#if defined(LAN)      //W5100 LAN Interface Library

#if !defined(MEGA_MCU)
#undef ENC28
#endif

#if defined(ENC28)  //MEGA MCU only!
#include <UIPEthernet.h>
#else					 																	   
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library
#endif
#endif

#if defined(ESP_WIFI)   //ESP8266 or ESP32
#if defined(ESP8266_MCU)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266mDNS.h>  //OTA update
#elif defined(ESP32_MCU)  
  #include <WiFi.h>
  #include <WiFiAP.h>   //optional?
  #include <ESPmDNS.h>  //ESP32 only, OTA update
#endif  
#include <WiFiClient.h> 
#include <WiFiUDP.h>
#endif

//**************************************************************
//Configure the analog 1.1V refenence voltage:
#ifndef AREF_1V1
  #if defined(EXTERNAL_UREF_1V1)
  #define AREF_1V1 EXTERNAL   //externe 1.1 Volt
  #elif defined(MEGA_MCU)   
  #define AREF_1V1 INTERNAL1V1  //Arduino MEGA internal 1,1 Volt
  #elif defined(UNO_MCU)  
  #define AREF_1V1 INTERNAL   //Arduino UNO (equal to 1.1 volts on the ATmega168 or ATmega328P)
  #elif defined(ESP32_MCU) | defined(ESP8266_MCU)   //ESP Modul
  #define AREF_1V1      //always 1,1 Volt
  #endif
  //other Arduino's 5,0 Volt internal refence!
#endif

//**************************************************************
static void globalPower (byte state);

//Z21 LAN Protokoll:
#if defined(LAN) || defined (WIFI) || defined(ESP_WIFI)
#include <z21.h> 
z21Class z21;
#endif

//**************************************************************
#if defined(DUE_MCU)
#include <DueFlashStorage.h>
DueFlashStorage Flash;
#define FIXSTORAGE Flash
#define FIXMODE write
#else
#include <EEPROM.h>   //EEPROM - to store number of S88 Module and LAN IP
#define FIXSTORAGE EEPROM
  #if defined(ESP8266_MCU) || defined(ESP32_MCU)
  #define FIXMODE write
  #else
  #define FIXMODE update
  #endif
#endif

#if defined(ESP8266_MCU) || defined(ESP32_MCU)
//EEPROM Konfiguration
#define EESize 512    //Größe des EEPROM
#define EEStringMaxSize 40   //Länge String im EEPROM
//Client:
#define EEssidLength 200       //Länge der SSID
#define EEssidBegin 201        //Start Wert
#define EEpassLength 232        //Länge des Passwort
#define EEpassBegin 233        //Start Wert
//AP:
#define EEssidAPLength 264       //Länge der SSID AP
#define EEssidAPBegin 265        //Start Wert
#define EEpassAPLength 298        //Länge des Passwort AP
#define EEpassAPBegin 300        //Start Wert
#define EEkanalAP 299          //Kanal AP
#endif

#define EES88Moduls 38  //Adresse EEPROM Anzahl der Module für S88
#define EEip 40    //Startddress im EEPROM für die IP

//---------------------------------------------------------------
#if defined(LAN)  //W5100 LAN Udp Setup:
EthernetUDP Udp;    //UDP for communication with APP/Computer (Port 21105)
//EthernetUDP UdpMT;  //UDP to Z21 Maintenance Tool (Port 34472)
//---------------------------------------------------------------
// The IP address will be dependent on your local network:
IPAddress ip(192, 168, 0, 111);   //Werkseinstellung ist: 192.168.0.111
/*LAN MAC configuration: */
#ifndef LANmacB2
#define LANmacB2 0xEF
#endif
#ifndef LANmacB1
#define LANmacB1 0xFE
#endif
#ifndef LANmacB0
#define LANmacB0 0xED
#endif
static byte mac[] = { 0x84, 0x2B, 0xBC, LANmacB2, LANmacB1, LANmacB0 };
/*LAN DHCP configuration: */
#ifndef LANTimeoutDHCP
#define LANTimeoutDHCP 5000 //Timeout to wait for a DHCP respose
#endif
#if defined(HTTPCONF) //W5100 LAN config Website:
EthernetServer server(80);  // (port 80 is default for HTTP):
#endif
#endif    //LAN end
//---------------------------------------------------------------
#if defined(ESP_WIFI)
#ifndef ESP_HTTPCONF
#define ESP_HTTPCONF
#endif
#ifndef SddidAP
#define SssidAP "Z21_ESP_Central"   // Default Z21 AP (SSID)
#endif
#ifndef SpassAP
#define SpassAP "12345678"  // Default Z21 network password
#endif
#ifndef SkanalAP
#define SkanalAP 3          // Default Kanal des AP
#endif

WiFiUDP Udp;
#if defined(ESP_HTTPCONF) //Setting Website
WiFiServer ESPWebserver(80);  //default port 80 for HTTP
#endif
#endif
//--------------------------------------------------------------
//Z21 Protokoll Typ Spezifikationen
#if defined(LAN) || defined (WIFI) || defined(ESP_WIFI)
#include "Z21type.h"    //Z21 Data Information
#endif

//--------------------------------------------------------------
//S88 Singel Bus:
#if defined(S88N)
#include "S88.h"
#endif

//--------------------------------------------------------------
//Dallas Temperatur Sensor:
#if defined(DALLASTEMPSENSE) && defined(MEGA_MCU)
#include <OneWire.h>
#include <DallasTemperature.h>
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
// arrays to hold device address
DeviceAddress insideThermometer;
#define DALLAS_RESOLUTION 9  //Temperatur resolution
#endif

//--------------------------------------------------------------
//DCC Interface Master Short Detection:
//EXTERNAL BOOSTER:
#define DetectShortCircuit 0x1FF    //to detect short circuit  (0xFF)
unsigned int ShortTime = 0;            //Time Count for Short Detect
unsigned long LEDcount = 0;    //Timer for Status LED
//INTERNAL BOOSTER:
#if defined(BOOSTER_INT_MAINCURRENT)
#define BOOSTER_INT_CURRENT_SHORT_DETECT //wenn nicht aktiviert dann hier einschalten!
byte ShortTimeINT = 0;      //Time Count for internal short detect
#ifndef DETECT_SHORT_INT_WAIT
#define DETECT_SHORT_INT_WAIT 10 //Time after internal short circuit is detected
#endif
#ifndef DETECT_SHORT_INT_VALUE
#if defined(AREF_1V1)
#define DETECT_SHORT_INT_VALUE  1000  //analogRead value for "mA" that is too much (AREF = 1.1V)
#else
#define DETECT_SHORT_INT_VALUE  400  //analogRead value for "mA" that is too much (AREF = 5.0V)
#endif
#endif
#endif

//--------------------------------------------------------------
DCCPacketScheduler dcc;
#define DCC     //activate DCC Interface

//--------------------------------------------------------------
#if defined(DCCGLOBALDETECTOR) && defined(DCC)
#include "Z21_RailCom.h"
#endif

//--------------------------------------------------------------
#if defined(XPRESSNET)
XpressNetMasterClass XpressNet;
#endif

//--------------------------------------------------------------
// certain global XPressnet status indicators
#define csNormal 0x00 // Normal Operation Resumed ist eingeschaltet
#define csEmergencyStop 0x01 // Der Nothalt ist eingeschaltet
#define csTrackVoltageOff 0x02 // Die Gleisspannung ist abgeschaltet
#define csShortCircuit 0x04 // Kurzschluss
#define csServiceMode 0x08 // Der Programmiermodus ist aktiv - Service Mode
byte Railpower = csNormal;   //State of RailPower at Startup
bool Z21ButtonLastState = false;    //store last value of the Push Button for GO/STOP

//--------------------------------------------------------------
//LocoNet-Bus:
#if defined (LOCONET)
#include "LNInterface.h"
#endif

//--------------------------------------------------------------
//DCC Decoder:
#if defined(DECODER)
#include "DCCDecoder.h"
#endif

//--------------------------------------------------------------
//XpressNet-Bus:
#if defined(XPRESSNET)
#include "XBusInterface.h"
#endif

//--------------------------------------------------------------
//Z21 Ethernet communication:
#if defined(LAN) || defined(WIFI) || defined(ESP_WIFI)
#include "Z21_LAN.h"
#endif

//--------------------------------------------------------------------------------------------
//POWER set configuration:
static void globalPower (byte state) {
  if (Railpower != state) {
    
    #if defined(LAN) || defined (WIFI)  
    if (Railpower == csServiceMode && state == csShortCircuit) {
      z21.setCVNackSC();  //response SHORT while Service Mode!
    }
    #endif
    
    Railpower = state;
    #if defined(DEBUG)
    Debug.print(F("Power: "));
    Debug.println(state);
    #endif
    switch (state) {
      case csNormal: 
        #if defined(DCC)
        dcc.setpower(ON);
        digitalWrite(ProgRelaisPin, LOW);     //ProgTrack 
        #endif
        #if defined(BOOSTER_EXT)
        if (digitalRead(ShortExtPin) == LOW)
          digitalWrite(GoExtPin, BOOSTER_EXT_ON);
        #endif
       
        #if (defined(BOOSTER_INT) && !defined(BOOSTER_INT_NDCC))
        digitalWrite(GoIntPin, BOOSTER_INT_ON);
        #endif
     
      break;
      case csTrackVoltageOff: 
        #if defined(DCC)
        dcc.setpower(OFF);
        digitalWrite(ProgRelaisPin, LOW);     //ProgTrack 
        #endif
        #if defined(BOOSTER_EXT)
        digitalWrite(GoExtPin, BOOSTER_EXT_OFF);
        #endif
        
        #if (defined(BOOSTER_INT) && !defined(BOOSTER_INT_NDCC))
        digitalWrite(GoIntPin, BOOSTER_INT_OFF);
        #endif
        
      break;
      case csServiceMode:
        #if defined(DCC) 
        dcc.setpower(SERVICE); //already on!
        digitalWrite(ProgRelaisPin, HIGH);     //ProgTrack 
        #endif
        #if defined(BOOSTER_EXT)
          #if defined(BOOSTER_INT)
          digitalWrite(GoExtPin, BOOSTER_EXT_OFF);
          #else
          if (digitalRead(ShortExtPin) == LOW)
            digitalWrite(GoExtPin, BOOSTER_EXT_ON);
          #endif
        #endif

        #if (defined(BOOSTER_INT) && !defined(BOOSTER_INT_NDCC))
        digitalWrite(GoIntPin, BOOSTER_INT_ON);
        #endif
        
      break;
      case csShortCircuit: 
        #if defined(DCC)
        dcc.setpower(SHORT);  //shut down via GO/STOP just for the Roco Booster
        digitalWrite(ProgRelaisPin, LOW);     //ProgTrack 
        #endif
        #if defined(BOOSTER_EXT)
        digitalWrite(GoExtPin, BOOSTER_EXT_OFF);
        #endif
        
        #if (defined(BOOSTER_INT) && !defined(BOOSTER_INT_NDCC))
        digitalWrite(GoIntPin, BOOSTER_INT_OFF);
        #endif
        
      break;
      case csEmergencyStop:
        #if defined(DCC)
        dcc.eStop();  
        #endif
      break;
    }
    if (Railpower == csShortCircuit)
      digitalWrite(ShortLed, HIGH);   //Short LED show State "short"
    if (Railpower == csNormal)  
      digitalWrite(ShortLed, LOW);   //Short LED show State "normal" 
    #if defined(LAN) || defined(WIFI) || defined(ESP_WIFI)
    z21.setPower(Railpower);
    #endif
    #if defined(XPRESSNET)
    XpressNet.setPower(Railpower);  //send to XpressNet
    #endif
    #if defined(LOCONET)
    LNsetpower(); //send to LocoNet
    #endif
  }
}

//--------------------------------------------------------------------------------------------
//from DCCPacketScheduler -> notify power state
void notifyPower(uint8_t state) {
  globalPower(state);
}

//--------------------------------------------------------------------------------------------
#if defined(DCC)
void ShortDetection() { 
  //Short Circuit?
  //Check BOOSTER extern
  #if defined(BOOSTER_EXT)
  if ((digitalRead(ShortExtPin) == HIGH) && (digitalRead(GoExtPin) == BOOSTER_EXT_ON) && (Railpower != csShortCircuit)) {  
    ShortTime++;
    if(ShortTime == DetectShortCircuit) {
        globalPower(csShortCircuit);
        #if defined(DEBUG)
        Debug.println(F("TRACK_SHORT_CIRCUIT EXT"));
        #endif
    }
    /*  NOT IN USE ANYMORE from v4.75 on!
    //Before Railpower cut out test change polarity:
    else if (ShortTime == KSRelaisShortCircuit) {   
      digitalWrite(KSPin, !digitalRead(KSPin));     //Kehrschleife
      #if defined(DEBUG)
      Debug.print(F("KS "));
      Debug.println( digitalRead(KSPin) );
      #endif
    }
    */
  }
  else ShortTime = 0;
  #endif
  //Check BOOSTER2 (z.B. TLE5206)
  #if defined(BOOSTER_INT)
  //---------------Short2 for CDE external Booster----------------------------------
  #if defined(BOOSTER_EXT_CDE)
  if ((digitalRead(ShortIntPin) == LOW) && (Railpower != csShortCircuit)) {
    globalPower(csShortCircuit);
    #if defined(DEBUG)
    Debug.println(F("TRACK_SHORT_CIRCUIT CDE"));
    #endif
  }
  //---------------Short2 TLE detection----------------------------------
  #elif defined(BOOSTER_INT_TLE5206)
  #if defined(BOOSTER_INT_NDCC)
  if ((digitalRead(ShortIntPin) == LOW) && (Railpower != csShortCircuit)) {
  #else
  //---------------Old: without RailCom support----------------------------------
  if ((digitalRead(ShortIntPin) == LOW) && (digitalRead(GoIntPin) == BOOSTER_INT_ON) && (Railpower != csShortCircuit)) {
  #endif
    globalPower(csShortCircuit);
    #if defined(DEBUG)
    Debug.println(F("TRACK_SHORT_CIRCUIT INT"));
    #endif
  }
  #endif
  
  #if defined(BOOSTER_INT_CURRENT_SHORT_DETECT)
  uint16_t VAmp = analogRead(VAmpIntPin);
  if ((VAmp >= DETECT_SHORT_INT_VALUE) && (Railpower != csShortCircuit)) {
    ShortTimeINT++;
    if (ShortTimeINT == DETECT_SHORT_INT_WAIT) {
      globalPower(csShortCircuit);
      #if defined(DEBUG)
      Debug.print(VAmp);
      Debug.print("-t");
      Debug.print(ShortTimeINT);
      Debug.println(F(" TRACK_SHORT_CIRCUIT INT"));
      #endif
    }
  }
  else ShortTimeINT = 0;
  #endif
  #endif
}
#endif

//--------------------------------------------------------------------------------------------
void updateLedButton() {
  //Button to control Railpower state
  if ((digitalRead(Z21ButtonPin) == LOW) && (Z21ButtonLastState == false)) {  //Button DOWN
    Z21ButtonLastState = true;
    LEDcount = millis();
  }
  else {
    if ((digitalRead(Z21ButtonPin) == HIGH) && (Z21ButtonLastState == true)) {  //Button UP
       #if defined(DEBUG)
         Debug.print(F("Button "));
      #endif
      unsigned long currentMillis = millis(); 
      Z21ButtonLastState = false;
      if(currentMillis - LEDcount > 750) //push long?
        if (FIXSTORAGE.read(52) == 0x00)  //Power-Button (short): 0=Gleisspannung aus, 1=Nothalt  
          globalPower(csEmergencyStop);  
        else globalPower(csTrackVoltageOff);
      else {
        if (Railpower == csNormal) {
          if (FIXSTORAGE.read(52) == 0x00) //Power-Button (short): 0=Gleisspannung aus, 1=Nothalt  
            globalPower(csTrackVoltageOff);
          else globalPower(csEmergencyStop);
        }
        else globalPower(csNormal);
      }
      LEDcount = 0;
    }
  }
  //Update LED  
  if (Z21ButtonLastState == false) {  //flash
    if (Railpower == csNormal) {
      digitalWrite(DCCLed, HIGH);
      return;
    }
    unsigned long currentMillis = millis(); 
    if (currentMillis > LEDcount) {
      if (Railpower == csTrackVoltageOff) {
        if (digitalRead(DCCLed) == HIGH)
          LEDcount = currentMillis + 1100;    //long OFF
        else LEDcount = currentMillis + 300;  //short ON
      }
      if (Railpower == csEmergencyStop) {
        if (digitalRead(DCCLed) == HIGH)
          LEDcount = currentMillis + 80;    //short OFF
        else LEDcount = currentMillis + 700;  //long ON
      }
      if (Railpower == csShortCircuit) 
        LEDcount = currentMillis + 200;  //short flash
        
      digitalWrite(DCCLed, !digitalRead(DCCLed));
    }
  }
}

//--------------------------------------------------------------------------------------------
#if defined(HTTPCONF) && defined(LAN)
void Webconfig() {
  EthernetClient client = server.available();
  if (client) {
    String receivedText = String(50);
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (receivedText.length() < 50) {
          receivedText += c;
        }
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println(F("HTTP/1.1 200 OK"));
          client.println(F("Content-Type: text/html"));
          client.println(F("Connection: close"));  // the connection will be closed after completion of the response
          //client.println(F("Refresh: 5"));  // refresh the page automatically every 5 sec
          client.println();   //don't forget this!!!
          //Website:
          client.println(F("<!DOCTYPE html>"));
          client.println(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"));
          client.println(F("<html><title>Z21</title><body><h1>Z21</h1>"));
          //----------------------------------------------------------------------------------------------------          
          int firstPos = receivedText.indexOf("?");
          if (firstPos > -1) {
            #if defined(DEBUG)
            Debug.print("Save new values ");
            //Debug.print(receivedText);
            #endif
            byte lastPos = receivedText.indexOf(" ", firstPos);
            String theText = receivedText.substring(firstPos+3, lastPos); // 10 is the length of "?A="
            byte S88Pos = receivedText.indexOf("S88=");
            #if defined(S88N)
              S88Module = receivedText.substring(S88Pos+4, receivedText.indexOf("HTTP")-1).toInt();
            #endif  
            byte Aip = theText.indexOf("&B=");
            byte Bip = theText.indexOf("&C=", Aip);
            byte Cip = theText.indexOf("&D=", Bip);
            byte Dip = theText.substring(Cip+3, S88Pos).toInt();
            Cip = theText.substring(Bip+3, Cip).toInt();
            Bip = theText.substring(Aip+3, Bip).toInt();
            Aip = theText.substring(0, Aip).toInt();
            ip[0] = Aip;
            ip[1] = Bip;
            ip[2] = Cip;
            ip[3] = Dip;
            client.println(F("-> RESET Z21"));
            #if defined(S88N)
            if (FIXSTORAGE.read(EES88Moduls) != S88Module) {
              FIXSTORAGE.write(EES88Moduls, S88Module);
              #if defined(DEBUG)
              Debug.print("neu S88: ");
              Debug.print(S88Module);
              #endif
              SetupS88();
              #if defined(WIFI)
              WLANSetup();
              #endif
            }
            #endif
            FIXSTORAGE.FIXMODE(EEip, Aip);
            FIXSTORAGE.FIXMODE(EEip+1, Bip);
            FIXSTORAGE.FIXMODE(EEip+2, Cip);
            FIXSTORAGE.FIXMODE(EEip+3, Dip);
          }
          //----------------------------------------------------------------------------------------------------          
          client.print(F("<form method=get>IP:<input type=number min=0 max=254 name=A value="));
          client.println(ip[0]);
          #if defined(DHCP)
          client.print(F(" disabled=disabled"));
          #endif
          client.print(F("><input type=number min=0 max=254 name=B value="));
          client.println(ip[1]);
          #if defined(DHCP)
          client.print(F(" disabled=disabled"));
          #endif
          client.print(F("><input type=number min=0 max=254 name=C value="));
          client.println(ip[2]);
          #if defined(DHCP)
          client.print(F(" disabled=disabled"));
          #endif
          client.print(F("><input type=number min=0 max=254 name=D value="));
          client.println(ip[3]);
          #if defined(DHCP)
          client.print(F(" disabled=disabled"));
          #endif
          client.print(F("><br/>8x S88:<input type=number min=0 max="));
          #if defined(S88N)
          client.print(S88MAXMODULE);
          #else
          client.print("0");
          #endif
          client.print(F(" name=S88 value="));
          #if defined(S88N)
            client.print(S88Module);
          #else
            client.print("-");
          #endif
          client.println(F("><br/><input type=submit></form></body></html>"));
          break;
        }
        if (c == '\n') 
          currentLineIsBlank = true; // you're starting a new line
        else if (c != '\r') 
          currentLineIsBlank = false; // you've gotten a character on the current line
      }
    }
    client.stop();  // close the connection:
  }
}
#endif

//--------------------------------------------------------------------------------------------
#if (defined(ESP8266_MCU) || defined(ESP32_MCU)) && defined(ESP_HTTPCONF)
void Webconfig() {
  WiFiClient client = ESPWebserver.available();
  if (!client)
    return;
    
  String HTTP_req;            // stores the HTTP request 

  if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                HTTP_req += c;  // save the HTTP request 1 char at a time
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: keep-alive");
                    client.println();      //don't forget this!!!
                    // AJAX request for switch state
                    if (HTTP_req.indexOf("/ajax_switch") > -1) {
                        #if defined(DEBUG)
                        Debug.println("Reset WLAN conf!");
                        #endif
                        // read switch state and send appropriate paragraph text
                        ssid = HTTP_req.substring(HTTP_req.indexOf("&s=")+3,HTTP_req.indexOf("&p="));
                        pass = HTTP_req.substring(HTTP_req.indexOf("&p=")+3,HTTP_req.indexOf("&As="));
                        ssidAP = HTTP_req.substring(HTTP_req.indexOf("&As=")+4,HTTP_req.indexOf("&Ap="));
                        passAP = HTTP_req.substring(HTTP_req.indexOf("&Ap=")+4,HTTP_req.indexOf("&Ak="));
                        kanalAP = HTTP_req.substring(HTTP_req.indexOf("&Ak=")+4,HTTP_req.indexOf("&S8=")).toInt();
                        #if defined(S88N)                        
                          S88Module = HTTP_req.substring(HTTP_req.indexOf("&S8=")+4,HTTP_req.indexOf("&nocache")).toInt();
                        #endif
                        
                        if ((kanalAP < 1) || (kanalAP > 13)) {
                          kanalAP = SkanalAP;
                          client.print("Ka. error! ");
                        }
                        if (passAP.length() < 8) {
                          passAP = SpassAP;
                          client.print("Code length error (min. 8)! ");
                        }
                        
                        // write eeprom
                        EEPROMwrite (ssid, EEssidLength, EEssidBegin);
                        EEPROMwrite (pass, EEpassLength, EEpassBegin);
                        
                        EEPROMwrite (ssidAP, EEssidAPLength, EEssidAPBegin);
                        EEPROMwrite (passAP, EEpassAPLength, EEpassAPBegin);
                        EEPROM.write(EEkanalAP, kanalAP);

                        EEPROM.commit(); 
                        
                        WiFi.disconnect();
                        tryWifiClient();
                        
                        Udp.begin(z21Port);

                        client.println("saved");   //OK!
                    }
                    else {  // HTTP request for web page
                        // send web page - contains JavaScript with AJAX calls
                        client.println("<!DOCTYPE html>");
                        client.println("<html><head><title>Z21</title>");
                        client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>");
                        client.println("<script>");
                        client.println("function SetState() {");
                        client.println("document.getElementById(\"state\").innerHTML = \"wait\";");
                        client.println("nocache = \"&s=\" + document.getElementById(\"ssid\").value;");
                        client.println("nocache += \"&p=\" + document.getElementById(\"pass\").value;");
                        client.println("nocache += \"&As=\" + document.getElementById(\"ssidAP\").value;");
                        client.println("nocache += \"&Ap=\" + document.getElementById(\"passAP\").value;");
                        client.println("nocache += \"&Ak=\" + document.getElementById(\"kanalAP\").value;");
                        client.println("nocache += \"&S8=\" + document.getElementById(\"S88\").value;");
                        client.println("nocache += \"&nocache=\" + Math.random() * 1000000;");
                        client.println("var request = new XMLHttpRequest();");
                        client.println("request.onreadystatechange = function() {");
                        client.println("if (this.readyState == 4){");
//                        client.println("if (this.status == 200){");
//                        client.println("if (this.responseText != null) {");
                        client.println("document.getElementById(\"state\").innerHTML = this.responseText;");
                        client.println("top.window.location.reload(true);");
                        client.println("}}");
                        client.println("request.open(\"GET\", \"ajax_switch\" + nocache, true);");
                        client.println("request.send(null);");
                        //client.println("setTimeout('SetState()', 1000);");
                        client.println("}");
                        client.println("</script>");
                        client.println("</head>");
                        client.println("<body><h1>Z21 Net-config</h1><hr>");
                        client.print("<h2>WiFi Direct AP</h2>");
                        client.print("<dl><dd>IP: ");
                        client.print(WiFi.softAPIP());
                        client.print("</dd><dd>Connected Clients: ");
                        client.print(WiFi.softAPgetStationNum());
                        client.print(" of 4</dd><dd>SSID: <input type=\"text\" id=\"ssidAP\" value=\"");
                        client.print(ssidAP);
                        client.print("\"></dd><dd>code: <input type=\"text\" id=\"passAP\" value=\"");
                        client.print(passAP);
                        client.print("\"></dd><dd>Ka.: <input type=\"number\" min=\"1\" max=\"13\" id=\"kanalAP\" value=\"");
                        client.print(kanalAP);
                        client.println("\"></dd></dl>");
                        
                        client.print("<h2>WiFi client</h2>");
                        client.print("<dl><dd>IP: ");
                        if (WiFi.status() == WL_CONNECTED)
                          client.print(WiFi.localIP());
                        else client.print("none");
                        client.print("</dd><dd>SSID: <input type=text id=\"ssid\" value=\"");  
                        client.print(ssid);
                        client.print("\"></dd><dd>code: <input type=text id=\"pass\" value=\"");
                        client.print(pass);
                        client.println("\"></dd></dl>");

                        client.println("<h2>S88 Module</h2>");
                        client.print("<dl><dd>8x Anzahl: <input type=number min=\"0\" max=\"62\" id=\"S88\" value=\"");
                        #if defined(S88N)                        
                          client.print(S88Module);
                          client.print("\"");
                        #else
                          client.print("0\" disabled");
                        #endif  
                        client.println("></dd></dl><br>");
                        
                        client.println("<input type=submit onclick=\"SetState()\">"); 
                        client.println("<p id=\"state\"></p>");
                        client.print("<hr><p>Z21_ESP_Central_UDP_v");
                        client.print(Z21mobileSwVer);
                        client.print(SwitchFormat);
                        #if defined (BOOSTER_INT_NDCC)
                        if (FIXSTORAGE.read(EEPROMRailCom) == 0x01)
                          client.print(".RAILCOM");
                        #endif
                        client.println("<br>Copyright (c) 2018 Philipp Gahtow<br>digitalmoba@arcor.de</p>");
                        client.println("</body>");
                        client.print("</html>");
                    }
                    // display received HTTP request on serial port
                    //Serial.print(HTTP_req);
                    HTTP_req = "";            // finished with request, empty string
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}
#endif

//--------------------------------------------------------------
//DCC handle back the request switch state
void notifyTrnt(uint16_t Adr, bool State) 
{
  #if defined(LAN) || defined(WIFI) || defined(ESP_WIFI)
  z21.setTrntInfo(Adr, State);
  #endif
  
  #if defined(DEBUG)
  Debug.print(F("DCC Trnt "));
  Debug.print(Adr);
  Debug.print("-");
  Debug.println(State);
  #endif
}

//-------------------------------------------------------------- 
//DCC return a CV value:
void notifyCVVerify(uint16_t CV, uint8_t value) {
  #if defined(LAN) || defined(WIFI) || defined(ESP_WIFI)
  z21.setCVReturn (CV, value);
  #endif
  
  #if defined(XPRESSNET) 
  XpressNet.setCVReadValue(CV, value);
  #endif
  
  #if defined(DEBUG)
  Debug.print(F("Verify CV#"));
  Debug.print(CV);
  Debug.print(" - ");
  Debug.print(value);
  Debug.print(" b");
  Debug.println(value, BIN);
  #endif
}

//-------------------------------------------------------------- 
//DCC return no ACK:
void notifyCVNack() {
  #if defined(LAN) || defined(WIFI) || defined(ESP_WIFI)
  z21.setCVNack();  //send back to device and stop programming!
  #endif
  
  #if defined(XPRESSNET) 
  XpressNet.setCVNack();
  #endif
  
  #if defined(DEBUG)
  Debug.println("CV# no ACK");
  #endif
}

//-------------------------------------------------------------- 
//DCC handle railpower while programming (Service Mode ON/OFF)
void notifyRailpower(uint8_t state) {
  globalPower(state); //send Power state to all Devices!
}

//-------------------------------------------------------------- 
//Reset the EEPROM to default:
void EEPROM_Load_Defaults() {

  #if defined(ESP32)
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&myMutex);
  #endif  

  #if defined(DEBUG)
     Debug.println(F("Set all EEPROM to default!")); 
  #endif  

  FIXSTORAGE.FIXMODE(EEPROMRailCom, 1); //RailCom ON
  FIXSTORAGE.FIXMODE(EEPROMRSTsRepeat, 25); //Reset Pakete (start)
  FIXSTORAGE.FIXMODE(EEPROMRSTcRepeat, 12); //Reset Pakete (fortsetzen)
  FIXSTORAGE.FIXMODE(EEPROMProgRepeat, 12); //Programmier Pakete

  FIXSTORAGE.FIXMODE(EES88Moduls, 62);   //S88 max Module
  //IP Werkseinstellung:
  FIXSTORAGE.FIXMODE(EEip,   192);
  FIXSTORAGE.FIXMODE(EEip+1, 168);
  FIXSTORAGE.FIXMODE(EEip+2,   0);
  FIXSTORAGE.FIXMODE(EEip+3, 111);

  //Default VCC Rail and Prog to 20V:
  FIXSTORAGE.FIXMODE(72, 32);
  FIXSTORAGE.FIXMODE(73, 78);
  FIXSTORAGE.FIXMODE(74, 32);
  FIXSTORAGE.FIXMODE(75, 78);

  //Default Prog option:
  FIXSTORAGE.FIXMODE(52, 0);
  FIXSTORAGE.FIXMODE(53, 3);

  #if defined(ESP_WIFI)
  EEPROM.commit();
  #endif

  #if defined(ESP32)  
  portEXIT_CRITICAL(&myMutex);
  #endif 
}

//-------------------------------------------------------------- 
//Check the firmware status need update?:
void EEPROM_Setup() {
  #if defined(DEBUG)
  Debug.print(F("Check EEPROM..."));
  #endif
  
  
  byte SwVerMSB = FIXSTORAGE.read(EEPROMSwVerMSB);
  byte SwVerLSB = FIXSTORAGE.read(EEPROMSwVerLSB);
  //Check if we already run on this MCU?
  if ((SwVerMSB == 0xFF) && (SwVerLSB == 0xFF)) {
    //First Startup, set everything to default!
    EEPROM_Load_Defaults();
    
  }
  //Check if there need to handel an update?
  if ( (SwVerMSB != highByte(Z21mobileSwVer)) || (SwVerLSB != lowByte(Z21mobileSwVer)) ) {
    //Update to new Firmware
    #if defined(DEBUG)
    Debug.println(F("Firmware Update!"));
    #endif
/*
    #if defined(ESP32)
    portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&myMutex);
    #endif
*/
    FIXSTORAGE.FIXMODE(EEPROMSwVerMSB, highByte(Z21mobileSwVer));
    FIXSTORAGE.FIXMODE(EEPROMSwVerLSB, lowByte(Z21mobileSwVer));

    #if defined(ESP_WIFI)
    EEPROM.commit();
    #endif
/*
    #if defined(ESP32)  
    portEXIT_CRITICAL(&myMutex);
    #endif 
    */
  }
  else {
    #if defined(DEBUG)
      Debug.println(F("OK"));
    #endif
  }
  #if defined(ESP32)
  yield();
  #endif
}

//--------------------------------------------------------------------------------------------
#if defined(DEBUG) && !defined(DUE_MCU) && !defined(ESP8266) && !defined(ESP32)
//ONLY for Atmega, not for Arduino DUE or ESP chip (ARM)!
int freeRam () 
{
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#endif

//--------------------------------------------------------------------------------------------
//ONLY for ESP
#if defined(DEBUG) && (defined(ESP8266) || defined(ESP32))
int freeRam () 
{
  return ESP.getFreeHeap();
}
#endif

//--------------------------------------------------------------
//INIT all ports and interfaces:
void setup() {	
  
  //Reduce the reference Voltage to 1,1 Volt!
  #if defined(AREF_1V1) && !defined(ESP32_MCU) && !defined(ESP8266_MCU)
    analogReference(AREF_1V1); //EXTERNAL or INTERNAL1V1
  #endif  

  pinMode(DCCLed, OUTPUT);      //DCC Status LED
  digitalWrite(DCCLed, LOW);    //DCC LED is in "off" State
  pinMode(ShortLed, OUTPUT);    //Short Status LED
  digitalWrite(ShortLed, HIGH);    //Short LED showes working and Power up
  pinMode(ProgRelaisPin, OUTPUT);       //ProgTrack-Relais
  digitalWrite(ProgRelaisPin, LOW);     //ProgTrack 
  #if defined(BOOSTER_EXT)    //Booster (ROCO) external: 
    pinMode(ShortExtPin, INPUT_PULLUP);  //set short pin and Turn on internal Pull-Up Resistor
    pinMode(GoExtPin, OUTPUT);      //GO/STOP Signal
    digitalWrite(GoExtPin, BOOSTER_EXT_OFF);    //set STOP to Booster
  #endif
  #if defined(BOOSTER_INT)    //Booster2 internal:
    #if !defined(BOOSTER_INT_NDCC)
    pinMode(GoIntPin, OUTPUT);    //GO/STOP2 Signal
    digitalWrite(GoIntPin, BOOSTER_INT_OFF);   //set STOP to Booster2 invertet
    #endif
    #if defined(BOOSTER_INT_TLE5206) || defined(BOOSTER_EXT_CDE)
    pinMode(ShortIntPin, INPUT_PULLUP);  //set up short2 PIN and Turn on internal Pull-Up Resistor
    #endif
  #endif
  pinMode(Z21ResetPin, INPUT_PULLUP); //Turn on internal Pull-Up Resistor
  pinMode(Z21ButtonPin, INPUT_PULLUP); //Turn on internal Pull-Up Resistor

  #if defined(MEGA_MCU)
  //pinMode(VAmSencePin, INPUT_PULLUP);  //AC 5A Sensor (for testing only)
  #if defined(AREF_1V1)
    pinMode(VoltIntPin, INPUT);  //Rail Voltage: Rail:100k - Sence - 4,7k - GND
    #if !defined(DALLASTEMPSENSE)
    pinMode(TempPin, INPUT);     //Temp.Resistor(15k)
    #endif
  #else
    pinMode(VoltIntPin, INPUT_PULLUP);  //Rail Voltage: Rail:100k - Sence - 4,7k - GND
    #if !defined(DALLASTEMPSENSE)
    pinMode(TempPin, INPUT_PULLUP);     //Temp.Resistor(15k)
    #endif
  #endif
  #endif

  #if defined(DEBUG) || defined(LnDEB) || defined(Z21DEBUG) || defined(REPORT) || defined(XnDEB)
    Debug.begin(DebugBaud);
    #if defined(ESP8266_MCU) || defined(ESP32_MCU)
    Debug.println();  //Zeilenumbruch einfügen
    #endif
    Debug.print(F("Z21 v"));
    Debug.print(Z21mobileSwVer);
    Debug.print(SwitchFormat);

    #if defined(UNO_MCU)
    Debug.print(F(" - UNO"));
    #elif defined(MEGA_MCU)
    Debug.print(F(" - MEGA"));
    #elif defined(SANGUINO_MCU)
    Debug.print(F(" - SANGUINO"));
    #elif defined(DUE_MCU)
    Debug.print(F(" - DUE"));
    #elif defined(ESP8266_MCU)
    Debug.print(F(" - ESP8266"));
    #elif defined(ESP32_MCU)
    Debug.print(F(" - ESP32"));
    #endif
    
    #if defined (BOOSTER_INT_NDCC)
      if (FIXSTORAGE.read(EEPROMRailCom) == 0x01)
        Debug.print(".RAILCOM");
    #endif

    Debug.println();
    
  #endif

  #if defined(ESP_WIFI)
  EEPROM.begin(EESize);  //init EEPROM
  ESPSetup();   //ESP8266 and ESP32 WLAN Setup
  #endif

  //Check Firmware in EEPROM:
  EEPROM_Setup();   //init Z21 EEPROM defaults
  
  #if defined(DCC)
    //setup the DCC signal:
    #if defined(BOOSTER_INT_NDCC)
      #if defined(FS14)
      dcc.setup(DCCPin, GoIntPin, DCC14, SwitchFormat, Railpower); 
      #elif defined(FS28)
      dcc.setup(DCCPin, GoIntPin, DCC28, SwitchFormat, Railpower); 
      #else
      dcc.setup(DCCPin, GoIntPin, DCC128, SwitchFormat, Railpower); 
      #endif
    #else
      #if defined(FS14)    
      dcc.setup(DCCPin, 0, DCC14, SwitchFormat, Railpower);  //no NDCC and no RAILCOM
      #elif defined(FS28)    
      dcc.setup(DCCPin, 0, DCC28, SwitchFormat, Railpower);  //no NDCC and no RAILCOM
      #else
      dcc.setup(DCCPin, 0, DCC128, SwitchFormat, Railpower);  //no NDCC and no RAILCOM
      #endif
    #endif  
    //for CV reading activate the current control:
    #if defined(BOOSTER_INT_MAINCURRENT) 
	  //pinMode(VAmpIntPin, INPUT);//_PULLUP); 
      dcc.setCurrentLoadPin(VAmpIntPin);
    #endif
    //for CV reading over RAILCOM activate i2c communication:
    #if defined(DCCGLOBALDETECTOR)
      #if defined(MEGA_MCU)
      RailComSetup(); //init the Serial interface for receiving RailCom
      #endif
    #endif
    //extra DCC Output for S88 or LocoNet
    #if defined(additionalOutPin)
      #if (!defined(LAN) && defined (UNO_MCU)) || defined(MEGA_MCU) || defined(DUE_MCU) || defined(ESP_WIFI)
        dcc.enable_additional_DCC_output(additionalOutPin);
        #if defined(DEBUG)
          Debug.println(".addOutput");
        #endif
      #endif
    #endif
  #endif

  #if defined(LAN)
  //Want to RESET Ethernet to default IP?
  if ((digitalRead(Z21ButtonPin) == LOW) || (FIXSTORAGE.read(EEip) == 255)) {
      
      EEPROM_Load_Defaults();
      
      while (digitalRead(Z21ButtonPin) == LOW) {  //Wait until Button - "UP"
        #if defined(DEBUG)
          Debug.print("."); 
        #endif  
        delay(200);   //Flash:
        digitalWrite(DCCLed, !digitalRead(DCCLed));
        digitalWrite(ShortLed, !digitalRead(DCCLed));
      }
      #if defined(DEBUG)
          Debug.println();  //new line!
      #endif  
      digitalWrite(DCCLed, LOW);    //DCC LED is in "off" State
      digitalWrite(ShortLed, LOW);    //Short LED is in "off" State
  }
  
  ip[0] = FIXSTORAGE.read(EEip);
  ip[1] = FIXSTORAGE.read(EEip+1);
  ip[2] = FIXSTORAGE.read(EEip+2);
  ip[3] = FIXSTORAGE.read(EEip+3);
  #endif

  #if defined(S88N)
    SetupS88();    //S88 Setup 
  #endif  

  #if defined(XPRESSNET)  
    #if defined(FS14)
    XpressNet.setup(Loco14, XNetTxRxPin);    //Initialisierung XNet Serial und Send/Receive-PIN  
    #elif defined(FS28)
    XpressNet.setup(Loco28, XNetTxRxPin);    //Initialisierung XNet Serial und Send/Receive-PIN  
    #else
    XpressNet.setup(Loco128, XNetTxRxPin);    //Initialisierung XNet Serial und Send/Receive-PIN  
    #endif
    XpressNet.setPower(Railpower);  //send Railpowerinformation to XpressNet
  #endif

  #if defined(DECODER)
    DCCDecoder_init();    //DCC Decoder init
  #endif
  
  #if defined(LOCONET)
    LNsetup();      //LocoNet Interface init
    LNsetpower();   //send Railpowerinformation to LocoNet
  #endif

  #if defined(LAN) || defined(WIFI) || defined(ESP_WIFI)
    z21.setPower(Railpower);  //send Railpowerinformation to Z21 Interface
  #endif

  #if defined(WIFI)
    WLANSetup();    //Start ESP WLAN
  #endif 

  #if defined(ESP_WIFI)
  
    #if defined(ESP32)
    yield();
    delay(2000);
    #endif
    
    #if defined(ESP_HTTPCONF)
      #if defined(DEBUG)
      Debug.print(F("Server Begin..."));
      #endif
      ESPWebserver.begin(); //Start the HTTP server
      #if defined(DEBUG)
      Debug.println("OK");
      #endif
    #endif
    #if defined(DEBUG)
    Debug.print(F("Start UDP listener..."));
    #endif
    Udp.begin(z21Port);   //open Z21 port
    #if defined(DEBUG)
    Debug.println("OK");
    #endif
    #if defined(ESP32)
    yield();
    #endif
    
    #if defined(ESP_OTA)
      #if defined(DEBUG)
      Debug.print(F("Init OTA..."));
      #endif
      ESP_OTA_Init();
      #if defined(DEBUG)
      Debug.println("OK");
      #endif
    #endif
  #endif

  

  #if defined(LAN)
    // start the Ethernet and UDP:
    delay(120); //wait for Ethernet Shield to get up (need longer to boot!)

    #if defined(ENC28)
     /* Disable SD card */
    pinMode(SDSSPIN, OUTPUT);
    digitalWrite(SDSSPIN, HIGH);
    #endif

    Ethernet.init(LANSSPIN);  //SS-Pin Most Arduino shields
    
    #if defined(DHCP)
      #if defined(DEBUG)
          Debug.print(F("IP over DHCP..."));  
      #endif 
      #if defined(ENC28)
      if (Ethernet.begin(mac) == 0) { //ENC28 UIP Lib doesn't have timeout option!
      #else 
      if (Ethernet.begin(mac,LANTimeoutDHCP,2000) == 0) { //Trying to get an IP address using DHCP
      #endif  
        #if defined(DEBUG)
          Debug.println(F("fail!"));  
        #endif
        #undef DHCP
      }
      else {
        //Save IP that receive from DHCP
        ip = Ethernet.localIP();
        #if defined(DEBUG)
          Debug.println("OK");  
        #endif
      }
    #endif
    #if !defined(DHCP)
      // initialize the Ethernet device not using DHCP:
      Ethernet.begin(mac,ip);  //set IP and MAC  
    #endif
    Udp.begin(z21Port);  //UDP Z21 Port 21105

  //UdpMT.begin(34472);   //UDP Maintenance Tool
  //0x30 0x80 0x01 0x02

    #if defined(HTTPCONF)
      server.begin();    //HTTP Server
    #endif
  #endif

  #if defined(DEBUG)
    #if defined(LAN)
    Debug.print(F("Eth IP: "));
    Debug.println(ip);
    #endif
    #if defined(S88N)
      Debug.print(F("S88N Module: "));
      Debug.println(S88Module);
    #endif
    #if !defined(DUE_MCU) //not for the DUE!
      Debug.print(F("RAM: "));
      Debug.println(freeRam());  
    #endif
  #endif

  #if defined(DALLASTEMPSENSE) && defined(MEGA_MCU)
  sensors.begin();
  sensors.getAddress(insideThermometer, 0);
  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(insideThermometer, DALLAS_RESOLUTION);
  sensors.setWaitForConversion(false);  //kein Warten auf die Umwandlung!

  sensors.requestTemperatures();  //1. Abfrage der Temperatur
  #endif

  digitalWrite(ShortLed, LOW);    //Short LED goes off - we are ready to work!
}

//--------------------------------------------------------------------------------------------
//run the state machine to update all interfaces
void loop() {

   #if defined(ESP32)
  yield();
  #endif  


  updateLedButton();     //DCC Status LED and Button

  #if defined(DCC)
  ShortDetection();  //handel short on rail to => power off
  dcc.update();    //handel Rail Data
  
    #if defined(DCCGLOBALDETECTOR) && defined(MEGA_MCU)
    RailComRead();  //check RailCom Data
    #endif
    
  #endif

  #if (defined(HTTPCONF) && defined(LAN)) || ((defined(ESP8266_MCU) || defined(ESP32_MCU)) && defined(ESP_HTTPCONF))
    Webconfig();    //Webserver for Configuration
  #endif
  
  #if defined(S88N)
    notifyS88Data();    //R-Bus geänderte Daten 1. Melden
  #endif  
  
  #if defined(DECODER)
    DCCDecoder_update();    //Daten vom DCC Decoder
  #endif
  
  #if defined(XPRESSNET)  
    XpressNet.update(); //XpressNet Update
  #endif

  #if defined(LOCONET) && !defined(ESP32_MCU) //handle ESP32 with call back function!
    LNupdate();      //LocoNet update
  #endif
  
  #if defined(LAN) || defined(WIFI) || defined(ESP_WIFI)
    Z21LANreceive();   //Z21 LAN Decoding
  #endif

  #if defined(DHCP) && defined(LAN)
     Ethernet.maintain(); //renewal of DHCP leases
  #endif  

  #if defined(ESP_OTA)
    ArduinoOTA.handle();
  #endif

}
