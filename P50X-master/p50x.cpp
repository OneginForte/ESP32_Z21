/*
*****************************************************************************
  *		p50x.h - library for p50x protocoll
  *		Copyright (c) 2013 Philipp Gahtow  All right reserved.
  *
  *		Portions Copyright (C) modeltreno
  *
*****************************************************************************
  * IMPORTANT:
  * 
  * 	Some of the message formats used in this code are Copyright Digitrax, Inc.
  * 	and are used with permission as part of the EmbeddedLocoNet project. That
  * 	permission does not extend to uses in other software products. If you wish
  * 	to use this code, algorithm or these message formats outside of
  * 	EmbeddedLocoNet, please contact Digitrax Inc, for specific permission.
  * 
  * 	Note: The sale any LocoNet device hardware (including bare PCB's) that
  * 	uses this or any other LocoNet software, requires testing and certification
  * 	by Digitrax Inc. and will be subject to a licensing agreement.
  * 
  * 	Please contact Digitrax Inc. for details.
*****************************************************************************
*/

// include this library's description file
#include "p50x.h"
#include <avr/interrupt.h>

#define interval 750           // interval Status LED (milliseconds)

#define SendRepeat EEPROM.read(20)		//Wiederholung bei der Sendung neuer Datenpakete = 3
#define UpdateRepeat 1				//Wiederholung bei erneutem Senden = 1

p50xClass *p50xClass::active_object = 0;	//Static

// Constructor /////////////////////////////////////////////////////////////////
// Function that handles the creation and setup of instances

p50xClass::p50xClass()
{
	// initialize this instance's variables for feedback
	FBS88clear();	//löscht alle gespeichterten Zustände
	
	// do whatever is required to initialize the library
	PWREvent = false;	//Spannungsänderung
	Railpower = false;	//Keine Gleisspannung
	Go = false;		//Gleisspannung EIN
	Stop = false;	//Gleisspannung AUS
	Halt = false;	//Loks anhalten
	PTMsg.Event = false;	//Keine Programm Track Event
	PTMode = false;		//Programmiermodus AUS

	xLokEvent = false;			//da war mind. ein Zugriff auf eine Lok
	xLokStsclear();	//löscht alle Slots
	xLokStsaktSlot = 0x00;		//Für Datenwiederholung, aktuell gesendeter Slot -> keiner
	xLokStsrepeat = 0x00;	  //Anzahl Wiederholungen für Updates -> 0 = Endlos

	if (EEPROM.read(0) == 0xFF && EEPROM.read(5) == 0xFF) {
		EEPROM.write(0, Version);
		EEPROM.write(5, Version);
		EEPROM.write(1,3);	//	19200 Baud (default)	
		EEPROM.write(2,0);	// OpenDCC Standradversion
		EEPROM.write(6,0);  //255 = CTS is unused
		EEPROM.write(7,1);	//normaler S88-Betrieb mit Einlesen der externen Rückmelder
		EEPROM.write(8,128);	//S88 Autoread, Es können max. 128 Bytes eingestellt werden. 
		EEPROM.write(9,2);	//S88 Module 1, default: 2
		EEPROM.write(10,2);	//S88 Module 2, default: 2
		EEPROM.write(11,2);	//S88 Module 3, default: 2
		EEPROM.write(12,1); //Invertiere Weichenbefehl, default: 1 = Vertausche rot/grün bei Lenz oder auf dem Xpressnet
		EEPROM.write(13,2); //Weichenbefehl wird bei der Gleisausgabe sooft wiederholt. Voreinstellung: 2 
		EEPROM.write(14,100); //Weichenschaltzeit, Voreinstellung: 100 
		EEPROM.write(15,0); //Einschaltzustand, default: 0 = Normalmode (P50/P50X mixed)
		EEPROM.write(16,0); //Offset für Weichenrückmeldung, Voreinstellung: 0 
		EEPROM.write(17,0); //Weichenrückmeldung Mode (Nur SO17 = 0,1 oder 2 erlaubt.)
		EEPROM.write(18,3); //Extended Resets: Es werden um die angegebene Zahl mehr Reset-Packets gesendet. Voreinstellung: 3 
		EEPROM.write(19,3); //Extended Program Commands: Es werden um die angegebene Zahl mehr Programmierpakete gesendet. Voreinstellung: 3 
		EEPROM.write(20,3);	//PoM Wiederholung, Voreinstellung 3. (lt. NMRA mindestens zweimal) 
		EEPROM.write(21,3);	//Geschwindigkeitsbefehle Wiederholung, default: 3
		EEPROM.write(22,0); //Funktionsbefehl Wiederholung, default: 0
		EEPROM.write(24,3); //Voreinstellung DCC Format mit 128 Fahrstufen (bzw. 126, um genau zu sein), default: 3
		EEPROM.write(25,0); //Voreinstellung BiDi (railcom) - Kein Cutout (default) 
		EEPROM.write(26,0); //Voreinstellung Fast Clock - Es wird kein DCC FAST CLOCK erzeugt
		EEPROM.write(30,2); //Timing S88, Voreinstellung 2, also 20µs High und 20µs Low.
		EEPROM.write(31,0); //Anzahl Weichenrückmelder im S88, default: 0
		EEPROM.write(32,0); //Summen-Anzahl der Melderbits S88, default: 0 
		EEPROM.write(34,6);	//Abschaltzeit bei Kurzschluß Hauptgleis, default: 6
		EEPROM.write(35,8); //Abschaltzeit bei Kurzschluß Programmiergleis, default: 8
		EEPROM.write(36,0); //Externe Abschaltung erlaubt, default: 0
		EEPROM.write(37,6); //Totzeit der externen Abschaltung nach Start, default: 6
		EEPROM.write(39,1); //Seriennummer, default: 1
/*		for (int i = 40; i < 1023; i++)  {	//Lokdaten bei OpenDCC - keine
			if (EEPROM.read(i) != 0)
				EEPROM.write(i,0);
		}
*/
	}
}

//******************************************Serial*******************************************
void p50xClass::setup(long baud /* = 19200 */)  //Initialisierung Serial
{
	ledState = HIGH;       // Status LED, used to set the LED
	previousMillis = millis();   // will store last time LED was updated
	if (notifyRS232)
		notifyRS232 (ledState);

	p50clear();		//Nachricht zurücksetzen

	//Set up on 19200 Baud
	cli();  //disable interrupts while initializing the USART
	#ifdef __AVR_ATmega8__
		UBRRH = ((F_CPU / 16 + baud / 2) / baud - 1) >> 8;
		UBRRL = ((F_CPU / 16 + baud / 2) / baud - 1);
		UCSRA = 0;
		UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE) | (1 << UCSZ2);
		UCSRC = (1 << UCSZ1) | (1 << UCSZ0);
	#else
		UBRR0H = ((F_CPU / 16 + baud / 2) / baud - 1) >> 8;;
		UBRR0L = ((F_CPU / 16 + baud / 2) / baud - 1);
		UCSR0A = 0;
		UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0) | (1 << UCSZ02);
		UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
	#endif
	
	sei(); // Enable the Global Interrupt Enable flag so that interrupts can be processed 
	/*
	*  Enable reception (RXEN = 1).
	*  Enable transmission (TXEN0 = 1).
	*	Enable Receive Interrupt (RXCIE = 1).
	*  Set 8-bit character mode (UCSZ00, UCSZ01, and UCSZ02 together control this,
	*  But UCSZ00, UCSZ01 are in Register UCSR0C).
	*/

	active_object = this;		//hold Object to call it back in ISR
}

//Byte via Serial senden
void p50xClass::send(byte out)
{
	#ifdef __AVR_ATmega8__
		while (!(UCSRA & (1 << UDRE))) {}
		// put the data into buffer, and send 
		UDR = out;
	#else
		while (!(UCSR0A & (1 << UDRE0))) {}
		// put the data into buffer, and send 
		UDR0 = out;
	#endif

	p50clear();		//Nachricht zurücksetzen
	
	//Daten, setzte LED = ON!
	if (ledState == LOW) { 
		ledState = HIGH;
		if (notifyRS232)
			notifyRS232 (ledState);
	}
	previousMillis = millis();		//Zeit merken an der Daten empfangen und verarbeitet wurden
}

//--------------------------------------------------------------------------------------------
byte p50xClass::USART_Receive(void) {
	unsigned char status, resh, resl;
	// Wait for data to be received
	#ifdef __AVR_ATmega8__
		status = UCSRA;
		while (!(status & (1 << RXC))) { return -1; }//status = UCSRA;}

		resl = UDR;

		// If error, return -1 
		if (status & ((1 << FE) | (1 << DOR) | (1 << PE))) { return -1; }

	#else
		status = UCSR0A;
		while (!(status & (1 << RXC0))) { return -1; }//status = UCSR0A;}

		resl = UDR0;

		// If error, return -1
		if (status & ((1 << FE0) | (1 << DOR0) | (1 << UPE0))) { return -1; }
	#endif

	return resl;
}

//--------------------------------------------------------------------------------------------
//Interrupt routine for reading via Serial
#ifdef __AVR_ATmega8__
ISR(USART_RX_vect)  {
	p50xClass::handle_interrupt();	 //weiterreichen an die Funktion
}
#else
ISR(USART0_RX_vect) {
	p50xClass::handle_interrupt();	 //weiterreichen an die Funktion
}
#endif

// Interrupt handling
/* static */
inline void p50xClass::handle_interrupt()
{
	if (active_object)
	{
		active_object->p50get();	//Daten Einlesen und Speichern
	}
}
//--------------------------------------------------------------------------------------------

//Serial einlesen:
void p50xClass::p50get() 
{
	byte rxdata = USART_Receive();

	if (rxdata != -1) {		//Daten wurden korrekt empfangen?
		p50xMsg[p50length]++;	//neue Nachricht
		p50xMsg[p50xMsg[p50length]] = rxdata;
	}
}

// Public Methods //////////////////////////////////////////////////////////////
// Functions available in Wiring sketches, this library, and other libraries

//*******************************************************************************************
//Rückmeldung Senden
bool p50xClass::FBsend (int Adr, bool State) 
{    
	if (Adr > 0 && Adr <= (16*FBMax)) {
	  byte ModulNr = ((Adr-1) / 16) + 1;  //Modulnummer (1..127)
      byte Kontakt = (Adr-1) % 16;
	  for (int i = 0; i < FBMaxChange; i++) {
		  if (FBChange[i].modul == 0) {			//Modul noch nicht in der Liste!
			  FBChange[i].modul = ModulNr;
			  FBChange[i].data = FBS88[ModulNr-1];
			  if (Kontakt < 8)
			     bitWrite(FBChange[i].data, 7-Kontakt, State);          //Kontakte 1..8 dieses Moduls (Bits 7..0)
			  else bitWrite(FBChange[i].data, 8 + (7- (Kontakt-8)), State);   //Kontakte 9..16 dieses Moduls (Bits 15 ..8)
			  FBS88Event = true;	//Rückmeldedaten eingetroffen = JA
			  return true;
			  break;
		  }
		  else if (FBChange[i].modul == ModulNr) {  //Modul schon mit Change Flag vorhanden!
			  bool getState;
			  if (Kontakt < 8)
			     getState = bitRead(FBS88[ModulNr-1], 7-Kontakt);          //Kontakte 1..8 dieses Moduls (Bits 7..0)
			  else getState = bitRead(FBS88[ModulNr-1], 8 + (7- (Kontakt-8)));   //Kontakte 9..16 dieses Moduls (Bits 15 ..8)
			  if (getState == State) {
				  if (Kontakt < 8)
					getState = bitRead(FBChange[i].data, 7-Kontakt);          //Kontakte 1..8 dieses Moduls (Bits 7..0)
				  else getState = bitRead(FBChange[i].data, 8 + (7- (Kontakt-8)));   //Kontakte 9..16 dieses Moduls (Bits 15 ..8)
				  if (getState != State) {
					for (int j = (i+1); j < FBMaxChange; j++) {
						  if (FBChange[j].modul == 0) {
							FBChange[j].modul = ModulNr;
							FBChange[j].data = FBChange[i].data;
							if (Kontakt < 8)
								bitWrite(FBChange[j].data, 7-Kontakt, State);          //Kontakte 1..8 dieses Moduls (Bits 7..0)
							else bitWrite(FBChange[j].data, 8 + (7- (Kontakt-8)), State);   //Kontakte 9..16 dieses Moduls (Bits 15 ..8)
							FBS88Event = true;	//Rückmeldedaten eingetroffen = JA
							return true;
							break;
						}
					}
				  }
				  else break;
			  }
			  else {	//Status unterscheidet sich zu dem vorhergehenden	
				if (Kontakt < 8)
					bitWrite(FBChange[i].data, 7-Kontakt, State);          //Kontakte 1..8 dieses Moduls (Bits 7..0)
				else bitWrite(FBChange[i].data, 8 + (7- (Kontakt-8)), State);   //Kontakte 9..16 dieses Moduls (Bits 15 ..8)
				FBS88Event = true;	//Rückmeldedaten eingetroffen = JA
				return true;
				break;
			  }
		  }
	  }
	}
	return false;		//Fehler, Adresse nicht im Bereich oder Datenüberlauf
}

//*******************************************************************************************
//Daten ermitteln und Auswerten
void p50xClass::receive(void) 
{
  if (true) { //p50get()) {	//Datenbyte von Serial lesen
	int Adr = 0;
	byte Byte = 0;
    //Prüfen ob P50X aktiv?
    if ((p50xMsg[p50msg] == 0x78 || p50xMsg[p50msg] == 0x58) && p50xMsg[p50command] >= 0x80 && p50xMsg[p50length] > 1) {  ////P50Xb-Kommandos (Binary-Commands) - erweitertes Command-Set
		  if (p50xMsg[p50length] >= 4)
			  Adr = (p50xMsg[p50Highbyte] << 8) | p50xMsg[p50Lowbyte];	//Adresse der Daten bestimmen
		  switch (p50xMsg[p50command]) {		
			  case 0xA0: ////printStatus(" XVer");    //Versionsabfrage
						 send(0x02);  //Länge 2
						 send(Version);  //Version
						 send(Subversion);  //Subversion
						 send(0x01);
						 send(EEPROM.read(39));	//Seriennummer
						 send(OK);  //Ende der Liste
						 break;                                
			  case 0xA2: //printStatus(" XStatus");  //Statusabfrage
						 bitWrite(Byte,7,0);  //Erweiterungsbit
						 bitWrite(Byte,6,1);  //Spannungsregelung
						 bitWrite(Byte,5,0);  //externes I2C
						 bitWrite(Byte,4,Halt);  //HALT
						 bitWrite(Byte,3,Railpower);  //PWR
						 bitWrite(Byte,2,0);  //HOT
						 bitWrite(Byte,1,Go);  //GO
						 bitWrite(Byte,0,Stop);  //STOP
						 send(Byte);
						 Go = false;
						 Stop = false;
						 Halt = false;
						 PWREvent = false;	//Spannungsänderung
						 send(OK);  //(Kommando okay)
						 break;
			  case 0xC9: //printStatus(" XEvtLok");    //Es werden alle seit der letzten Abfrage veränderten Loks (z.B. per Handregler) gemeldet.
						 xLokStsEvt();	//Slots Rückmelden
//						 send(0x80);  //= nichts zu melden, Keine Lok
						 break;
			  case 0xC4: //printStatus(" XNop");    //XNop (nichts tun)
						 send(OK);  //(Kommando okay)
						 break;                                
			  case 0xCA: //printStatus(" XEvtTrnt");    //Weichenstatus melden, max. 64 Weichen werden gemeldet
						 send(OK);
						 break;
			  case 0xCB: //printStatus(" XEvtSen");    //Das Vorliegen eines S88-Event wird mit XEvent gemeldet
						 FBS88get();    //Rückmelderegister auswerten
						 break;
			  case 0xC8: //printStatus(" XEvent");      // Statusabfrage für Zustandsänderungen, wie z.B. S88, Programmierung usw.
/*		 Byte 1:
           Bit 7:  Erweiterungsbit, wenn 1, kommt noch ein weiteres Byte als Antwort.
           Bit 6:  LSY   1: da war ein Lissy Event
           Bit 5:  TRNT  1: da war mindestens ein Weichen-Event (**)
           Bit 4:  Tres  1: da war mindestens ein Zugriff auf eine reservierte Weiche
           Bit 3:  PWR   1: da war mind. ein Power Off Event (*)
           Bit 2:  S88   1: da war mind. ein s88-Event (*)
           Bit 1:  GO    1: da war ein IR-Event
           Bit 0:  LOK   1: da war mind. ein Zugriff auf eine Lok (**)
         Byte 2:
           Bit 7:  Erweiterungsbit, wenn 1, kommt noch ein weiteres Byte als Antwort.
           Bit 6:  Sts:  1: Aenderung im Status
           Bit 5:  Hot   1: Temperaturanzeige
           Bit 4:  PTsh  1: Schluss vom Programmiergleis zum Hauptgleis
           Bit 3:  RSsh  1: Schluss auf dem Programmiergleis oder auf dem Boosteranschluß
           Bit 2:  IntSh 1: Kurzschluss intern
           Bit 1:  LMSh  1: Kurzschluss auf den Lokmausanschluß
           Bit 0:  ExtSh 1: Kurzschlussmeldung eines externen Boosters
         Byte 3:
           Bit 7:  Erweiterungsbit, wenn 1, kommt noch ein weiteres Byte als Antwort.
           Bit 6:  EvBiDi: ( Neu: BiDi: Orts- oder Geschwindigkeitsrückmeldung eingetroffen, Abfrage mit XEvtBiDi)
           Bit 5:  BiDiCV: ( Neu: BiDi: CV-Meldung eingetroffen, Abfrage mit XEvtPT)**
           Bit 4:  ExVlt 1: Fremdspannung vorhanden
           Bit 3:  TkRel 1: Uebernahme einer Lok durch andere Controller
           Bit 2:  Mem   1: Memory Event
           Bit 1:  RSOF  1: RS232 Overflow
           Bit 0:  PT    1: Programming Track Event (*)		*/
						 //Byte1:
						 bitWrite(Byte, 7, 1);  //Erweiterungsbit, wenn 1, kommt noch ein weiteres Byte als Antwort
						 bitWrite(Byte, 6, 0);	 //da war ein Lissy Event
						 bitWrite(Byte, 5, 0);	 //da war mindestens ein Weichen-Event
						 bitWrite(Byte, 4, 0);	 //da war mindestens ein Zugriff auf eine reservierte Weiche
						 bitWrite(Byte, 3, PWREvent);	 //da war mind. ein Power Off Event
						 bitWrite(Byte, 2, FBS88Event);	 //da war mind. ein s88-Event
						 bitWrite(Byte, 1, 0);	 //da war ein IR-Event
						 bitWrite(Byte, 0, 0);	 //da war mind. ein Zugriff auf eine Lok
						 send(Byte);	//Byte1
						 //Byte2:
						 bitWrite(Byte, 7, 1);  //Erweiterungsbit, wenn 1, kommt noch ein weiteres Byte als Antwort
						 bitWrite(Byte, 6, 0);	 //Aenderung im Status
						 bitWrite(Byte, 5, 0);	 //Temperaturanzeige
						 bitWrite(Byte, 4, 0);	 //Schluss vom Programmiergleis zum Hauptgleis
						 bitWrite(Byte, 3, 0);	 //Schluss auf dem Programmiergleis oder auf dem Boosteranschluß
						 bitWrite(Byte, 2, 0);	 //Kurzschluss intern
						 bitWrite(Byte, 1, 0);	 //Kurzschluss auf den Lokmausanschluß
						 bitWrite(Byte, 0, 0);	 //Kurzschlussmeldung eines externen Boosters
						 send(Byte);	//Byte2
						 //Byte3:
						 bitWrite(Byte, 7, 0);  //Erweiterungsbit, wenn 1, kommt noch ein weiteres Byte als Antwort
						 bitWrite(Byte, 6, 0);	 //( Neu: BiDi: Orts- oder Geschwindigkeitsrückmeldung eingetroffen, Abfrage mit XEvtBiDi)
						 bitWrite(Byte, 5, PTMsg.Event);	 //( Neu: BiDi: CV-Meldung eingetroffen, Abfrage mit XEvtPT)
						 bitWrite(Byte, 4, 0);	 //Fremdspannung vorhanden
						 bitWrite(Byte, 3, 0);	 //Uebernahme einer Lok durch andere Controller
						 bitWrite(Byte, 2, 0);	 //Memory Event
						 bitWrite(Byte, 1, 0);	 //RS232 Overflow
						 bitWrite(Byte, 0, PTMsg.Event);	 //Programming Track Event
						 send(Byte);	//Byte3
						 break;   
			  case 0x80: if (p50xMsg[p50length] >= 6) {		//"X" + 1 + 4 Byte Befehl
							//printStatus(" XLok ");	
							if (xLokStsadd (p50xMsg[p50Highbyte], p50xMsg[p50Lowbyte], p50xMsg[p50data1], p50xMsg[p50data2])) {  //(byte MSB, byte LSB, byte Speed, byte FktSts)
								if (notifyLokRequest)
									notifyLokRequest( Adr, p50xMsg[p50data1], bitRead(p50xMsg[p50data2],5), bitRead(p50xMsg[p50data2],4), SendRepeat );	//Speed, Fkt
								if (bitRead(p50xMsg[p50data2],7) == 1) {	//F4 .. F1 als Funktionsbits übernehmen
									if (notifyLokF1F4Request)
										notifyLokF1F4Request( Adr, p50xMsg[p50data2] & B00001111, SendRepeat );
								}
							}
							send(OK);
						 }
						 break;
			  case 0x81: if (p50xMsg[p50length] >= 6) {		//"X" + 1 + 4 Byte Befehl
							//printStatus(" XLokX ");  //Dies ist eine Erweiterung von TAMS. Ist fast identisch zu XLok, jedoch mit echter Decoderspeed.
							if (xLokStsadd (p50xMsg[p50Highbyte], p50xMsg[p50Lowbyte], p50xMsg[p50data1], p50xMsg[p50data2])) {  //(byte MSB, byte LSB, byte Speed, byte FktSts)
								if (notifyLokRequest)
									notifyLokRequest( Adr, p50xMsg[p50data1], bitRead(p50xMsg[p50data2],5), bitRead(p50xMsg[p50data2],4), SendRepeat );	//Speed, Fkt
								if (bitRead(p50xMsg[p50data2],7) == 1) {	//F4 .. F1 als Funktionsbits übernehmen
									if (notifyLokF1F4Request)
										notifyLokF1F4Request( Adr, p50xMsg[p50data2] & B00001111, SendRepeat );
								}
							}
							send(OK);
						 }
						 break;
			  case 0x84: if (p50xMsg[p50length] >= 4) {		//"X" + 1 + 2 Byte Befehl 
							//printStatus(" XLokSts");
							send(XNODATA);  //Die Lok ist nicht im Refreshbuffer
						 }
						 break;
			  case 0x88: if (p50xMsg[p50length] >= 5) {		//"X" + 1 + 3 Byte Befehl 
							//printStatus(" XFunc ");
							if (notifyLokFuncRequest && xLokStsFunc (p50xMsg[p50Highbyte], p50xMsg[p50Lowbyte], p50xMsg[p50data1]))	//(byte MSB, byte LSB, byte Func)	Eintragen Änderung / neuer Slot XFunc
								notifyLokFuncRequest( Adr, p50xMsg[p50data1], SendRepeat);
							send(OK);    //0x00: OK, Befehl ausgeführt, 0x02: Lokadresse außerhalb des Bereichs, 0x0B: Queues sind voll
						 }
						 break;
			  case 0x89: if (p50xMsg[p50length] >= 5) {		//"X" + 1 + 3 Byte Befehl 
							//printStatus(" XFunc2 ");
							if (notifyLokFunc2Request && xLokStsFunc2 (p50xMsg[p50Highbyte], p50xMsg[p50Lowbyte], p50xMsg[p50data1]))	//(byte MSB, byte LSB, byte Func2 = F16-F9)
								notifyLokFunc2Request( Adr, p50xMsg[p50data1], SendRepeat );
							send(OK);    //0x00: OK, Befehl ausgeführt, 0x02: Lokadresse außerhalb des Bereichs, 0x0B: Queues sind voll
						 }
						 break;
			  case 0x8A: if (p50xMsg[p50length] >= 6) {		//"X" + 1 + 4 Byte Befehl 
							//printStatus(" XFunc34 ");
							if (notifyLokFunc34Request && xLokStsFunc34 (p50xMsg[p50Highbyte], p50xMsg[p50Lowbyte], p50xMsg[p50data1], p50xMsg[p50data2])) //(byte MSB, byte LSB, byte Func3 = F24-F17, byte Func4 = F28-F25)
								notifyLokFunc34Request( Adr, p50xMsg[p50data1], p50xMsg[p50data2], SendRepeat );
							send(OK);    //0x00: OK, Befehl ausgeführt, 0x02: Lokadresse außerhalb des Bereichs, 0x0B: Queues sind voll
						 }
						 break;            
			  case 0x90: if (p50xMsg[p50length] >= 4) {		//"X" + 1 + 2 Byte Befehl 
							//printStatus(" XTrnt ");    //= turnout bzw. Schaltbefehl
							Adr = ((p50xMsg[p50Highbyte] & 0x0F) << 8) | p50xMsg[p50Lowbyte]; 
						 //Bit 7 = Color:   1 = closed (grün), 0 = thrown (rot)
						 //Bit 6 = Sts:     Spulenzustand: (1 = ein, 0 = aus)
						 //Bit 5 = Res:     Hier könnte man eine Weiche verriegeln
							if(notifyTrntRequest)
								notifyTrntRequest( Adr, bitRead(p50xMsg[p50Highbyte],7), bitRead(p50xMsg[p50Highbyte],6), bitRead(p50xMsg[p50Highbyte],5) );
							send(OK);  //0x00: OK, Befehl ausgeführt, 0x06: abgeschaltet!, 0x02: Weichenadresse außerhalb des Bereichs,  0x40: Die Queue ist fast voll
						 }
						 break;
			  case 0x93: //printStatus(" XTrntFree");  //Nicht unterstützt: es gibt keine Weichenreservierung
						 send(OK);    //accepted
						 break;
			  case 0x94: //printStatus(" XTrntSts");  //(= turnout bzw. Schaltbefehl - Status Abfrage) 
						 send(XBADPRM);    //illegal parameter value
						 break;
			  case 0x95: //printStatus(" XTrntGrp");  //(= turnout bzw. Schaltbefehl - Status Abfrage, gruppenweise)
						 send(XBADPRM);    //illegal parameter value
						 break;
			  case 0x98: if (p50xMsg[p50length] >= 3) {		//"X" + 1 + 1 Byte Befehl 
							//printStatus(" XSensor");
							FBS88get(p50xMsg[p50Lowbyte]);	//Modulnummer (1..128)
						 }
						 break;
			  case 0x99: //printStatus(" XSensOff");   //Löscht alle S88-Bits auf Null und löscht alle Change-Flags.  
						 FBS88clear();	
						 send(OK);    //0 = Ok, accepted
						 break;
			  case 0x9D: if (p50xMsg[p50length] >= 7) {		//"X" + 1 + 5 Byte Befehl 
							//printStatus(" X88PSet");	//Einstellen der S88 Konfiguration
						 	//Zahl automatisch gelesenen Bytes
						 	//Zahl der Bytes auf Port1
							//Zahl der Bytes auf Port2
						 	//Zahl der Bytes auf Port3
						 	//Zahl der Bytes für Weichenrückmeldung
							send(OK);
						 }
						 break;
			  case 0xA6: //printStatus(" XPwrOff");
						 if (notifyPowerRequest)
							 notifyPowerRequest (false);
						 Railpower = false;
						 send(OK);    //cmd ok
						 break;
			  case 0xA7: //printStatus(" XPwrOn");
						 if (notifyPowerRequest)
							 notifyPowerRequest (true);	
						 Railpower = true;
						 send(OK);    //00h (cmd ok) or error code XPWOFF (06h): the Power is Off!
						 break;  
			  case 0xA5: //printStatus(" XHalt");  //Loks anhalten aber DCC bleibt aktiv
						 setHalt();		//Loks anhalten!
						 send(OK);    //00h (cmd ok) or error code
						 break;  
				//Programming:
			  case 0xE1: //XPT_On (= Programmiermodus)
						 PTMode = true;		//Programmiermodus EIN
						 send(OK);
						 break;
			  case 0xE2: //XPT_Off (= Verlassen Programmiermodus)
						 PTMode = false;		//Programmiermodus AUS
						 send(OK);
						 break;
					//Direct-Modes
			  case 0xF0: if (p50xMsg[p50length] >= 4) {		//"X" + 1 + 2 Byte Befehl 
							//printStatus(" XPT_DCCRD");  //Lesen mit Hilfe des Direct-Modes
							if (notifyXPT_DCCRDRequest)
								notifyXPT_DCCRDRequest ( Adr );	//CV Adresse
							send(OK);
						 }
						 break;
			  case 0xF1: if (p50xMsg[p50length] >= 5) {		//"X" + 1 + 3 Byte Befehl 
							//printStatus(" XPT_DCCWD");	 //Schreiben mit Hilfe des Direct-Modes
							if (notifyXPT_DCCWDRequest)
								notifyXPT_DCCWDRequest ( Adr, p50xMsg[p50data1] );
							send(OK);
						 }
						 break;
					//Bit-Modes
			  case 0xF2: if (p50xMsg[p50length] >= 4) {		//"X" + 1 + 2 Byte Befehl 
							//printStatus(" XPT_DCCRB");  //liest mit Hilfe des DCC bit-read Kommandos das CV aus der Lok aus
							if (notifyXPT_DCCRBRequest)
								notifyXPT_DCCRBRequest ( Adr );	//CV Adresse
							send(OK);
						 }
						 break;
			  case 0xF3: if (p50xMsg[p50length] >= 6) {		//"X" + 1 + 4 Byte Befehl 
							//printStatus(" XPT_DCCWB");  //Einzelbit Schreiben mit Hilfe des Bit-Modes
							if (notifyXPT_DCCWBRequest)
								notifyXPT_DCCWBRequest ( Adr, p50xMsg[p50data1], p50xMsg[p50data2] );	//CV, Bitposition (0..7), Der zu schreibende Inhalt (0|1)
							send(OK);
						 }
						 break;	
					//Paged Mode		
			  case 0xEE: if (p50xMsg[p50length] >= 4) {		//"X" + 1 + 2 Byte Befehl 
							//printStatus(" XPT_DCCRP");  //Lesen mit Hilfe des Register-Mode Befehls und Pages
							if (notifyXPT_DCCRPRequest)
								notifyXPT_DCCRPRequest ( Adr );  //CV Adresse
							send(OK);
						 }
						 break;
			  case 0xEF: if (p50xMsg[p50length] >= 5) {		//"X" + 1 + 3 Byte Befehl 
							//printStatus(" XPT_DCCWP");	 //Schreiben mit Hilfe des Page Mode (Auswahl von Page und dann mit Register-Mode Befehl
							if (notifyXPT_DCCWPRequest)
								notifyXPT_DCCWPRequest ( Adr, p50xMsg[p50data1] );  //CV Adresse, zu schreibende Inhalt
							send(OK);
						 }
						 break;
					//PoM
			  case 0xDA: if (p50xMsg[p50length] >= 6) {		//"X" + 1 + 4 Byte Befehl 
							//printStatus(" XDCC_PDR");		//Lok-Programmieren auf dem Hauptgleis = POM, CV-Lesen
							if (notifyXDCC_PDRRequest)
								notifyXDCC_PDRRequest (Adr, (p50xMsg[p50data2] << 8) |p50xMsg[p50data1], SendRepeat ); //Lokadresse; Low Byte der CV-Adresse, welche zu lesen ist.; High Byte der CV-Adresse, welche zu lesen ist. (1..1024)
							send(OK);
						 }
						 break;	
			  case 0xDE: if (p50xMsg[p50length] >= 7) {		//"X" + 1 + 5 Byte Befehl 
							//printStatus(" XDCC_PD");		//Lok-Programmieren auf dem Hauptgleis = POM
							if (notifyXDCC_PDRequest)
								notifyXDCC_PDRequest (Adr, (p50xMsg[p50data2] << 8) |p50xMsg[p50data1], p50xMsg[p50data3], SendRepeat ); //Lokadresse; Low Byte der CV-Adresse, welche zu lesen ist.; High Byte der CV-Adresse, welche zu lesen ist. (1..1024); Value (Wert)
							send(OK);
						 }
						 break;	
			  case 0xFE: //printStatus(" XPT_Term");		//Beenden der aktuellen Programmieraufgabe (Abbruch)
						 Byte = 0xF3;	//no task is active
						 if (notifyXPT_TermRequest)
							 Byte = notifyXPT_TermRequest();
						 send(Byte);	
						 break;	
			  case 0xCE: //printStatus(" XEvtPT");		//Abfrage Programmierergebnisse
						 if (PTMsg.Event == false)
							send(0xF5); //noch nicht fertig, es gibt nichts zu melden
						 else {
							send(PTMsg.command);	//falls [1,2,3,4,6] Zahl der Folgebytes
							send(PTMsg.status);		//Status der Programming Task
							if (PTMsg.command == 0x02)	//Inhalt CV oder low byte Adresse
								send(PTMsg.byte2);
							if (PTMsg.command == 0x03)	//high byte Adresse
								send(PTMsg.byte3);
/*							if (PTMsg.command == 0x04)
								send(PTMsg.byte4);
							if (PTMsg.command == 0x05)
								send(PTMsg.byte5);
							if (PTMsg.command == 0x06)
								send(PTMsg.byte6);
*/
							PTMsg.Event = false;	//Statusrückgabe erfolgt
						 }
						 break;
				//SO EEPROM Programm Zentrale
			  case 0xA3: if (p50xMsg[p50length] >= 5) {		//"X" + 1 + 3 Byte Befehl 
							//printStatus(" XSOSet");		//setzte EEprom
							if (EEPROM.read(Adr) != p50xMsg[p50data1])
								EEPROM.write(Adr,p50xMsg[p50data1]);
							send(OK);
						 }
						 break;
			  case 0xA4: if (p50xMsg[p50length] >= 4) {		//"X" + 1 + 2 Byte Befehl 
							//printStatus(" XSOGet");		//lese EEprom
							send(OK);
							send(EEPROM.read(Adr));
						 }
						 break;

//			  default:  printStatus("0x" + String(p50xMsg[p50command], HEX) + " unknown P50Xb");	//unbekannte P50Xb-Kommandos
			  }
	}
	else {
		if ((p50xMsg[p50msg] == 0x78 || p50xMsg[p50msg] == 0x58) && p50xMsg[p50command] < 0x80 && p50xMsg[p50length] > 1) {
/*			switch (p50xMsg[p50command]) {	//P50Xa-Kommandos

			default: printStatus("0x" + String(p50xMsg[p50command], HEX) + " unknown P50Xa");	//unbekannte P50Xa-Kommandos
			}
*/
		}
		else {
			switch (p50xMsg[p50msg]) {	//P50-Kommandos (Märklin 6050)
			case 0xC4:	//printStatus(" leere Antwort");		
						send(OK); //Leere Antwort
						break;
			case 0x60:  //printStatus(" Pwr on");
						if (notifyPowerRequest)
							notifyPowerRequest (true);
						Railpower = true;
						send(OK);           
						break;
			case 0x61:  //printStatus(" Pwr off");
						if (notifyPowerRequest)
							notifyPowerRequest (false);	
						Railpower = false;
						send(OK);           
						break;
/*			default:	//printStatus("0x" + String(p50xMsg[p50msg], HEX) + " unknown");	  //unknown ?
						//printStatus("0x" + String(p50xMsg[p50command], HEX) + " Command");	  //unknown ?
						//printStatus("0x" + String(p50xMsg[p50Lowbyte], HEX) + " LowByte");	  //unknown ?	*/
			}
		}
	}
  }
  else {	//keine Daten empfangen, setzte LED = Blink
	unsigned long currentMillis = millis();
	if(currentMillis - previousMillis > interval) {			//WARTEN
		p50clear();		//Nachricht zurücksetzen
		// save the last time you blinked the LED 
		previousMillis = currentMillis;   
		// if the LED is off turn it on and vice-versa:
		ledState = !ledState;
		if (notifyRS232)
			notifyRS232 (ledState);
	}
  }
}

//Zustand der Gleisversorgung setzten
void p50xClass::setPower(bool Power) 
{
	Railpower = Power;
	if (Power = true)
		Go = true;
	else Stop = true;
	PWREvent = true;	//Spannungsänderung
}

//Halt Befehl weiterleiten
void p50xClass::setHalt()
{
	Halt = true;
	for (int i = 0; i < SlotMax; i++) {		//alle Lok Slots durchlaufen
		if (xLokStsIsEmpty(i))
			break;
		else {
			xLokSts[i].speed = 0x00;
			bitWrite(xLokSts[i].func4, 7, 1);	//Datengruppe aktualisiert
			if (xLokSts[i].state < 0xFF)
				xLokSts[i].state += ((bitRead(xLokSts[i].func4, 7) + bitRead(xLokSts[i].func4, 6) + bitRead(xLokSts[i].func4, 5) + bitRead(xLokSts[i].func4, 4)) * xLokStsrepeat);	//Update Slot count
		}
	}
	//Rückmeldung der neuen Speed per p50x
/*	...
		...
		...
		...
*/
}

void p50xClass::ReturnPT(byte CV) {
	PTMsg.command = 0x02;	//2 Folgebytes
	PTMsg.status = OK;		//Command completed, no errors
	PTMsg.byte2 = CV;		//CV Adresse
	PTMsg.Event = true;
}


bool p50xClass::xLokStsUpdate()		//löst notifyLokRequest für nächsten aktiven Lok/Slot aus
//repeat gibt an wie oft wiederholt gesendet wird (0 = alle durchlaufend wiederholen)
{
	bool change = false;
	if (xLokStsaktSlot >= SlotMax || xLokStsIsEmpty(xLokStsaktSlot)) {
		xLokStsaktSlot = 0;		//beginne von vorne!
	}

	int sendSlot = xLokStsaktSlot;
	
	if (!(xLokStsIsEmpty(sendSlot))) {
		int Adr = ((xLokSts[sendSlot].high & 0x3F) << 8) | xLokSts[sendSlot].low;	//Adresse bestimmen
		if (bitRead(xLokSts[sendSlot].func4, 7) == 1 && (notifyLokRequest)) {	//xLokEvt
			notifyLokRequest( Adr, xLokSts[sendSlot].speed, xLokSts[sendSlot].high >> 7, bitRead(xLokSts[sendSlot].high, 6), UpdateRepeat );
			change = true;
			if (xLokSts[sendSlot].state > 0x00)
				xLokSts[sendSlot].state--;		//wurde wiederholt gesendet!
		}
		if (bitRead(xLokSts[sendSlot].func4, 6) == 1 && (notifyLokFuncRequest)) {		//xFunc
			notifyLokFuncRequest( Adr, xLokSts[sendSlot].func, UpdateRepeat);
			change = true;
			if (xLokSts[sendSlot].state > 0x00)
				xLokSts[sendSlot].state--;		//wurde wiederholt gesendet!
		}
		if (bitRead(xLokSts[sendSlot].func4, 5) == 1 && (notifyLokFunc2Request)) {		//xFunc2
			notifyLokFunc2Request( Adr, xLokSts[sendSlot].func2, UpdateRepeat);
			change = true;
			if (xLokSts[sendSlot].state > 0x00)
				xLokSts[sendSlot].state--;		//wurde wiederholt gesendet!
		}
		if (bitRead(xLokSts[sendSlot].func4, 4) == 1 && (notifyLokFunc34Request)) {	//xFunc34
			notifyLokFunc34Request( Adr, xLokSts[sendSlot].func3, xLokSts[sendSlot].func4 & 0x0F, UpdateRepeat);
			change = true;
			if (xLokSts[sendSlot].state > 0x00)
				xLokSts[sendSlot].state--;		//wurde wiederholt gesendet!
		}
		
/*		if (xLokSts[sendSlot].state == 0x00) {	//alle Daten (F1 - F28) wiederholt gesendet? - nicht erneut Senden
/*			if (xLokSts[sendSlot].func == 0x00) 	
				bitWrite(xLokSts[sendSlot].func4, 6, 0);			//xFunc nicht wiederholen!		*/
/*			if (xLokSts[sendSlot].func2 == 0x00) 	
				bitWrite(xLokSts[sendSlot].func4, 5, 0);			//xFunc2 nicht wiederholen!
			if (xLokSts[sendSlot].func3 == 0x00 && (xLokSts[sendSlot].func4 & 0x0F) == 0x00) 	
				bitWrite(xLokSts[sendSlot].func4, 4, 0);			//xFunc34 nicht wiederholen!
		}
*/
		xLokStsaktSlot++;				//nächsten Slot holen
	}
	return change;
}

//Handreglerereignis eintragen in Slotserver und Adresse für PC-Softwareupdate Speichern - Speed + DIR + F0
bool p50xClass::xLokStsTrottle (uint16_t Address, uint8_t Speed, uint8_t Direction, uint8_t F0) 
{
	for (int i = 0; i < TrottleMax; i++) {
		if (xLokTrottle[i] == 0x00 || xLokTrottle[i] == Address)	{  //Leer oder update?
			byte FktSts = 0x00;
			bitWrite(FktSts, 5, Direction);
			bitWrite(FktSts, 4, F0);
			if (xLokStsadd (highByte(Address),lowByte(Address),Speed & 0x7F, FktSts)) {	//Eintragen 
				xLokTrottle[i] == Address;		//Adresse des Trottle merken
				xLokEvent = true;		//Zugriff auf Lok!
				return true;
			}
		}
	}
	return false;
}

//Handreglerereignis eintragen in Slotserver - Func (F1 - F8)
bool p50xClass::xLokStsTrottleFunc (uint16_t Address, uint8_t Func)	//uint8_t Func2, uint8_t Func3, uint8_t Func4
{
	for (int i = 0; i < TrottleMax; i++) {
		if (xLokTrottle[i] == 0x00 || xLokTrottle[i] == Address)	{  //Leer oder update?
			if (xLokStsFunc (highByte(Address),lowByte(Address), Func)) {	//Eintragen 
				xLokTrottle[i] == Address;		//Adresse des Trottle merken
				xLokEvent = true;		//Zugriff auf Lok!
				return true;
			}
		}
	}
	return false;
}


void p50xClass::xLokStsUpdateClear (void) //Löscht refresh circle (Alle Slots zurücksetzten)
{
	xLokStsclear();	//Slots zurücksetzten
}

// Private Methods ///////////////////////////////////////////////////////////////////////////////////////////////////
// Functions only available to other functions in this library *******************************************************

//Rückmeldedaten ermitteln
void p50xClass::FBS88get(void)
{
  int count = 0;  //Zähler für kopiervorgänge doppelter Einträge
  bool repeat = false;
  for (int i = 0; i < FBMaxChange; i++) {
	  if (FBChange[i].modul > 0) {
		//Prüfen ob Modul nochmal vorhanden?
		int listpos = 0;  //falls doppelt zweite Position!
		for (int j = (i+1); j < FBMaxChange; j++) {
		  if (FBChange[i].modul == FBChange[j].modul) {
			  listpos = j;
			  break;
		  }
		}
		send(FBChange[i].modul);
		send(lowByte(FBChange[i].data));
		send(highByte(FBChange[i].data));
		FBS88[FBChange[i].modul-1] = FBChange[i].data;
		FBChange[i].modul = 0;
		FBChange[i].data = 0;
		if (listpos > 0) {
		  repeat = true; //erneutes Senden notwendig.
		  FBChange[count].modul = FBChange[listpos].modul;	//kopiere
		  FBChange[count].data = FBChange[listpos].data;
		  FBChange[listpos].modul = 0;		//altes Modul löschen!
		  FBChange[listpos].data = 0;
		  count++;
		}
	  }
	  else break;
  }
  send(OK);    //alle Rückmeldungen gesendet!
  FBS88Event = repeat;	//Rückmeldedaten übrig = JA/NEIN	
}

//Rückmeldemodul Datenrückgabe
void p50xClass::FBS88get(byte Modul)
{
	send(OK);
	send(lowByte(FBS88[Modul]));	//Kontakte 1..8 dieses Moduls (Bits 7..0)
	send(highByte(FBS88[Modul]));	//Kontakte 9..16 dieses Moduls
}

//Löscht alle S88-Bits auf Null und löscht alle Change-Flags. 
void p50xClass::FBS88clear(void)
{
	for (int i = 0; i < FBMax; i++)   //Rückmelderegister leere
		FBS88[i] = 0x00;
	for (int i = 0; i < FBMaxChange; i++) {    //neue Einträge leeren (Changes)
	    FBChange[i].modul = 0;  
		FBChange[i].data = 0x00;
	}
	FBS88Event = false;	//Rückmeldedaten eingetroffen = NEIN
}

//Löschen des letzten gesendeten Befehls
void p50xClass::p50clear(void) 
{
	p50xMsg[p50length] = 0x00;
	p50xMsg[p50msg] = 0x00;
	p50xMsg[p50command] = 0x00;
	p50xMsg[p50Lowbyte] = 0x00;
	p50xMsg[p50Highbyte] = 0x00;
	p50xMsg[p50data1] = 0x00;
	p50xMsg[p50data2] = 0x00;
	p50xMsg[p50data3] = 0x00;
}

/*
***************************************** SLOTSERVER ****************************************
	uint8_t low;		// A7, A6, A5, A4, A3, A2, A1, A0
	uint8_t high;		//Dir, FL, A13, A12, A11, A10, A9, A8
	uint8_t speed;		//Speed 0..127 (0x00 - 0x7F)
	uint8_t func;		//F1..F8 (bit #0..7)
	uint8_t func2;
	uint8_t func3;
	uint8_t func4;
	uint8_t state;	//Zahl der Zugriffe
*/

void p50xClass::xLokStsclear (void)	//löscht alle Slots
{
	for (int i = 0; i < SlotMax; i++) {
		xLokSts[i].low = 0xFF;
		xLokSts[i].high = 0xFF;
		xLokSts[i].speed = 0xFF;
		xLokSts[i].func = 0xFF;
		xLokSts[i].func2 = 0xFF;
		xLokSts[i].func3 = 0xFF;
		xLokSts[i].func4 = 0x0F;
		xLokSts[i].state = 0x00;
	}
	for (int i = 0; i < TrottleMax; i++) {
		xLokTrottle[i] = 0x00;
	}
}

void p50xClass::xLokStsEvt (void)		//XEvtLok Packet bestimmen und Senden
{
	if (xLokEvent == true) {
		for (int i = 0; i < TrottleMax; i++) {
			if (xLokTrottle[i] == 0x00)		//Prüfen ob am Ende?
				break;
			int slot = xLokStsgetSlot(highByte(xLokTrottle[i]), lowByte(xLokTrottle[i]));	//Slot ermitteln
			send(xLokSts[slot].speed);
			send(xLokSts[slot].func);
			send(xLokSts[slot].low);
			send(xLokSts[slot].high);
			send(xLokSts[slot].speed);
			xLokTrottle[i] = 0x00;		//Zurücksetzten
		}
		xLokEvent = false;		//Zugriffe Zurückgemeldet!
	}
	send(0x80);	//= nichts zu melden, Keine weitere Lok
}

bool p50xClass::xLokStsadd (byte MSB, byte LSB, byte Speed, byte FktSts)	//Eintragen Änderung / neuer Slot XLok
{
	bool change = false;
	byte Slot = xLokStsgetSlot(MSB, LSB);
	if (xLokSts[Slot].speed != Speed) {
		xLokSts[Slot].speed = Speed;
		change = true;
	}
	//FktSts = Chg-F, X, Dir, F0, F4, F3, F2, F1
	if ((bitRead(FktSts, 7) == 1) && (xLokSts[Slot].func != (FktSts & 0x0F) | (xLokSts[Slot].func & 0xF0))) {
		xLokSts[Slot].func = (FktSts & 0x0F) | (xLokSts[Slot].func & 0xF0);
		change = true;
	}
	if (bitRead(xLokSts[Slot].high, 7) != bitRead(FktSts, 5)) {
		bitWrite(xLokSts[Slot].high, 7, bitRead(FktSts, 5));	//DIR
		change = true;
	}
	if (bitRead(xLokSts[Slot].high, 6) != bitRead(FktSts, 4)) {
		bitWrite(xLokSts[Slot].high, 6, bitRead(FktSts, 4));	//F0
		change = true;
	}
	if (change == true) {
		bitWrite(xLokSts[Slot].func4, 7, 1);	//Datengruppe aktualisiert
		if (xLokSts[Slot].state < 0xFF)
			xLokSts[Slot].state += ((bitRead(xLokSts[Slot].func4, 7) + bitRead(xLokSts[Slot].func4, 6) + bitRead(xLokSts[Slot].func4, 5) + bitRead(xLokSts[Slot].func4, 4)) * xLokStsrepeat);	//Update Slot count
	}
	return change;
}

bool p50xClass::xLokStsFunc (byte MSB, byte LSB, byte Func)	//Eintragen Änderung / neuer Slot XFunc
{
	bool change = false;
	byte Slot = xLokStsgetSlot(MSB, LSB);
	if (xLokSts[Slot].func != Func) {
		xLokSts[Slot].func = Func;
		change = true;
	}
	if (change == true) {
		bitWrite(xLokSts[Slot].func4, 6, 1);	//Datengruppe aktualisiert
		if (xLokSts[Slot].state < 0xFF)
			xLokSts[Slot].state += ((bitRead(xLokSts[Slot].func4, 7) + bitRead(xLokSts[Slot].func4, 6) + bitRead(xLokSts[Slot].func4, 5) + bitRead(xLokSts[Slot].func4, 4)) * xLokStsrepeat);	//Update Slot count
	}
	return change;
}

bool p50xClass::xLokStsFunc2 (byte MSB, byte LSB, byte Func2)	//Eintragen Änderung / neuer Slot XFunc2
{
	bool change = false;
	byte Slot = xLokStsgetSlot(MSB, LSB);
	if (xLokSts[Slot].func2 != Func2) {
		xLokSts[Slot].func2 = Func2;
		change = true;
	}
	if (change == true) {
		bitWrite(xLokSts[Slot].func4, 5, 1);	//Datengruppe aktualisiert
		if (xLokSts[Slot].state < 0xFF)
			xLokSts[Slot].state += ((bitRead(xLokSts[Slot].func4, 7) + bitRead(xLokSts[Slot].func4, 6) + bitRead(xLokSts[Slot].func4, 5) + bitRead(xLokSts[Slot].func4, 4)) * xLokStsrepeat);	//Update Slot count
	}
	return change;
}

bool p50xClass::xLokStsFunc34 (byte MSB, byte LSB, byte Func3, byte Func4)	//Eintragen Änderung / neuer Slot XFunc34
{
	bool change = false;
	byte Slot = xLokStsgetSlot(MSB, LSB);
	if (xLokSts[Slot].func3 != Func3) {
		xLokSts[Slot].func3 = Func3;
		change = true;
	}
	if (xLokSts[Slot].func4 != (Func4 & 0x0F) | (xLokSts[Slot].func4 & 0xF0)) {
		xLokSts[Slot].func4 = (Func4 & 0x0F) | (xLokSts[Slot].func4 & 0xF0);
		change = true;
	}
	if (change == true) {
		bitWrite(xLokSts[Slot].func4, 4, 1);	//Datengruppe aktualisiert
		if (xLokSts[Slot].state < 0xFF)
			xLokSts[Slot].state += ((bitRead(xLokSts[Slot].func4, 7) + bitRead(xLokSts[Slot].func4, 6) + bitRead(xLokSts[Slot].func4, 5) + bitRead(xLokSts[Slot].func4, 4)) * xLokStsrepeat);	//Update Slot count
	}
	return change;
}

byte p50xClass::xLokStsgetSlot (byte MSB, byte LSB)		//gibt Slot für Adresse zurück / erzeugt neuen Slot (0..126)
{
	byte Slot = 0x00;	//kein Slot gefunden!
	for (int i = 0; i < SlotMax; i++) {
		if ((xLokSts[i].low == LSB && (xLokSts[i].high & 0x3F) == MSB) || xLokStsIsEmpty(i)) {
			Slot = i;			//Slot merken
			if (xLokStsIsEmpty(i)) 	//neuer freier Slot - Lok eintragen
				xLokStsSetNew(Slot, MSB, LSB);	//Eintragen
			return Slot;
		}
	}
	//kein Slot mehr vorhanden!
	byte zugriff = 0xFF;
	for (int i = 0; i < SlotMax; i++) {
		if (zugriff > xLokSts[i].state) {
			Slot = i;
			zugriff = xLokSts[i].state;
		}
	}
	xLokStsSetNew(Slot, MSB, LSB);	//Eintragen
	return Slot;
}

int p50xClass::xLokStsgetAdr (byte Slot)			//gibt Lokadresse des Slot zurück, wenn 0x0000 dann keine Lok vorhanden
{
	if (!xLokStsIsEmpty(Slot))
		return ((xLokSts[Slot].high & 0x3F) << 8) | xLokSts[Slot].low;	//Addresse zurückgeben
	return 0x0000;
}

bool p50xClass::xLokStsIsEmpty (byte Slot)	//prüft ob Datenpacket/Slot leer ist?
{
	if (xLokSts[Slot].low == 0xFF && xLokSts[Slot].high == 0xFF && xLokSts[Slot].speed == 0xFF && xLokSts[Slot].func == 0xFF && 
		xLokSts[Slot].func2 == 0xFF && xLokSts[Slot].func3 == 0xFF && xLokSts[Slot].func4 == 0x0F && xLokSts[Slot].state == 0x00)
		return true;
	return false;
}

void p50xClass::xLokStsSetNew (byte Slot, byte MSB, byte LSB)	//Neue Lok eintragen mit Adresse
{
	xLokSts[Slot].low = LSB;
	xLokSts[Slot].high = MSB & 0x3F;
	xLokSts[Slot].speed = 0x00;
	xLokSts[Slot].func = 0x00;
	xLokSts[Slot].func2 = 0x00;
	xLokSts[Slot].func3 = 0x00;
	xLokSts[Slot].func4 = 0x00;
	xLokSts[Slot].state = 0x00;
}