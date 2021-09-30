#ifndef Z_21_H_
#define Z_21_H_

/*
  z21.h - library for Z21mobile protocoll
  Copyright (c) 2013-2016 Philipp Gahtow  All right reserved.

  Version 2.1

  ROCO Z21 LAN Protocol for Arduino.
  
  Notice:
	- analyse the data and give back the content and a answer

  Grundlage: Z21 LAN Protokoll Spezifikation V1.05 (21.01.2015)

  �nderungen:
	- 23.09.15 Anpassung LAN_LOCONET_DETECTOR
			   Fehlerbeseitigung bei der LAN Pr�fsumme
			   Anpassung LAN_LOCONET_DISPATCH
	- 14.07.16 add S88 Gruppenindex for request
	- 22.08.16 add POM read notify
	- 19.12.16 add CV return value for Service Mode
	- 27.12.16 add CV no ACK and CV Short Circuit
	- 15.03.17 add System Information
	- 03.04.17 fix LAN_X_SET_LOCO_FUNCTION in DB3 type and index
	- 14.04.17 add EEPROM and store Z21 configuration (EEPROM: 50-75)
	- 19.06.17 add FW Version 1.28, 1.29 and 1.30
	- 06.08.17 add support for Arduino DUE
	- 27.08.17 fix speed step setting
*/

// include types & constants of Wiring core API
#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
//--------------------------------------------------------------
#define z21Port 21105      // local port to listen on
#define PORT z21Port

#define Z21_UDP_TX_MAX_SIZE 128 // max received UDP packet size

#define CONFIG_EXAMPLE_IPV4

// Init local variables
uint8_t packetBuffer[Z21_UDP_TX_MAX_SIZE];
volatile uint8_t txBuffer[Z21_UDP_TX_MAX_SIZE];
volatile uint8_t txBlen;
volatile uint8_t txBflag;
volatile uint8_t txSendFlag;
volatile uint16_t txBsock;
volatile ip4_addr_t txAddr;

volatile uint8_t storedIP; // number of currently stored IPs

//**************************************************************




//**************************************************************
//Firmware-Version der Z21:
#define z21FWVersionMSB 0x01
#define z21FWVersionLSB 0x30
/*
HwType: #define D_HWT_Z21_OLD 0x00000200 // �schwarze Z21� (Hardware-Variante ab 2012) 
		#define D_HWT_Z21_NEW 0x00000201 // �schwarze Z21�(Hardware-Variante ab 2013) 
		#define D_HWT_SMARTRAIL 0x00000202 // SmartRail (ab 2012) 
		#define D_HWT_z21_SMALL 0x00000203 // �wei�e z21� Starterset-Variante (ab 2013) 
		#define D_HWT_z21_START 0x00000204 // �z21 start� Starterset-Variante (ab 2016) 
*/
//Hardware-Typ: 0x00000201 // Z21 (Hardware-Variante ab 2013)
#define z21HWTypeMSB 0x02
#define z21HWTypeLSB 0x01
//Seriennummer:
#define z21SnMSB 0x1A
#define z21SnLSB 0xF5
//Store Z21 configuration inside EEPROM:
#define CONF1STORE 50 	//(10x Byte)
#define CONF2STORE 60	//(15x Byte)
//--------------------------------------------------------------
// certain global XPressnet status indicators
#define csNormal 0x00 // Normal Operation Resumed ist eingeschaltet
#define csEmergencyStop 0x01// Der Nothalt ist eingeschaltet
#define csTrackVoltageOff 0x02 // Die Gleisspannung ist abgeschaltet
#define csShortCircuit 0x04 // Kurzschluss
#define csServiceMode 0x08 // Der Programmiermodus ist aktiv - Service Mode

// IP settings
#define z21clientMAX 20        //Speichergr��e f�r IP-Adressen
#define z21ActTimeIP 20    //Aktivhaltung einer IP f�r (sec./2)
#define z21IPinterval 2000   //interval at milliseconds

//DCC Speed Steps
#define DCCSTEP14	0x01
#define DCCSTEP28	0x02
#define DCCSTEP128	0x03

struct TypeActIP
{
  uint8_t ip0;    // Byte IP
  uint8_t ip1;    // Byte IP
  uint8_t ip2;    // Byte IP
  uint8_t ip3;    // Byte IP
  uint16_t BCFlag; //BoadCastFlag 4. Byte Speichern
  uint8_t time;   //Zeit
  uint16_t port;
  uint8_t client;
  uint16_t adr;		//Loco control Adr
};

uint8_t Railpower;		  //state of the railpower
long z21IPpreviousMillis; // will store last time of IP decount updated




typedef struct //Rückmeldung des Status der Programmierung
{
	uint8_t IP0; //client IP-Adresse
	uint8_t IP1;
	uint8_t IP2;
	uint8_t IP3;
	uint8_t time;	   //aktive Zeit
	unsigned int port; //source Port
} listofIP;
listofIP mem[z21clientMAX];


//volatile uint8_t rx_buffer[128];

//Variables:
uint8_t Railpower;				   //state of the railpower
long z21IPpreviousMillis;	   // will store last time of IP decount updated
struct TypeActIP ActIP[z21clientMAX]; //Speicherarray f�r IPs

void receive(uint8_t client, uint8_t *packet); //Pr�fe auf neue Ethernet Daten

void setPower(uint8_t state); //Zustand Gleisspannung Melden

uint8_t getPower();			  //Zusand Gleisspannung ausgeben

void setCVPOMBYTE(uint16_t CVAdr, uint8_t value); //POM write byte return

void setLocoStateFull(int Adr, uint8_t steps, uint8_t speed, uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3, bool bc); //send Loco state
																		 

void setS88Data(uint8_t *data, uint8_t modules); //return state of S88 sensors

void setLNDetector(uint8_t client, uint8_t *data, uint8_t DataLen);			//return state from LN detector
void setLNMessage(uint8_t *data, uint8_t DataLen, uint8_t bcType, bool TX); //return LN Message

void setCANDetector(uint16_t NID, uint16_t Adr, uint8_t port, uint8_t typ, uint16_t v1, uint16_t v2); //state from CAN detector

void setTrntInfo(uint16_t Adr, bool State); //Return the state of accessory

void setExtACCInfo(uint16_t Adr, uint8_t State); //Return EXT Accessory INFO

void setCVReturn(uint16_t CV, uint8_t value); //Return CV Value for Programming
void setCVNack();							  //Return no ACK from Decoder
void setCVNackSC();							  //Return Short while Programming

void sendSystemInfo(uint8_t client, uint16_t maincurrent, uint16_t mainvoltage, uint16_t temp); //Send to all clients that request via BC the System Information




//Functions:
void returnLocoStateFull(uint8_t client, uint16_t Adr, bool bc); //Antwort auf Statusabfrage
void EthSend(uint8_t client, unsigned int DataLen, unsigned int Header, uint8_t *dataString, bool withXOR, uint8_t BC);
uint8_t getLocalBcFlag(uint32_t flag); //Convert Z21 LAN BC flag to local stored flag
uint8_t Z21addIP(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, unsigned int port); 
void clearIP(uint8_t pos); //delete the stored client
void clearIPSlots();					 //delete all stored clients
void clearIPSlot(uint8_t client);		 //delete a client
uint8_t addIPToSlot(uint8_t client, uint8_t BCFlag);

uint32_t getz21BcFlag(uint8_t flag); //Convert local stored flag back into a Z21 Flag

void setOtherSlotBusy(uint8_t slot);
void addBusySlot(uint8_t client, uint16_t adr); 

void reqLocoBusy(uint16_t adr);

uint8_t getEEPROMBCFlagIndex();					//return the length of BC-Flag store
void setEEPROMBCFlag(uint8_t IPHash, uint8_t BCFlag); //add BC-Flag to store
uint8_t findEEPROMBCFlag(uint8_t IPHash);		//read the BC-Flag for this client

#if defined(__cplusplus)
extern "C"
{
#endif

	extern void notifyz21getSystemInfo(uint8_t client) __attribute__((weak));

	extern void notifyz21EthSend(uint8_t client, uint8_t *data, uint8_t datalen) __attribute__((weak));

	extern void notifyz21LNdetector(uint8_t client, uint8_t typ, uint16_t Adr) __attribute__((weak));
	extern uint8_t notifyz21LNdispatch(uint8_t Adr2, uint8_t Adr) __attribute__((weak));
	extern void notifyz21LNSendPacket(uint8_t *data, uint8_t length) __attribute__((weak));

	extern void notifyz21CANdetector(uint8_t client, uint8_t typ, uint16_t ID) __attribute__((weak));

	extern void notifyz21RailPower(uint8_t State) __attribute__((weak));

	extern void notifyz21CVREAD(uint8_t cvAdrMSB, uint8_t cvAdrLSB) __attribute__((weak));
	extern void notifyz21CVWRITE(uint8_t cvAdrMSB, uint8_t cvAdrLSB, uint8_t value) __attribute__((weak));
	extern void notifyz21CVPOMWRITEBYTE(uint16_t Adr, uint16_t cvAdr, uint8_t value) __attribute__((weak));
	extern void notifyz21CVPOMWRITEBIT(uint16_t Adr, uint16_t cvAdr, uint8_t value) __attribute__((weak));
	extern void notifyz21CVPOMREADBYTE(uint16_t Adr, uint16_t cvAdr) __attribute__((weak));

	extern uint8_t notifyz21AccessoryInfo(uint16_t Adr) __attribute__((weak));
	extern void notifyz21Accessory(uint16_t Adr, bool state, bool active) __attribute__((weak));

	extern void notifyz21ExtAccessory(uint16_t Adr, uint8_t state) __attribute__((weak));

	extern void notifyz21LocoState(uint16_t Adr, uint8_t data[]) __attribute__((weak));
	extern void notifyz21LocoFkt(uint16_t Adr, uint8_t type, uint8_t fkt) __attribute__((weak));
	extern void notifyz21LocoSpeed(uint16_t Adr, uint8_t speed, uint8_t steps) __attribute__((weak));

	extern void notifyz21S88Data(uint8_t gIndex) __attribute__((weak)); //return last state S88 Data for the Client!

	extern uint16_t notifyz21Railcom() __attribute__((weak)); //return global Railcom Adr

	extern void notifyz21UpdateConf() __attribute__((weak)); //information for DCC via EEPROM (RailCom, ProgMode,...)

	extern uint8_t requestz21ClientHash(uint8_t client) __attribute__((weak));

#if defined(__cplusplus)
}
#endif

#endif /* Z_21_H_ */