/*
  p50x.h - library for p50x protocoll
  Copyright (c) 2013 Philipp Gahtow  All right reserved.
*/

// ensure this library description is only included once
#ifndef p50x_h
#define p50x_h

// include types & constants of Wiring core API
#if defined(WIRING)
 #include <Wiring.h>
#elif ARDUINO >= 100
 #include <Arduino.h>
#else
 #include <WProgram.h>
#endif

#include <EEPROM.h>

//check if the micro is supported
#if defined (__AVR_ATmega48__) || defined (__AVR_ATmega88__) || defined (__AVR_ATmega168__) || defined (__AVR_ATmega328__) || defined (__AVR_ATmega48P__) || defined (__AVR_ATmega88P__) || defined (__AVR_ATmega168P__) || defined (__AVR_ATmega328P__)
#define ATMEGAx8
#elif defined (__AVR_ATtiny25__) || defined (__AVR_ATtiny45__) || defined (__AVR_ATtiny85__)
#define ATTINYx5
#elif defined (__AVR_ATmega8__) || defined (__AVR_ATmega8A__)
#define ATMEGA8
#elif defined (__AVR_ATtiny24__) || defined (__AVR_ATtiny44__) || defined (__AVR_ATtiny84__)
#define ATTINYx4
#elif defined (__AVR_ATmega640__) || defined (__AVR_ATmega1280__) || defined (__AVR_ATmega1281__) || defined (__AVR_ATmega2560__) || defined (__AVR_ATmega2561__)
#define ATMEGAx0
#elif defined (__AVR_ATmega344__) || defined (__AVR_ATmega344P__) || defined (__AVR_ATmega644__) || defined (__AVR_ATmega644P__) || defined (__AVR_ATmega644PA__) || defined (__AVR_ATmega1284P__)
#define ATMEGAx4
#elif defined (__AVR_ATtiny2313__) || defined (__AVR_ATtiny4313__)
#define ATTINYx313
#elif defined (__AVR_ATmega32U4__)
#define ATMEGAxU
#else
#error Sorry, microcontroller not supported!
#endif

//Version Zentrale
#define Version 0x01
#define Subversion 0x05

/* p50x opcodes */

//Datenpakete P50Xb:
#define XHalt   0xA5  //Loks anhalten aber DCC bleibt aktiv
#define XPwrOff 0xA6  //DCC Hauptgleis ausschalten
#define XPwrOn  0xA7  //DCC Hauptgleis einschalten

//Mögliche Rückmeldungen:
#define OK      0x00  //OK, Befehl ausgeführt
#define XBADPRM 0x02  //Lokadresse außerhalb des Bereichs (1 .. 10239) (illegal parameter value)
#define XPWOFF  0x06  //abgeschaltet!
#define XNODATA 0x0A  //Die Lok ist nicht im Refreshbuffer
#define XNOSLOT 0x0B  //Queues sind voll, Kommando wurde verworfen.
#define XBADTNP 0x0E  //Error: illegal Turnout address for this protocol
#define XLOWTSP 0x40  //Die Queue ist fast voll
#define XBUSY   0x80  //busy, command ignored

/* Rückmeldung S88 */
#define FBMax 127		//Anzahl Rückmelder je 2 Byte (2032 = 127 Module(16x) )
#define FBMaxChange 10	//Anzahl Einträge Change-Flages
typedef struct
{
	uint8_t	modul;		//1..127 oder Null = leer
	uint16_t data;		//neuer Zustand
}	FBChangeMsg;

/*Handregler Loks*/
#define TrottleMax 2	//nicht größer als SlotMax!
/* Slotliste Loks */
#define SlotMax 15		//Slots für Lokdaten

//p50X Befehl, jedes gesendete Byte
#define p50length	0		//Länge
#define p50msg		1		//p50 = 0x78
#define p50command	2		//Command/Befehl
#define p50Lowbyte	3		//AdrLowByte
#define p50Highbyte	4		//AdrHighByte
#define p50data1	5		//Databyte1
#define p50data2	6		//Databyte2
#define p50data3	7		//Databyte3

typedef struct		//Rückmeldung des Status der Programmierung
{
	bool Event;		//Programm Track Event
	uint8_t command;	//1. Byte, falls 0xF5: noch nicht fertig, es gibt nichts zu melden; falls [1,2,3,4,6] Zahl der Folgebytes
	uint8_t status;		//Status der Programming Task
	uint8_t byte2;		//(nur falls vom 1. Byte angekündigt) Inhalt CV oder low byte Adresse
	uint8_t byte3;		//(nur falls vom 1. Byte angekündigt) high byte Adresse
/*	uint8_t byte4;		//(nur falls vom 1. Byte angekündigt)
	uint8_t byte5;		//(nur falls vom 1. Byte angekündigt)
	uint8_t byte6;		//(nur falls vom 1. Byte angekündigt)
*/
} p50xPTMsg;


typedef struct	//Lokdaten	(Lok Events)
{
	uint8_t low;		// A7, A6, A5, A4, A3, A2, A1, A0
	uint8_t high;		//Dir, FL, A13, A12, A11, A10, A9, A8
	uint8_t speed;		//Speed 0..127 (0x00 - 0x7F)
	uint8_t func;		//F1..F8 (bit #0..7)
	uint8_t func2;
	uint8_t func3;
	uint8_t func4;
	uint8_t state;	//Zahl der Zugriffe
} p50xLok;

/*
typedef struct	//Weichendaten	(Trnt Events) or 88 Daten (Sensor Events)
{
	uint8_t low;		//(1) address low or Kontakte 1..8 dieses Moduls (Bits 7..0)
	uint8_t high;		//(2) address high + 'color' (1 = closed (green), 0 = thrown (red)) or Kontakte 9..16 dieses Moduls (Bits 15 ..8)
} p50xTrntSen;
*/

// library interface description
class p50xClass
{
  // user-accessible "public" interface
  public:
    p50xClass(void);	//Constuctor
	void setup(long baud = 19200);  //Initialisierung Serial
	bool FBsend (int Adr, bool State);	//Rückmeder Senden
	void receive(void);				//Prüfe ob p50 Packet vorhanden und werte es aus.
	void setPower(bool Power);		//Zustand Gleisspannung Melden
	void setHalt();						//Zustand Halt Melden
	void ReturnPT(byte CV);			//Rückmeldung CV vom PT
	bool xLokStsUpdate();		//löst notifyLokRequest für nächsten aktiven Lok/Slot aus
	void xLokStsUpdateClear (void);		//löscht Refresh circle
	bool xLokStsTrottle (uint16_t Address, uint8_t Speed, uint8_t Direction, uint8_t F0);	//Handregler Änderung weitermelden
	bool xLokStsTrottleFunc (uint16_t Address, uint8_t Func);	//Handregler Änderung weitermelden

	// public only for easy access by interrupt handlers
	static inline void handle_interrupt();		//Serial Interrupt bearbeiten

  // library-accessible "private" interface
  private:
	//Serial:
	static p50xClass *active_object;	//aktuelle aktive Object 
	byte p50xMsg[8];		//Serial receive: Länge, p50 = 0x78, Command/Befehl, AdrLowByte, AdrHighByte, Databyte1, Databyte2, Databyte3
	void send (byte out);		//Serial Senden
	byte USART_Receive(void);		//Serial Empfangen
	void p50get(void);				//Serial Nachricht Speichen
	void p50clear(void);		//Serial Nachricht zurücksetzten

	//S88 Feedback:
	uint16_t FBS88[FBMax];  //Rückmeldedaten von allen 2048 Rückmeldern
	FBChangeMsg FBChange[FBMaxChange];  //letzte Änderungen
	bool FBS88Event;		//Rückmeldedaten eingetroffen
	void FBS88get (void);		//XEvtSen Change Flags auslesen und Rückgeben
	void FBS88get (byte Modul);	//XSensor Modulstatus Rückgabe
	void FBS88clear (void);		//XSensOff Löscht alle S88-Bits auf Null und löscht alle Change-Flags.

	//Spannung und GO/STOP Events:
	bool PWREvent;	//Spannungsänderung
	bool Railpower;	//Gleisspannung
	bool Go;		//Gleisspannung EIN	
	bool Stop;	//Gleisspannung AUS
	bool Halt;	//Loks Halt

	//Programming:
	p50xPTMsg PTMsg;	//Programm Track Daten
	bool PTMode;		//Programmiermodus EIN/AUS

	//Lok Status:
	bool xLokEvent;			//da war mind. ein Zugriff auf eine Lok
	p50xLok xLokSts[SlotMax];	//Slotverwaltung für Loks
	int xLokTrottle[TrottleMax];	//Adressspeicher Lokadressen Handregler (Datenupdate)
	void xLokStsclear (void);	//löscht alle Slots
	void xLokStsEvt (void);		//XEvtLok Packet bestimmen und Senden
	bool xLokStsadd (byte MSB, byte LSB, byte Speed, byte FktSts);	//Eintragen Änderung / neuer Slot XLok
	bool xLokStsFunc (byte MSB, byte LSB, byte Func);	//Eintragen Änderung / neuer Slot XFunc
	bool xLokStsFunc2 (byte MSB, byte LSB, byte Func2);	//Eintragen Änderung / neuer Slot XFunc2
	bool xLokStsFunc34 (byte MSB, byte LSB, byte Func3, byte Func4);	//Eintragen Änderung / neuer Slot XFunc34
	byte xLokStsgetSlot (byte MSB, byte LSB);		//gibt Slot für Adresse zurück / erzeugt neuen Slot (1..127)
	int xLokStsgetAdr (byte Slot);					//gibt Lokadresse des Slot zurück, wenn 0x0000 dann keine Lok vorhanden
	byte xLokStsaktSlot;				//gesendeter letzter aktiver Slot
	byte xLokStsrepeat;					//Anzahl der Updatewiederholungen (0 = Endlos)
	bool xLokStsIsEmpty (byte Slot);	//prüft ob Datenpacket leer ist
	void xLokStsSetNew (byte Slot, byte MSB, byte LSB);	//Neue Lok eintragen mit Adresse
	
	//Status LED:
	int ledState;             // ledState used to set the LED
	long previousMillis;        // will store last time LED was updated
};

	extern void notifyRS232( uint8_t State ) __attribute__ ((weak));

	extern void notifyLokRequest( uint16_t Address, uint8_t Speed, uint8_t Direction, uint8_t F0, uint8_t repeat ) __attribute__ ((weak));
	extern void notifyLokF1F4Request( uint16_t Address, uint8_t Function, uint8_t repeat ) __attribute__ ((weak));
	extern void notifyLokFuncRequest( uint16_t Address, uint8_t Function, uint8_t repeat ) __attribute__ ((weak));
	extern void notifyLokFunc2Request( uint16_t Address, uint8_t Function2, uint8_t repeat ) __attribute__ ((weak));
	extern void notifyLokFunc34Request( uint16_t Address, uint8_t Function3, uint8_t Function4, uint8_t repeat ) __attribute__ ((weak));
	extern void notifyTrntRequest( uint16_t Address, uint8_t State, uint8_t Direction, uint8_t Lock ) __attribute__ ((weak));

//Programming:
	//Direct-Modus
	extern void notifyXPT_DCCRDRequest( uint16_t CVAddress ) __attribute__ ((weak));	//lesen
	extern void notifyXPT_DCCWDRequest( uint16_t CVAddress, uint8_t Value ) __attribute__ ((weak));		//schreiben
	//Bit-Modes
	extern void notifyXPT_DCCRBRequest( uint16_t CVAddress ) __attribute__ ((weak));	//lesen
	extern void notifyXPT_DCCWBRequest( uint16_t CVAddress, uint8_t Bit, uint8_t Value ) __attribute__ ((weak));	//schreiben
	//Paged Mode
	extern void notifyXPT_DCCRPRequest( uint16_t CVAddress ) __attribute__ ((weak));	//lesen
	extern void notifyXPT_DCCWPRequest( uint16_t CVAddress, uint8_t Value ) __attribute__ ((weak));		//schreiben
	//PoM
	extern void notifyXDCC_PDRRequest( uint16_t Address, uint16_t CVAddress,  uint8_t repeat ) __attribute__ ((weak));	//lesen
	extern void notifyXDCC_PDRequest( uint16_t Address, uint16_t CVAddress, uint8_t Value,  uint8_t repeat ) __attribute__ ((weak));		//schreiben
	//Abbruch
	extern byte notifyXPT_TermRequest() __attribute__ ((weak));	//Abbruch Programmierung, Fehlercode/OK Rückgabe

	extern void notifyPowerRequest( uint8_t Power ) __attribute__ ((weak));

#endif

