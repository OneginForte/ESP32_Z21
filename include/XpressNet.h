/*
  XpressNet.h - library for XpressNet protocoll
  Copyright (c) 2013-2017 Philipp Gahtow  All right reserved.
  for Private use only!

  Version 2.5 (26.08.2021)

  Notice:
  Works until now, only with XPressNet Version 3.0 or higher!
  *********************************************************************
  21.07.2015 Philipp Gahtow - change adressing of switch commands
							- optimize memory use of setPower Function
  29.09.2015 Philipp Gahtow - fix F13 to F20 command for Multimaus
  17.11.2015 Philipp Gahtow - fix in setTrntPos for AdrL
  02.02.2017 Philipp Gahtow - add accessory change 0x52 (by Norberto Redondo Melchor)
							- fix in setTrntPos Adr convert
							- fix narrow conversations in arrays
  28.04.2017 Philipp Gahtow - optimize ram usage in callByteParity							
  23.07.2017 Philipp Gahtow - add changes for WLANMaus made by Andr� Schenk
  20.10.2017 Philipp Gahtow - add BC for "Schaltinformation"
  18.04.2020 Philipp Gahtow - fix long Address from 100 on!
  26.08.2021 Philipp Gahtow - add E4 commands and detect all messages with seperate stack
*/

// ensure this library description is only included once
#ifndef XpressNet_h
#define XpressNet_h

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
//--------------------------------------------------------------------------------------------
#define LOW 0
#define HIGH 1
// XPressnet Call Bytes.
// broadcast to everyone, we save the incoming data and process it later.
#define GENERAL_BROADCAST 0x160

// certain global XPressnet status indicators
#define csNormal 0x00 // Normal Operation Resumed ist eingeschaltet
#define csEmergencyStop 0x01 // Der Nothalt ist eingeschaltet
#define csTrackVoltageOff 0x02 // Die Gleisspannung ist abgeschaltet
#define csShortCircuit 0x04 // Kurzschluss
#define csServiceMode 0x20 // Der Programmiermodus ist aktiv - Service Mode

//XpressNet Befehl, jedes gesendete Byte
#define XNetlength	0		//L�nge
#define XNetmsg		1		//Message
#define XNetcom		2		//Kennung/Befehl
#define XNetdata1	3		//Databyte1
#define XNetdata2	4		//Databyte2
#define XNetdata3	5		//Databyte3
#define XNetdata4	6		//Databyte4
#define XNetdata5	7		//Databyte5

typedef struct	//Lokdaten	(Lok Events)
{
	uint8_t low;		// A7, A6, A5, A4, A3, A2, A1, A0
	uint8_t high;		// 0, 0, A13, A12, A11, A10, A9, A8 -> DFAA AAAA
	uint8_t mode;		//Kennung 0000 B0FF -> B=Busy(1), F=Fahrstufen (0=14, 1=27, 2=28, 3=128)
	uint8_t speed;		//0, Speed 0..127 (0x00 - 0x7F) -> 0SSS SSSS
	uint8_t f0;		//X X Dir F0 F4 F3 F2 F1			
	uint8_t f1;		//F12 F11 F10 F9 F8 F7 F6 F5	
	uint8_t f2;		//F20 F19 F18 F17 F16 F15 F14 F13
	uint8_t f3;		//F28 F27 F26 F25 F24 F23 F22 F21 
	uint8_t state;	//Zahl der Zugriffe
} XNetLok;

#define AccessoryMax 128                  //64 Weichen / 8 = 8 byte
uint8_t BasicAccessory[AccessoryMax / 8]; //Speicher f�r Weichenzust�nde

/* Slotliste Loks */
#define XSendMax 16			//Maximalanzahl der Datenpakete im Sendebuffer
//#define SlotMax 15			//Slots f�r Lokdaten
#define SlotInterval 200	//Zeitintervall zur Aktualisierung der Slots (ms)
#define XSendMaxData 8		//Anzahl Elemente im Datapaket Array XSend

//**************************************************************
//byte XNetUserOps = 0x00;
//byte XNetReturnLoco = 0x00;


/* Set default SwitchFormat for accessory use */
#ifndef SwitchFormat
#define SwitchFormat ROCO
#endif

typedef struct	//Antwort/Abfragespeicher
{
	uint8_t length;			//Speicher f�r Datenl�nge
	uint8_t data[XSendMaxData]; //zu sendende Daten
} XSend;


// library interface description
void xnetreceive(void);

void XpressNetsetPower(uint8_t Power);            //Zustand Gleisspannung Melden
void setHalt();																  //Zustand Halt Melden
bool getLocoInfo(uint16_t Addr);							  //Abfragen der Lokdaten (mit F0 bis F12)
bool getLocoFunc(uint16_t Addr);								  //Abfragen der Lok Funktionszust�nde F13 bis F28
bool setLocoHalt(uint16_t Addr);								  //Lok anhalten
bool setLocoDrive(uint16_t Addr, uint8_t Steps, uint8_t Speed); //Lokdaten setzten
//bool setLocoFunc(uint8_t Adr_High, uint8_t Adr_Low, uint8_t type, uint8_t fkt);	  //Lokfunktion setzten
//void getLocoStateFull(uint16_t addr, bool Anfrage);         //Gibt Zustand der Lok zur�ck.
bool getTrntInfo(uint8_t FAdr_High, uint8_t FAdr_Low);							  //Ermitteln der Schaltstellung einer Weiche
bool setTrntPos(uint8_t FAdr_High, uint8_t FAdr_Low, uint8_t Pos);					  //Schalten einer Weiche
//Programming:
void readCVMode(uint8_t CV);			  //Lesen der CV im CV-Mode
void writeCVMode(uint8_t CV, uint8_t Data); //Schreiben einer CV im CV-Mode
void getresultCV();					  //Programmierergebnis anfordern
//Slot:
void setFree(uint8_t Adr_High, uint8_t Adr_Low); //Lok aus Slot nehmen

//Variables:
bool XNetRun;					  //XpressNet ist aktiv
unsigned int myDirectedOps;			  // the address we look for when we are listening for ops
unsigned int myCallByteInquiry;   // the address we look for for our Call uint8_t Window
unsigned int myRequestAck;			  // the address for a request acknowlegement sent
uint8_t XNetMsg[8];					  //Daten wurden empfangen
bool DataReady;						  //Daten k�nnen verarbeitet werden
uint8_t XNetMsgTemp[8];				  //Serial receive (Length, Message, Command, Data1 to Data5)
bool ReadData;						  //Empfangene Serial Daten: (Speichern = true/Komplett = false)
void XNetget(void);					  //Empfangene Daten eintragen
XSend XNetSend[XSendMax];			  //Sendbuffer
//XNetLok xLokSts[SlotMax];			  //Speicher f�r aktive Lokzust�nde

uint16_t SlotLokUse[32]; //store loco to DirectedOps


//Functions:
void getXOR(uint8_t *data, uint8_t length);			   // calculate the XOR
uint8_t calluint8_tParity(uint8_t me);				   // calculate the parity bit
int USART_Receive(void);					   //Serial Empfangen
void USART_Transmit(uint8_t data8);			   //Serial Senden
void XNetclear(void);						   //Serial Nachricht zur�cksetzten

void sendSchaltinfo(bool schaltinfo, uint8_t data1, uint8_t data2); //Aufbereiten der Schaltinformation

void XNetclearSendBuf();										//Sendbuffer leeren

void XNetsend(uint8_t *dataString, uint8_t byteCount); //Sende Datenarray out NOW!
void XNetsendout(void);
bool XNetSendadd(uint8_t *dataString, uint8_t uint8_tCount); //Zum Sendebuffer Hinzuf�gen

//Adressrequest:
int ReqLocoAdr; //Adresse f�r die Lok Daten angefragt wurden
int ReqLocoAgain;
int ReqFktAdr; //Adresse f�r die F2 und F3 angefragt wurde

//SlotServer:
long SlotTime;															 //store last time the Slot ask
int SlotLast;															 //letzter bearbeiteter Slot
void UpdateBusySlot(void);												 //Fragt Zentrale nach aktuellen Zust�nden
void LokStsclear(void);												 //l�scht alle Slots
bool LokStsadd(uint16_t Addr, uint8_t Mode, uint8_t Speed, uint8_t FktSts); //Eintragen �nderung / neuer Slot XLok
bool LokStsFunc0(uint16_t Addr, uint8_t Func);						 //Eintragen �nderung / neuer Slot XFunc0
bool LokStsFunc1(uint16_t Addr, uint8_t Func1);						 //Eintragen �nderung / neuer Slot XFunc1
bool LokStsFunc23(uint16_t Addr, uint8_t Func2, uint8_t Func3);			 //Eintragen �nderung / neuer Slot XFunc23
bool LokStsBusy(uint8_t Slot);											 //Busy Bit Abfragen
void LokStsSetBusy(uint16_t Addr);								 //Lok Busy setzten
//uint8_t xLokStsgetSlot(uint8_t MSB, uint8_t LSB);								 //gibt Slot f�r Adresse zur�ck / erzeugt neuen Slot (0..126)
int LokStsgetAdr(uint8_t Slot);											 //gibt Lokadresse des Slot zur�ck, wenn 0x0000 dann keine Lok vorhanden
bool LokStsIsEmpty(uint8_t Slot);											 //pr�ft ob Datenpacket/Slot leer ist?
uint8_t getNextSlot(uint8_t Slot);											 //gibt n�chsten genutzten Slot

bool setBasicAccessoryPos(uint16_t address, bool state, bool activ);

//void notifyLokFunc(uint16_t Address, uint8_t F2, uint8_t F3);

//Spannung und GO/STOP Events:
uint8_t Railpower; //Gleisspannung

//Programming:
extern bool opsReadDirectCV(uint16_t CV); //Read Direct Mode Byte
extern bool opsProgramCV(uint16_t address, uint16_t CV, uint8_t CV_data);
extern bool opsPOMwriteBit(uint16_t address, uint16_t CV, uint8_t Bit_data);
extern bool opsPOMreadCV(uint16_t address, uint16_t CV);
//Lok Status:

//Funktionen

//Status LED:
int ledState;		 // ledState used to set the LED
long previousMillis; // will store last time LED was updated

//extern void notifyXNetDebug(String s) __attribute__((weak));
uint16_t Word(uint8_t h, uint8_t l);
void getXOR(unsigned char *data, uint8_t length);
void notifyXNetTrnt(uint16_t Address, uint8_t data);
void notifyCVNack();
extern void notifyXNetLocoDrive14(uint16_t Address, uint8_t Speed) __attribute__((weak));
extern void notifyXNetLocoDrive27(uint16_t Address, uint8_t Speed) __attribute__((weak));
extern void notifyXNetLocoDrive28(uint16_t Address, uint8_t Speed) __attribute__((weak));
extern void notifyXNetLocoDrive128(uint16_t Address, uint8_t Speed) __attribute__((weak));
extern void notifyXNetLocoFunc1(uint16_t Address, uint8_t Func1) __attribute__((weak)); //Gruppe1 0 0 0 F0 F4 F3 F2 F1
extern void notifyXNetLocoFunc2(uint16_t Address, uint8_t Func2) __attribute__((weak)); //Gruppe2 0000 F8 F7 F6 F5
extern void notifyXNetLocoFunc3(uint16_t Address, uint8_t Func3) __attribute__((weak)); //Gruppe3 0000 F12 F11 F10 F9
extern void notifyXNetLocoFunc4(uint16_t Address, uint8_t Func4) __attribute__((weak)); //Gruppe4 F20-F13
extern void notifyXNetLocoFunc5(uint16_t Address, uint8_t Func5) __attribute__((weak)); //Gruppe5 F28-F21
extern void notifyXNetStatus(uint8_t LedState) __attribute__((weak));
extern void notifyXNetVer(uint8_t V, uint8_t ID) __attribute__((weak));
extern void notifyXNetPower(uint8_t State) __attribute__((weak));
extern void notifyLokFunc(uint16_t Address, uint8_t F2, uint8_t F3) __attribute__((weak));
extern void notifyLokAll(uint8_t Adr_High, uint8_t Adr_Low, bool Busy, uint8_t Steps, uint8_t Speed, uint8_t Direction, uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3, bool Req) __attribute__((weak));
extern void notifyCVInfo(uint8_t State) __attribute__((weak));
extern void notifyCVResult(uint8_t cvAdr, uint8_t cvData) __attribute__((weak));
void notifyTrnt(uint16_t cvAdr, uint8_t Pos) __attribute__((weak));
extern void notifyFeedback(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) __attribute__((weak));

//	extern void notifyXNetData(unsigned int data, bool line) __attribute__((weak));



#endif

