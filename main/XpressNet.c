/*
*****************************************************************************
  *		XpressNet.h - library for a client XpressNet protocoll
  *		Copyright (c) 2015-2017 Philipp Gahtow  All right reserved.
  *
  **	 Free for Private usage only!	
*****************************************************************************
  * IMPORTANT:
  * 
  * 	Please contact Lenz Inc. for details.
*****************************************************************************
 *  see for changes in XpressNet.h!
*/

// include this library's description file
#include "XpressNet.h"


#define interval 10500      //interval for Status LED (milliseconds)

//*******************************************************************************************
//Daten ermitteln und Auswerten
void receive(void) 
{
	/*
	XNetMsg[XNetlength] = 0x00;
	XNetMsg[XNetmsg] = 0x00;
	XNetMsg[XNetcommand] = 0x00;	savedData[1]
	XNetMsg[XNetdata1] = 0x00;	savedData[2]
	*/
	unsigned long currentMillis = esp_timer_get_time() / 1000; //aktuelle Zeit setzten

	if (DataReady == true)
	{ //Serial Daten dekodieren
		DataReady = false;
		//	  previousMillis = millis();   // will store last time LED was updated
		//Daten, setzte LED = ON!
		if (ledState == LOW)
		{ //LED -> aktivieren!
			ledState = HIGH;
			if (notifyXNetStatus)
				notifyXNetStatus(ledState);
		}
		if (XNetMsg[XNetmsg] == 0x60)
		{ //GENERAL_BROADCAST
			if (XNetMsg[XNetlength] == 4 && XNetMsg[XNetcom] == 0x61)
			{
				if ((XNetMsg[XNetdata1] == 0x01) && (XNetMsg[XNetdata2] == 0x60))
				{
					// Normal Operation Resumed
					Railpower = csNormal;
					if (notifyXNetPower)
						notifyXNetPower(Railpower);
				}
				else if ((XNetMsg[XNetdata1] == 0x00) && (XNetMsg[XNetdata2] == 0x61))
				{
					// Track power off
					Railpower = csTrackVoltageOff;
					if (notifyXNetPower)
						notifyXNetPower(Railpower);
				}
				else if ((XNetMsg[XNetdata1] == 0x08))
				{
					// Track Short
					Railpower = csShortCircuit;
					if (notifyXNetPower)
					{
						notifyXNetPower(csTrackVoltageOff);
						notifyXNetPower(Railpower);
					}
				}
				else if ((XNetMsg[XNetdata1] == 0x02) && (XNetMsg[XNetdata2] == 0x63))
				{
					// Service Mode Entry
					Railpower = csServiceMode;
					if (notifyXNetPower)
						notifyXNetPower(Railpower);
				}
			}
			else if (XNetMsg[XNetcom] == 0x81)
			{
				if ((XNetMsg[XNetdata1] == 0x00) && (XNetMsg[XNetdata2] == 0x81))
				{
					//Emergency Stop
					Railpower = csEmergencyStop;
					if (notifyXNetPower)
						notifyXNetPower(Railpower);
				}
			}
			else if ((XNetMsg[XNetcom] & 0xF0) == 0x40)
			{
				//R�ckmeldung
				uint8_t len = (XNetMsg[XNetcom] & 0x0F) / 2;	//each Adr and Data
				for (uint8_t i = 1; i <= len; i++)
				{
					sendSchaltinfo(false, XNetMsg[XNetcom+(i*2)-1] /*Adr*/, XNetMsg[XNetcom+(i*2)]/*Data*/);
				}
		  }
		  else if (XNetMsg[XNetlength] == 8 && XNetMsg[XNetcom] == 0x05 && XNetMsg[XNetdata1] == 0xF1) {
			  //DCC FAST CLOCK set request
			  /* 0x05 0xF1 TCODE1 TCODE2 TCODE3 TCODE4 [XOR]
				00mmmmmm	TCODE1, mmmmmm = denotes minutes, range 0...59.
				100HHHHH	TCODE2, HHHHH = denotes hours, range 0...23.
				01000www	TCODE3, www = denotes day of week, 0=monday, 1=tuesday, a.s.o.
				110fffff	TCODE4, fffff = denotes speed ratio, range 0..31. (0=stopped)
				*/
		  }
	}
	//else if (XNetMsg[XNetmsg] == myDirectedOps && XNetMsg[XNetlength] >= 3) {
		//change by Andr� Schenk
	else if (XNetMsg[XNetlength] >= 3) {	

		switch (XNetMsg[XNetcom]) {
		//add by Norberto Redondo Melchor:	
		case 0x52:	// Some other device asked for an accessory change
          //if (XNetMsg[XNetlength] >= 3) {
			  //change by Andr� Schenk
			if (XNetMsg[XNetdata2] >= 0x80) {  
            // Pos = 0000A00P A = turnout output (active 0/inactive 1); P = Turn v1 or --0
			uint8_t A_bit = (XNetMsg[XNetdata2] >> 3) & 0b0001;
			unsigned int Adr = (XNetMsg[XNetdata1] << 2) | ((XNetMsg[XNetdata2] & 0b0110) >> 1);  // Dir afectada
			uint8_t Pos = (XNetMsg[XNetdata2] & 0b0001) + 1;
			if (!A_bit) { // Accessory activation request
			  if (notifyTrnt)

			    //highByte(Adr), lowByte(Adr)
				data[1] = Adr >> 8; //High
			    data[2] = Adr & 0xFF;	  //Low
			  notifyTrnt(Adr, Pos);
            }
            else { // Accessory deactivation request
				Pos = Pos | 0b1000;
				if (notifyTrnt)
					notifyTrnt(Adr, Pos);
            }
          }
		break;
		case 0x62:
			if (XNetMsg[XNetdata1] == 0x21 && XNetMsg[XNetlength] >= 4) {    //Sw Version 2.3
				// old version - version 1 and version 2 response.
	        }
			else if (XNetMsg[XNetdata1] == 0x22 && XNetMsg[XNetlength] >= 5) {
				if (XNetRun == false) {	//Softwareversion anfragen
						unsigned char commandVersionSequence[] = {0x21, 0x21, 0x00};
						XNetSendadd (commandVersionSequence, 3);
						XNetRun = true;
				}
				Railpower = csNormal;
				if (XNetMsg[XNetdata2] != 0) {
					// is track power turned off?
					if ((XNetMsg[XNetdata2] & 0x01) == 0x01) { Railpower = csEmergencyStop; } //Bit 0: wenn 1, Anlage in Nothalt
		            // is it in emergency stop?
					if ((XNetMsg[XNetdata2] & 0x02) == 0x02) { Railpower = csTrackVoltageOff; }  //Bit 1: wenn 1, Anlage in Notaus
					// in service mode?
					if ((XNetMsg[XNetdata2] & 0x08) == 0x08) {Railpower = csServiceMode;}   //Bit 3: wenn 1, dann Programmiermode aktiv          
					// in powerup mode - wait until complete
					if ((XNetMsg[XNetdata2] & 0x40) == 0x40) {
						// put us in a state where we do the status request again...
						XNetRun = false;
					}
				}
				if (notifyXNetPower)
					notifyXNetPower(Railpower);
			}
		break;
		case 0x61:
		  if (XNetMsg[XNetlength] >= 4) {
			if (XNetMsg[XNetdata1] == 0x13) {
				//Programmierinfo �Daten nicht gefunden�
				if (notifyCVInfo)
					notifyCVInfo(0x02);
			}
			if (XNetMsg[XNetdata1] == 0x1F) {
				//Programmierinfo �Zentrale Busy�
				if (notifyCVInfo)
					notifyCVInfo(0x01);
			}
			if (XNetMsg[XNetdata1] == 0x11) {
				//Programmierinfo �Zentrale Bereit�
				if (notifyCVInfo)
					notifyCVInfo(0x00);
			}
			if (XNetMsg[XNetdata1] == 0x12) {
				//Programmierinfo �short-circuit�
				if (notifyCVInfo)
					notifyCVInfo(0x03);
			}
			if (XNetMsg[XNetdata1] == 0x80) {
				//Transfer Error
				if (notifyCVInfo)
					notifyCVInfo(0xE1);
			}
			if (XNetMsg[XNetdata1] == 0x82) {
				//Befehl nicht vorhanden R�ckmeldung
			}
		  }
		break;	
		case 0x63:
			//Softwareversion Zentrale
			if ((XNetMsg[XNetdata1] == 0x21) && (XNetMsg[XNetlength] >= 5)) {
				if (notifyXNetVer)
					notifyXNetVer(XNetMsg[XNetdata2], XNetMsg[XNetdata3]);
			}
			//Programmierinfo �Daten 3-Byte-Format� & �Daten 4-Byte-Format�
			if ((XNetMsg[XNetdata1] == 0x10 || XNetMsg[XNetdata1] == 0x14) && XNetMsg[XNetlength] >= 5) {
				uint8_t cvAdr = XNetMsg[XNetdata2];
				uint8_t cvData = XNetMsg[XNetdata3];
				if (notifyCVResult)
					notifyCVResult(cvAdr, cvData);
			}
		break;
		case 0xE4:	//Antwort der abgefragen Lok
			if ((XNetMsg[XNetlength] >= 7) && (ReqLocoAdr != 0) && ((XNetMsg[XNetdata1] >> 4) != 0)) {
				uint8_t Adr_MSB = highByte(ReqLocoAdr);
				uint8_t Adr_LSB = lowByte(ReqLocoAdr);
				ReqLocoAdr = 0;
				uint8_t Steps = XNetMsg[XNetdata1];		//0000 BFFF - B=Busy; F=Fahrstufen	
				bitWrite(Steps, 3, 0);	//Busy bit l�schen
				if (Steps == 0b100)
					Steps = 0b11;
				bool Busy = false;
				if (bitRead(XNetMsg[XNetdata1], 3) == 1)
					Busy = true;
				uint8_t Speed = XNetMsg[XNetdata2];		//RVVV VVVV - R=Richtung; V=Geschwindigkeit
				bitWrite(Speed, 7, 0);	//Richtungs bit l�schen
				uint8_t Direction = false;
				if (bitRead(XNetMsg[XNetdata2], 7) == 1)
					Direction = true;
				uint8_t F0 = XNetMsg[XNetdata3];	//0 0 0 F0 F4 F3 F2 F1
				uint8_t F1 = XNetMsg[XNetdata4];	//F12 F11 F10 F9 F8 F7 F6 F5

				uint8_t BSteps = Steps;
				if (Busy)
					bitWrite(BSteps, 3, 1);
				uint8_t funcsts = F0;				//FktSts = Chg-F, X, Dir, F0, F4, F3, F2, F1
				bitWrite(funcsts, 5, Direction);	//Direction hinzuf�gen
				
				bool chg = xLokStsadd (Adr_MSB, Adr_LSB, BSteps, Speed, funcsts);	//Eintrag in SlotServer
				chg = chg | xLokStsFunc1 (Adr_MSB, Adr_LSB, F1);
				if (chg == true) 			//�nderungen am Zustand?
					getLocoStateFull(Adr_MSB, Adr_LSB, true);
				
				if (Speed == 0) { //Lok auf Besetzt schalten
					setLocoHalt (Adr_MSB, Adr_LSB);//Sende Lok HALT um Busy zu erzeugen!
				}
			}
			else {
				uint8_t Adr_MSB = XNetMsg[XNetdata2];
				uint8_t Adr_LSB = XNetMsg[XNetdata3];
				uint8_t Slot = xLokStsgetSlot(Adr_MSB, Adr_LSB);
				switch (XNetMsg[XNetdata1]) {
					case 0x10: xLokSts[Slot].speed = XNetMsg[XNetdata4];//14 Speed steps
								break;
					case 0x11: xLokSts[Slot].speed = XNetMsg[XNetdata4];//27 Speed steps
								break;
					case 0x12: xLokSts[Slot].speed = XNetMsg[XNetdata4];//28 Speed steps	
								break;
					case 0x13: xLokSts[Slot].speed = XNetMsg[XNetdata4];//128 Speed steps
								break;	
					case 0x20: xLokSts[Slot].f0 = XNetMsg[XNetdata4];//Fkt Group1			
								break;
					case 0x21: xLokSts[Slot].f1 = XNetMsg[XNetdata4];//Fkt Group2			
								break;
					case 0x22: xLokSts[Slot].f2 = XNetMsg[XNetdata4];//Fkt Group3			
								break;	
					case 0x23: xLokSts[Slot].f3 = XNetMsg[XNetdata4];//Fkt Group4			
								break;				
					case 0x24: //Fkt Status			
								break;				
				}
				getLocoStateFull(Adr_MSB, Adr_LSB, true);
			}
		break;
		case 0xE3:	//Antwort abgefrage Funktionen F13-F28
			if (XNetMsg[XNetdata1] == 0x52 && XNetMsg[XNetlength] >= 6 && ReqFktAdr != 0) {	//Funktionszustadn F13 bis F28
				uint8_t Adr_MSB = highByte(ReqFktAdr);
				uint8_t Adr_LSB = lowByte(ReqFktAdr);
				ReqFktAdr = 0;
				uint8_t F2 = XNetMsg[XNetdata2]; //F2 = F20 F19 F18 F17 F16 F15 F14 F13
				uint8_t F3 = XNetMsg[XNetdata3]; //F3 = F28 F27 F26 F25 F24 F23 F22 F21
				if (xLokStsFunc23 (Adr_MSB, Adr_LSB, F2, F3) == true) {	//�nderungen am Zustand?
					if (notifyLokFunc)
						notifyLokFunc(Adr_MSB, Adr_LSB, F2, F3 );
					getLocoStateFull(Adr_MSB, Adr_LSB, true);
				}
			}
			if (XNetMsg[XNetdata1] == 0x40 && XNetMsg[XNetlength] >= 6) { 	// Locomotive is being operated by another device
				XLokStsSetBusy (XNetMsg[XNetdata2], XNetMsg[XNetdata3]);
			}
		break;
		case 0xE1:
			if (XNetMsg[XNetlength] >= 3) {
				//Fehlermeldung Lok control
				if (notifyCVInfo)
					notifyCVInfo(0xE1);
			}
		break;
		case 0x42:	//Antwort Schaltinformation
			if (XNetMsg[XNetlength] >= 4) {
				sendSchaltinfo(true, XNetMsg[XNetdata1], XNetMsg[XNetdata2]);
			}
		break;
		case 0xA3:	// Locomotive is being operated by another device
			if (XNetMsg[XNetlength] >= 4) {
				if (notifyXNetPower)
					notifyXNetPower(XNetMsg[XNetdata1]);
			}
		break;
		}	//switch myDirectedOps ENDE
	}
//	if (ReadData == false)	//Nachricht komplett empfangen, dann hier l�schen!
		XNetclear();	//alte verarbeitete Nachricht l�schen
  }		//Daten vorhanden ENDE
  else {	//keine Daten empfangen, setzte LED = Blink
	  previousMillis++;
	  if (previousMillis > interval) {			//WARTEN
		  XNetRun = false;	//Keine Zentrale vorhanden
		// save the last time you blinked the LED 
		previousMillis = 0;   
		// if the LED is off turn it on and off (Blink):
		ledState = !ledState;
		if (notifyXNetStatus)
			notifyXNetStatus (ledState);
	  }
  }
  //Slot Server aktualisieren
  if (currentMillis - SlotTime > SlotInterval) {
	  SlotTime = currentMillis;
	  UpdateBusySlot();		//Server Update - Anfrage nach Status�nderungen
  }
}

//--------------------------------------------------------------------------------------------
//Aufbereiten der Schaltinformation
void sendSchaltinfo(bool schaltinfo, uint8_t data1, uint8_t data2)
{
	int Adr = data1 * 4;
	uint8_t nibble = bitRead(data2, 4);
	uint8_t Pos1 = data2 & 0b11;
	uint8_t Pos2 = (data2 >> 2) & 0b11;

	if (nibble == 1)
		Adr = Adr + 2;
	
	if (schaltinfo) {
		if (notifyTrnt)
			notifyTrnt(highByte(Adr), lowByte(Adr), Pos1);
		if (notifyTrnt)
			notifyTrnt(highByte(Adr+1), lowByte(Adr+1), Pos2);
	}
	else {
		if (notifyFeedback)
			notifyFeedback(highByte(Adr), lowByte(Adr), Pos1);
		if (notifyFeedback)
			notifyFeedback(highByte(Adr+1), lowByte(Adr+1), Pos2);
	}
}

//--------------------------------------------------------------------------------------------
//Zustand der Gleisversorgung setzten
bool setPower(uint8_t Power)
{
	switch (Power) {	
	case csNormal: {
			unsigned char PowerAn[] = { 0x21, 0x81, 0xA0 };
			return XNetSendadd(PowerAn, 3);
		}
		case csEmergencyStop: {
			unsigned char EmStop[] = { 0x80, 0x80 };
			return XNetSendadd(EmStop, 2);
		}
		case csTrackVoltageOff: {
			unsigned char PowerAus[] = { 0x21, 0x80, 0xA1 };
			return XNetSendadd(PowerAus, 3);
		}
/*		case csShortCircuit:
			return false;
		case csServiceMode:
			return false;	*/
	}
	return false;
}

//--------------------------------------------------------------------------------------------
//Abfrage letzte Meldung �ber Gleispannungszustand
uint8_t getPower()
{
	return Railpower;
}

//--------------------------------------------------------------------------------------------
//Halt Befehl weiterleiten
void setHalt()
{
	setPower(csEmergencyStop);
}

//--------------------------------------------------------------------------------------------
//Abfragen der Lokdaten (mit F0 bis F12)
bool getLocoInfo(uint8_t Adr_High, uint8_t Adr_Low)
{
	bool ok = false;
	
	getLocoStateFull(Adr_High, Adr_Low, false);

	uint8_t Slot = xLokStsgetSlot(Adr_High, Adr_Low);
	if (xLokSts[Slot].state < 0xFF)
		xLokSts[Slot].state++;		//aktivit�t
		
	if (xLokStsBusy(Slot) == true && ReqLocoAdr == 0)	{		//Besetzt durch anderen XPressNet Handregler
		ReqLocoAdr = word(Adr_High, Adr_Low); //Speichern der gefragen Lok Adresse
		unsigned char getLoco[] = {0xE3, 0x00, Adr_High, Adr_Low, 0x00};
		if (ReqLocoAdr > 99)
			getLoco[2] = Adr_High | 0xC0;
		getXOR(getLoco, 5);
		ok = XNetSendadd (getLoco, 5);
	}
	
	return ok;
}

//--------------------------------------------------------------------------------------------
//Abfragen der Lok Funktionszust�nde F13 bis F28
bool getLocoFunc(uint8_t Adr_High, uint8_t Adr_Low)
{
	if (ReqFktAdr == 0) {
		ReqFktAdr = word(Adr_High, Adr_Low); //Speichern der gefragen Lok Adresse
		unsigned char getLoco[] = {0xE3, 0x09, Adr_High, Adr_Low, 0x00};
		if (word(Adr_High,Adr_Low) > 99)
			getLoco[2] = Adr_High | 0xC0;
		getXOR(getLoco, 5);
		return XNetSendadd (getLoco, 5);
	}
	unsigned char getLoco[] = {0xE3, 0x09, highByte(ReqFktAdr), lowByte(ReqFktAdr), 0x00};
	if (ReqFktAdr > 99)
			getLoco[2] = highByte(ReqFktAdr) | 0xC0;
	getXOR(getLoco, 5);
	return XNetSendadd (getLoco, 5);
}

//--------------------------------------------------------------------------------------------
//Lok Stoppen
bool setLocoHalt(uint8_t Adr_High, uint8_t Adr_Low)
{
	bool ok = false;
	unsigned char setLocoStop[] = {0x92, Adr_High, Adr_Low, 0x00};
	if (word(Adr_High,Adr_Low) > 99)
			setLocoStop[2] = Adr_High | 0xC0;
	getXOR(setLocoStop, 4);
	ok = XNetSendadd (setLocoStop, 4);

	uint8_t Slot = xLokStsgetSlot(Adr_High, Adr_Low);
	xLokSts[Slot].speed = 0;	//STOP

	getLocoStateFull(Adr_High, Adr_Low, true);
	return ok;
}

//--------------------------------------------------------------------------------------------
//Lokdaten setzten
bool setLocoDrive(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Steps, uint8_t Speed)
{
	bool ok = false;
	unsigned char setLoco[] = {0xE4, 0x10, Adr_High, Adr_Low, Speed, 0x00};
	if (word(Adr_High,Adr_Low) > 99)
			setLoco[2] = Adr_High | 0xC0;
	setLoco[1] |= Steps;
	
	getXOR(setLoco, 6);
	ok = XNetSendadd (setLoco, 6);

	uint8_t Slot = xLokStsgetSlot(Adr_High, Adr_Low);
	xLokSts[Slot].mode = (xLokSts[Slot].mode & 0b11111100) | Steps;	//Fahrstufen
	xLokSts[Slot].speed = Speed & 0b01111111;
	bitWrite(xLokSts[Slot].f0, 5, bitRead(Speed, 7));	//Dir

//	getLocoStateFull(Adr_High, Adr_Low, true);	

	//Nutzung protokollieren:
	if (xLokSts[Slot].state < 0xFF)
		xLokSts[Slot].state++;		//aktivit�t
	return ok;
}

//--------------------------------------------------------------------------------------------
//Lokfunktion setzten
bool setLocoFunc(uint8_t Adr_High, uint8_t Adr_Low, uint8_t type, uint8_t fkt)
{
	bool ok = false;	//Funktion wurde nicht gesetzt!
	bool fktbit = 0;	//neue zu �ndernde fkt bit
	if (type == 1)	//ein
		fktbit = 1;
	uint8_t Slot = xLokStsgetSlot(Adr_High, Adr_Low);
	//zu �nderndes bit bestimmen und neu setzten:
	if (fkt <= 4) {
		uint8_t func = xLokSts[Slot].f0 & 0b00011111; //letztes Zustand der Funktionen 000 F0 F4..F1
		if (type == 2) { //um
			if (fkt == 0)
				fktbit = !(bitRead(func, 4));
			else fktbit = !(bitRead(func, fkt-1));
		}
		if (fkt == 0)
			bitWrite(func, 4, fktbit);
		else bitWrite(func, fkt-1, fktbit);
		//Daten �ber XNet senden:
		unsigned char setLocoFunc[] = {0xE4, 0x20, Adr_High, Adr_Low, func, 0x00};	//Gruppe1 = 0 0 0 F0 F4 F3 F2 F1
		if (word(Adr_High,Adr_Low) > 99)
			setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNetSendadd (setLocoFunc, 6);
		//Slot anpassen:
		if (fkt == 0)
			bitWrite(xLokSts[Slot].f0, 4, fktbit);
		else bitWrite(xLokSts[Slot].f0, fkt-1, fktbit);
	}
	else if ((fkt >= 5) && (fkt <= 8)) {
		uint8_t funcG2 = xLokSts[Slot].f1 & 0x0F; //letztes Zustand der Funktionen 0000 F8..F5
		if (type == 2) //um
			fktbit = !(bitRead(funcG2, fkt-5));
		bitWrite(funcG2, fkt-5, fktbit);
		//Daten �ber XNet senden:
		unsigned char setLocoFunc[] = {0xE4, 0x21, Adr_High, Adr_Low, funcG2, 0x00};	//Gruppe2 = 0 0 0 0 F8 F7 F6 F5
		if (word(Adr_High,Adr_Low) > 99)
			setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNetSendadd (setLocoFunc, 6);
		//Slot anpassen:
		bitWrite(xLokSts[Slot].f1, fkt-5, fktbit);
	}
	else if ((fkt >= 9) && (fkt <= 12)) {
		uint8_t funcG3 = xLokSts[Slot].f1 >> 4; //letztes Zustand der Funktionen 0000 F12..F9
		if (type == 2) //um
			fktbit = !(bitRead(funcG3, fkt-9));
		bitWrite(funcG3, fkt-9, fktbit);
		//Daten �ber XNet senden:
		unsigned char setLocoFunc[] = {0xE4, 0x22, Adr_High, Adr_Low, funcG3, 0x00};	//Gruppe3 = 0 0 0 0 F12 F11 F10 F9
		if (word(Adr_High,Adr_Low) > 99)
			setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNetSendadd (setLocoFunc, 6);
		//Slot anpassen:
		bitWrite(xLokSts[Slot].f1, fkt-9+4, fktbit);
	}
	else if ((fkt >= 13) && (fkt <= 20)) {
		uint8_t funcG4 = xLokSts[Slot].f2;
		if (type == 2) //um
			fktbit = !(bitRead(funcG4, fkt-13));
		bitWrite(funcG4, fkt-13, fktbit);
		//Daten �ber XNet senden:
		//unsigned char setLocoFunc[] = {0xE4, 0x23, Adr_High, Adr_Low, funcG4, 0x00};	//Gruppe4 = F20 F19 F18 F17 F16 F15 F14 F13
		unsigned char setLocoFunc[] = {0xE4, 0xF3, Adr_High, Adr_Low, funcG4, 0x00};	//Gruppe4 = F20 F19 F18 F17 F16 F15 F14 F13
		//0xF3 = undocumented command is used when a mulitMAUS is controlling functions f20..f13. 
		if (word(Adr_High,Adr_Low) > 99)
			setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNetSendadd (setLocoFunc, 6);
		//Slot anpassen:
		bitWrite(xLokSts[Slot].f2, (fkt-13), fktbit);
	}
	else if ((fkt >= 21) && (fkt <= 28)) {
		uint8_t funcG5 = xLokSts[Slot].f3;
		if (type == 2) //um
			fktbit = !(bitRead(funcG5, fkt-21));
		bitWrite(funcG5, fkt-21, fktbit);
		//Daten �ber XNet senden:
		unsigned char setLocoFunc[] = {0xE4, 0x28, Adr_High, Adr_Low, funcG5, 0x00};	//Gruppe5 = F28 F27 F26 F25 F24 F23 F22 F21
		if (word(Adr_High,Adr_Low) > 99)
			setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNetSendadd (setLocoFunc, 6);
		//Slot anpassen:
		bitWrite(xLokSts[Slot].f3, (fkt-21), fktbit);
	}
	getLocoStateFull(Adr_High, Adr_Low, true);	//Alle aktiven Ger�te Senden!
	return ok;
}

//--------------------------------------------------------------------------------------------
//Gibt aktuellen Lokstatus an Anfragenden Zur�ck
void getLocoStateFull(uint8_t Adr_High, uint8_t Adr_Low, bool bc)
{
	uint8_t Slot = xLokStsgetSlot(Adr_High, Adr_Low);
	uint8_t Busy = bitRead(xLokSts[Slot].mode, 3);
	uint8_t Dir = bitRead(xLokSts[Slot].f0, 5);
	uint8_t F0 = xLokSts[Slot].f0 & 0b00011111;
	uint8_t F1 = xLokSts[Slot].f1;
	uint8_t F2 = xLokSts[Slot].f2;
	uint8_t F3 = xLokSts[Slot].f3;
	if (notifyLokAll)
		notifyLokAll(Adr_High, Adr_Low, Busy, xLokSts[Slot].mode & 0b11, xLokSts[Slot].speed, Dir, F0, F1, F2, F3, bc);
	//Nutzung protokollieren:
	if (xLokSts[Slot].state < 0xFF)
		xLokSts[Slot].state++;		//aktivit�t
}

//--------------------------------------------------------------------------------------------
//Ermitteln der Schaltstellung einer Weiche
bool getTrntInfo(uint8_t FAdr_High, uint8_t FAdr_Low)
{
	int Adr = word(FAdr_High, FAdr_Low);
	uint8_t nibble = 0; // 0 = Weiche 0 und 1; 1 = Weiche 2 und 3
	if ((Adr & 0x03) >= 2)
		nibble = 1;
	unsigned char getTrntPos[] = {0x42, 0x00, 0x80, 0x00};
	getTrntPos[1] = Adr >> 2;
	getTrntPos[2] += nibble;
	getXOR(getTrntPos, 4);
	return XNetSendadd (getTrntPos, 4);
}
//--------------------------------------------------------------------------------------------
//Schalten einer Weiche
bool setTrntPos(uint8_t FAdr_High, uint8_t FAdr_Low, uint8_t Pos)
//Pos = 0000A00P	A=Weichenausgang (Aktive/Inaktive); P=Weiche nach links oder nach rechts 
{
	int Adr = word(FAdr_High, FAdr_Low);
	uint8_t AdrL = ((Pos & 0x0F) | 0b110) & (((Adr & 0x03) << 1) | 0b1001); //1000ABBP -> A00P = Pos | BB = Adr & 0x03 (LSB Weichenadr.)

	Adr = Adr >> 2;
	bitWrite(AdrL, 7, 1);
	unsigned char setTrnt[] = {0x52, 0x00, AdrL, 0x00};	//old: 0x52, Adr, AdrL, 0x00
	//setTrnt[1] =  (Adr >> 2) & 0xFF;
	//change by Andr� Schenk:
	setTrnt[1] = Adr;
	getXOR(setTrnt, 4);

	//getTrntInfo(FAdr_High, FAdr_Low);  //Schaltstellung abfragen
	if (notifyTrnt)
		notifyTrnt(FAdr_High, FAdr_Low, (Pos & 0b1) + 1);

	return XNetSendadd (setTrnt, 4);
}

//--------------------------------------------------------------------------------------------
//CV-Mode CV Lesen
void readCVMode(uint8_t CV)
{
	unsigned char cvRead[] = {0x22, 0x15, CV, 0x00};
	getXOR(cvRead, 4);
	XNetSendadd (cvRead, 4);
	getresultCV(); //Programmierergebnis anfordern
}

//--------------------------------------------------------------------------------------------
//Schreiben einer CV im CV-Mode
void writeCVMode(uint8_t CV, uint8_t Data)
{
	unsigned char cvWrite[] = {0x23, 0x16, CV, Data, 0x00};
	getXOR(cvWrite, 5);
	XNetSendadd (cvWrite, 5);
	//getresultCV(); //Programmierergebnis anfordern

	if (notifyCVResult)
		notifyCVResult(CV, Data);
}

//--------------------------------------------------------------------------------------------
//Programmierergebnis anfordern
void getresultCV ()
{
	unsigned char getresult[] = {0x21, 0x10, 0x31};
	XNetSendadd (getresult, 3);
}

// Private Methods ///////////////////////////////////////////////////////////////////////////////////////////////////
// Functions only available to other functions in this library *******************************************************



//--------------------------------------------------------------------------------------------
//L�schen des letzten gesendeten Befehls
void XNetclear() 
{
	XNetMsg[XNetlength] = 0x00;
	XNetMsg[XNetmsg] = 0x00;
	XNetMsg[XNetcom] = 0x00;
	XNetMsg[XNetdata1] = 0x00;
	XNetMsg[XNetdata2] = 0x00;
	XNetMsg[XNetdata3] = 0x00;
	XNetMsg[XNetdata4] = 0x00;
	XNetMsg[XNetdata5] = 0x00;
}

//--------------------------------------------------------------------------------------------
void XNetclearSendBuf()		//Buffer leeren
{
	for (uint8_t i = 0; i < XSendMax; i++)
	{
		XNetSend[i].length = 0x00;			//L�nge zur�cksetzten
		for (uint8_t j = 0; j < XSendMaxData; j++)
		{
			XNetSend[i].data[j] = 0x00;		//Daten l�schen
		}
	}
}

//--------------------------------------------------------------------------------------------
bool XNetSendadd(uint8_t *dataString, uint8_t byteCount)
{
	for (uint8_t i = 0; i < XSendMax; i++)
	{
		if (XNetSend[i].length == 0) {	//Daten hier Eintragen:
			XNetSend[i].length = byteCount;	 //Datenlaenge
			for (uint8_t b = 0; b < byteCount; b++)
			{
				XNetSend[i].data[b] = *dataString;
				dataString++;
			}
			return true;	//leeren Platz gefunden -> ENDE
		}
	}
	return false;	//Kein Platz im Sendbuffer frei!
}

//--------------------------------------------------------------------------------------------
//Byte via Serial senden
void XNetsendout(void)
{	
	if (XNetSend[0].length != 0) { // && XNetSend[0].length < XSendMaxData) {
		if (XNetSend[0].data[0] != 0)
			XNetsend(XNetSend[0].data,XNetSend[0].length);
		for (int i = 0; i < (XSendMax-1); i++) {
			XNetSend[i].length = XNetSend[i+1].length;
			for (int j = 0; j < XSendMaxData; j++) {
				XNetSend[i].data[j] = XNetSend[i+1].data[j];		//Daten kopieren
			}
		}
		//letzten Leeren
		XNetSend[XSendMax-1].length = 0x00;
		for (int j = 0; j < XSendMaxData; j++) {
			XNetSend[XSendMax-1].data[j] = 0x00;		//Daten l�schen
		}
	}
	else XNetSend[0].length = 0;
}

/*
***************************************** SLOTSERVER ****************************************
	uint8_t low;		// A7, A6, A5, A4, A3, A2, A1, A0
	uint8_t high;		//X, X, A13, A12, A11, A10, A9, A8
	uint8_t speed;		//Speed 0..127 (0x00 - 0x7F)
	uint8_t f0;		//0, 0, Dir, F0, F4, F3, F2, F1
	uint8_t f1;
	uint8_t func3;
	uint8_t func4;
	uint8_t state;	//Zahl der Zugriffe
*/

//--------------------------------------------------------------------------------------------
void UpdateBusySlot(void)	//Fragt Zentrale nach aktuellen Zust�nden
{
/*
	if (ReqLocoAdr == 0) {
		if (xLokStsIsEmpty(SlotLast) == false && xLokSts[SlotLast].state > 0 && xLokStsBusy(SlotLast) == true) { 
			byte Adr_High = xLokSts[SlotLast].high & 0x3F;
			byte Adr_Low = xLokSts[SlotLast].low; 
			ReqLocoAdr = word(Adr_High, Adr_Low); //Speichern der gefragen Lok Adresse
			unsigned char getLoco[] = {0xE3, 0x00, Adr_High, Adr_Low, 0x00};
			getXOR(getLoco, 5);
			XNetSendadd (getLoco, 5);
//			if (bitRead(xLokSts[SlotLast].mode, 3) == 1)	//Slot BUSY?
				getLocoFunc (Adr_High, Adr_Low);	//F13 bis F28 abfragen
		}
		int Slot = SlotLast;
		SlotLast = getNextSlot(SlotLast);	//n�chste Lok holen
		while (SlotLast != Slot) {
			if (xLokStsBusy(SlotLast) == true) {
				Slot = SlotLast;
				break;
			}
			SlotLast = getNextSlot(SlotLast);	//n�chste Lok holen
		}
		
	}
	else
*/	
	if (ReqLocoAdr != 0) {
		ReqLocoAgain++;
		if (ReqLocoAgain > 9) {
			unsigned char getLoco[] = {0xE3, 0x00, highByte(ReqLocoAdr), lowByte(ReqLocoAdr), 0x00};
			if (ReqLocoAdr > 99)
				getLoco[2] = highByte(ReqLocoAdr) | 0xC0;
			getXOR(getLoco, 5);
			XNetSendadd (getLoco, 5);
			ReqLocoAgain = 0;
		}
	}
/*
	//Nichtnutzung von Slots erfassen:
	for (int i = 0; i < SlotMax; i++) {
		if (xLokSts[i].state > 0)
			xLokSts[i].state--;
		if (xLokSts[i].state > 0)
			xLokSts[i].state--;
	}
*/
}

//--------------------------------------------------------------------------------------------
void xLokStsclear (void)	//l�scht alle Slots
{
	for (int i = 0; i < SlotMax; i++) {
		xLokSts[i].low = 0xFF;
		xLokSts[i].high = 0xFF;
		xLokSts[i].mode = 0xFF;
		xLokSts[i].speed = 0xFF;
		xLokSts[i].f0 = 0xFF;
		xLokSts[i].f1 = 0xFF;
		xLokSts[i].f2 = 0xFF;
		xLokSts[i].f3 = 0xFF;
		xLokSts[i].state = 0x00;
	}
}

//--------------------------------------------------------------------------------------------
bool xLokStsadd(uint8_t MSB, uint8_t LSB, uint8_t Mode, uint8_t Speed, uint8_t FktSts) //Eintragen �nderung / neuer Slot XLok
{
	bool change = false;
	uint8_t Slot = xLokStsgetSlot(MSB, LSB);
	if (xLokSts[Slot].mode != Mode) {	//Busy & Fahrstufe (keine 14 Fahrstufen!)
		xLokSts[Slot].mode = Mode;
		change = true;
	}
	if (xLokSts[Slot].speed != Speed) {
		xLokSts[Slot].speed = Speed;
		change = true;
	}
	//FktSts = X, X, Dir, F0, F4, F3, F2, F1
	if (xLokSts[Slot].f0 != FktSts) {
		xLokSts[Slot].f0 = FktSts;
		change = true;
	}
	if (change == true && xLokSts[Slot].state < 0xFF)
		xLokSts[Slot].state++;
	
	return change;
}

//--------------------------------------------------------------------------------------------
bool xLokStsFunc0(uint8_t MSB, uint8_t LSB, uint8_t Func) //Eintragen �nderung / neuer Slot XFunc
{
	bool change = false;
	uint8_t Slot = xLokStsgetSlot(MSB, LSB);
	if ((xLokSts[Slot].f0 & 0b00011111) != Func) {
		xLokSts[Slot].f0 = Func | (xLokSts[Slot].f0 & 0b00100000);	//Dir anh�ngen!
		change = true;
	}
	if (change == true && xLokSts[Slot].state < 0xFF)
		xLokSts[Slot].state++;
	return change;
}

//--------------------------------------------------------------------------------------------
bool xLokStsFunc1(uint8_t MSB, uint8_t LSB, uint8_t Func1) //Eintragen �nderung / neuer Slot XFunc1
{
	bool change = false;
	uint8_t Slot = xLokStsgetSlot(MSB, LSB);
	if (xLokSts[Slot].f1 != Func1) {
		xLokSts[Slot].f1 = Func1;
		change = true;
	}
	if (change == true && xLokSts[Slot].state < 0xFF)
		xLokSts[Slot].state++;
	return change;
}

//--------------------------------------------------------------------------------------------
bool xLokStsFunc23(uint8_t MSB, uint8_t LSB, uint8_t Func2, uint8_t Func3) //Eintragen �nderung / neuer Slot
{
	bool change = false;
	uint8_t Slot = xLokStsgetSlot(MSB, LSB);
	if (xLokSts[Slot].f2 != Func2) {
		xLokSts[Slot].f2 = Func2;
		change = true;
	}
	if (xLokSts[Slot].f3 != Func3) {
		xLokSts[Slot].f3 = Func3;
		change = true;
	}
	if (change == true && xLokSts[Slot].state < 0xFF)
		xLokSts[Slot].state++;
	return change;
}

bool xLokStsBusy(uint8_t Slot)
{
	bool Busy = false;
	if (bitRead(xLokSts[Slot].mode, 3) == 1)
		Busy = true;
	return Busy;
}

void XLokStsSetBusy(uint8_t MSB, uint8_t LSB)
{
	uint8_t Slot = xLokStsgetSlot(MSB, LSB);
	bitWrite(xLokSts[Slot].mode, 3, 1);
	if (xLokSts[Slot].state < 0xFF)
		xLokSts[Slot].state++;
}

//--------------------------------------------------------------------------------------------
uint8_t xLokStsgetSlot(uint8_t MSB, uint8_t LSB) //gibt Slot f�r Adresse zur�ck / erzeugt neuen Slot (0..126)
{
	uint8_t Slot = 0x00; //kein Slot gefunden!
	for (int i = 0; i < SlotMax; i++) {
		if ((xLokSts[i].low == LSB && xLokSts[i].high == MSB) || xLokStsIsEmpty(i)) {
			Slot = i;			//Slot merken
			if (xLokStsIsEmpty(Slot)) 	//neuer freier Slot - Lok eintragen
				xLokStsSetNew(Slot, MSB, LSB);	//Eintragen
			return Slot;
		}
	}
	//kein Slot mehr vorhanden!
	uint8_t zugriff = 0xFF;
	for (int i = 0; i < SlotMax; i++) {
		if (xLokSts[i].state < zugriff) {
			Slot = i;
			zugriff = xLokSts[i].state;
		}
	}
	xLokStsSetNew(Slot, MSB, LSB);	//Eintragen
	return Slot;
}

//--------------------------------------------------------------------------------------------
int xLokStsgetAdr(uint8_t Slot) //gibt Lokadresse des Slot zur�ck, wenn 0x0000 dann keine Lok vorhanden
{
	if (!xLokStsIsEmpty(Slot))
		return word(xLokSts[Slot].high, xLokSts[Slot].low);	//Addresse zur�ckgeben
	return 0x0000;
}

//--------------------------------------------------------------------------------------------
bool xLokStsIsEmpty(uint8_t Slot) //pr�ft ob Datenpacket/Slot leer ist?
{
	if (xLokSts[Slot].low == 0xFF && xLokSts[Slot].high == 0xFF && xLokSts[Slot].speed == 0xFF && xLokSts[Slot].f0 == 0xFF && 
		xLokSts[Slot].f1 == 0xFF && xLokSts[Slot].f2 == 0xFF && xLokSts[Slot].f3 == 0xFF && xLokSts[Slot].state == 0x00)
		return true;
	return false;
}

//--------------------------------------------------------------------------------------------
void xLokStsSetNew(uint8_t Slot, uint8_t MSB, uint8_t LSB) //Neue Lok eintragen mit Adresse
{
	xLokSts[Slot].low = LSB;
	xLokSts[Slot].high = MSB;
	xLokSts[Slot].mode = 0b1011;	//Busy und 128 Fahrstufen
	xLokSts[Slot].speed = 0x00;
	xLokSts[Slot].f0 = 0x00;
	xLokSts[Slot].f1 = 0x00;
	xLokSts[Slot].f2 = 0x00;
	xLokSts[Slot].f3 = 0x00;
	xLokSts[Slot].state = 0x00;
}

//--------------------------------------------------------------------------------------------
uint8_t getNextSlot(uint8_t Slot) //gibt n�chsten genutzten Slot
{
	uint8_t nextS = Slot;
	for (int i = 0; i < SlotMax; i++) {
		nextS++;	//n�chste Lok
		if (nextS >= SlotMax)
			nextS = 0;	//Beginne von vorne
		if (xLokStsIsEmpty(nextS) == false)
			return nextS;
	}
	return nextS;
}

//--------------------------------------------------------------------------------------------
void setFree(uint8_t MSB, uint8_t LSB) //Lok aus Slot nehmen
{
	uint8_t Slot = xLokStsgetSlot(MSB, LSB);
	xLokSts[Slot].low = 0xFF;
	xLokSts[Slot].high = 0xFF;
	xLokSts[Slot].mode = 0xFF;
	xLokSts[Slot].speed = 0xFF;
	xLokSts[Slot].f0 = 0xFF;
	xLokSts[Slot].f1 = 0xFF;
	xLokSts[Slot].f2 = 0xFF;
	xLokSts[Slot].f3 = 0xFF;
	xLokSts[Slot].state = 0x00;
}

//--------------------------------------------------------------------------------------------
void notifyTrnt(uint16_t Adr, bool State)
{
	setTrntInfo(Adr, State);
}

bool setBasicAccessoryPos(uint16_t address, bool state)
{
	return setBasicAccessoryPos(address, state, true); //Ausgang aktivieren
}

bool setBasicAccessoryPos(uint16_t address, bool state, bool activ)
{
	/*
	Accessory decoder packet format:
	================================
	1111..11 0 1000-0001 0 1111-1011 0 EEEE-EEEE 1
      Preamble | 10AA-AAAA | 1aaa-CDDX | Err.Det.B

      aaaAAAAAA -> 111000001 -> Acc. decoder number 1

	  UINT16 FAdr = (FAdr_MSB << 8) + FAdr_LSB;
	  UINT16 Dcc_Addr = FAdr >> 2	//aaaAAAAAA

	  Beispiel:
	  FAdr=0 ergibt DCC-Addr=0 Port=0;
	  FAdr=3 ergibt DCC-Addr=0 Port=3;
	  FAdr=4 ergibt DCC-Addr=1 Port=0; usw

      C on/off:    1 => on		// Ausgang aktivieren oder deaktivieren
      DD turnout: 01 => 2		// FAdr & 0x03  // Port
      X str/div:   1 => set to diverging  // Weiche nach links oder nach rechts 
		=> X=0 soll dabei Weiche auf Abzweig bzw. Signal auf Halt kennzeichnen.

     => COMMAND: SET TURNOUT NUMBER 2 DIVERGING

	 1111..11 0 1000-0001 0 1111-0011 0 EEEE-EEEE 1
	 => COMMAND: SET TURNOUT NUMBER 6 DIVERGING
	*/
	if (address > 0x7FF) //check if Adr is ok, (max. 11-bit for Basic Adr)
		return false;

	DCCPacket p((address + TrntFormat) >> 2); //9-bit Address + Change Format Roco / Intellibox
	uint8_t data[1];
	data[0] = ((address + TrntFormat) & 0x03) << 1; //0000-CDDX
	if (state == true)								//SET X Weiche nach links oder nach rechts
		bitWrite(data[0], 0, 1);					//set turn
	if (activ == true)								//SET C Ausgang aktivieren oder deaktivieren
		bitWrite(data[0], 3, 1);					//set ON

	p.addData(data, 1);
	p.setKind(basic_accessory_packet_kind);
	p.setRepeat(OTHER_REPEAT);

	if (notifyTrnt)
		notifyTrnt(address, state);

	bitWrite(BasicAccessory[address / 8], address % 8, state); //pro SLOT immer 8 Zust�nde speichern!

	//return high_priority_queue.insertPacket(&p);
	return e_stop_queue.insertPacket(&p);
}
