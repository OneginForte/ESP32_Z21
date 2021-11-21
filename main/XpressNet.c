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
#include "XBusInterface.h"
#include "XpressNet.h"
#include "z21.h"

#define interval 10500      //interval for Status LED (milliseconds)

static const char *XNETP_TASK_TAG = "XNET_PARSER_TASK";

//--------------------------------------------------------------------------------------------
// calculate the XOR
void getXOR(unsigned char *data, uint8_t length)
{
	uint8_t XOR = 0x00;
	for (int i = 0; i < (length - 1); i++)
	{
		XOR = XOR ^ *data;
		data++;
	}
	*data = XOR;
}

//*******************************************************************************************
//Daten ermitteln und Auswerten
void xnetreceive(void) 
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
		ESP_LOGI(XNETP_TASK_TAG, "Hello in XNET Parse!");
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

			ESP_LOGI(XNETP_TASK_TAG, "XNET GENERAL_BROADCAST:");
			ESP_LOG_BUFFER_HEXDUMP(XNETP_TASK_TAG, XNetMsg, XNetMsg[XNetlength]+1, ESP_LOG_INFO);
			if (XNetMsg[XNetlength] == 4 && XNetMsg[XNetcom] == 0x61)
			{
				if ((XNetMsg[XNetdata1] == 0x01) && (XNetMsg[XNetdata2] == 0x60))
				{
					// Normal Operation Resumed
					ESP_LOGI(XNETP_TASK_TAG, "XNET csNormal");
					Railpower = csNormal;
					if (notifyXNetPower)
						notifyXNetPower(Railpower);
				}
				else if ((XNetMsg[XNetdata1] == 0x00) && (XNetMsg[XNetdata2] == 0x61))
				{
					// Track power off
					ESP_LOGI(XNETP_TASK_TAG, "XNET csTrackVoltageOff");
					Railpower = csTrackVoltageOff;
					if (notifyXNetPower)
						notifyXNetPower(Railpower);
				}
				else if ((XNetMsg[XNetdata1] == 0x08))
				{
					// Track Short
					ESP_LOGI(XNETP_TASK_TAG, "XNET csShortCircuit");
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
					ESP_LOGI(XNETP_TASK_TAG, "XNET csServiceMode");
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
					ESP_LOGI(XNETP_TASK_TAG, "XNET csEmergencyStop");
					Railpower = csEmergencyStop;
					if (notifyXNetPower)
						notifyXNetPower(Railpower);
				}
			}
			else if ((XNetMsg[XNetcom] & 0xF0) == 0x40)
			{
				//R�ckmeldung
				ESP_LOGI(XNETP_TASK_TAG, "XNET Ruckmeldung");
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
			  //if (notifyTrnt){

			    //highByte(Adr), lowByte(Adr)
				//data[1] = Adr >> 8; //High
			    //data[2] = Adr & 0xFF;	  //Low
			   notifyTrnt(Adr, Pos);
            	}
            else { // Accessory deactivation request
				Pos = Pos | 0b1000;
				//if (notifyTrnt)
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
						XNettransmit (commandVersionSequence, 3);
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
				uint16_t Addr=ReqLocoAdr;
				
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
				
				bool chg = LokStsadd (Addr, BSteps, Speed, funcsts);	//Eintrag in SlotServer
				chg = chg | LokStsFunc1 (Addr, F1);
				if (chg == true) 			//�nderungen am Zustand?
					getLocoStateFull(Addr, true);
				
				if (Speed == 0) { //Lok auf Besetzt schalten
					setLocoHalt (Addr);//Sende Lok HALT um Busy zu erzeugen!
				}
			}
			else {
				uint8_t Adr_MSB = XNetMsg[XNetdata2];
				uint8_t Adr_LSB = XNetMsg[XNetdata3];
				
				uint8_t Slot = LokStsgetSlot(Word(Adr_MSB, Adr_LSB));

				switch (XNetMsg[XNetdata1]) {
					case 0x10: LokDataUpdate[Slot].speed = XNetMsg[XNetdata4];//14 Speed steps
								break;
					case 0x11: LokDataUpdate[Slot].speed = XNetMsg[XNetdata4];//27 Speed steps
								break;
					case 0x12: LokDataUpdate[Slot].speed = XNetMsg[XNetdata4];//28 Speed steps	
								break;
					case 0x13: LokDataUpdate[Slot].speed = XNetMsg[XNetdata4];//128 Speed steps
								break;	
					case 0x20: LokDataUpdate[Slot].f0 = XNetMsg[XNetdata4];//Fkt Group1			
								break;
					case 0x21: LokDataUpdate[Slot].f1 = XNetMsg[XNetdata4];//Fkt Group2			
								break;
					case 0x22: LokDataUpdate[Slot].f2 = XNetMsg[XNetdata4];//Fkt Group3			
								break;	
					case 0x23: LokDataUpdate[Slot].f3 = XNetMsg[XNetdata4];//Fkt Group4			
								break;				
					case 0x24: //Fkt Status			
								break;				
				}
				getLocoStateFull(Word(Adr_MSB, Adr_LSB), true);
			}
		break;
		case 0xE3:	//Antwort abgefrage Funktionen F13-F28
			if (XNetMsg[XNetdata1] == 0x52 && XNetMsg[XNetlength] >= 6 && ReqFktAdr != 0) {	//Funktionszustadn F13 bis F28
				uint8_t Adr_MSB = highByte(ReqFktAdr);
				uint8_t Adr_LSB = lowByte(ReqFktAdr);
				ReqFktAdr = 0;
				uint8_t F2 = XNetMsg[XNetdata2]; //F2 = F20 F19 F18 F17 F16 F15 F14 F13
				uint8_t F3 = XNetMsg[XNetdata3]; //F3 = F28 F27 F26 F25 F24 F23 F22 F21
				if (LokStsFunc23 (Word(Adr_MSB, Adr_LSB), F2, F3) == true) {	//�nderungen am Zustand?

					notifyLokFunc(Word(Adr_MSB, Adr_LSB), F2, F3);
					getLocoStateFull(Word(Adr_MSB, Adr_LSB), true);
				}
			}
			if (XNetMsg[XNetdata1] == 0x40 && XNetMsg[XNetlength] >= 6) { 	// Locomotive is being operated by another device
				LokStsSetBusy(Word( XNetMsg[XNetdata2], XNetMsg[XNetdata3]));
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
			notifyTrnt(Adr, Pos1);
		if (notifyTrnt)
			notifyTrnt(Adr+1, Pos2);
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
			//return XNettransmit(PowerAn, 3);
		}
		case csEmergencyStop: {
			unsigned char EmStop[] = { 0x80, 0x80 };
			//return XNettransmit(EmStop, 2);
		}
		case csTrackVoltageOff: {
			unsigned char PowerAus[] = { 0x21, 0x80, 0xA1 };
			//return XNettransmit(PowerAus, 3);
		}
/*		case csShortCircuit:
			return false;
		case csServiceMode:
			return false;	*/
	}
	return false;
}


//--------------------------------------------------------------------------------------------
//Halt Befehl weiterleiten
void setHalt()
{
	setPower(csEmergencyStop);
}


//--------------------------------------------------------------------------------------------
//Abfragen der Lokdaten (mit F0 bis F12)
bool getLocoInfo(uint16_t Addr)
{
	bool ok = false;
	
	getLocoStateFull(Addr, false);

	uint8_t Slot = LokStsgetSlot(Addr);
	if (LokDataUpdate[Slot].state < 0xFF)
		LokDataUpdate[Slot].state++; //aktivit�t

	if (LokStsBusy(Slot) == true && ReqLocoAdr == 0)	{		//Besetzt durch anderen XPressNet Handregler
		
		ReqLocoAdr = Addr; //Speichern der gefragen Lok Adresse
		
		unsigned char getLoco[] = {0xE3, 0x00, highByte(Addr), lowByte(Addr), 0x00};
		if (ReqLocoAdr > 99)
			getLoco[2] = highByte(Addr) | 0xC0;
		getXOR(getLoco, 5);
		ok = XNettransmit (getLoco, 5);
	}
	
	return ok;
}

//--------------------------------------------------------------------------------------------
//Abfragen der Lok Funktionszust�nde F13 bis F28
bool getLocoFunc(uint16_t Addr)
{
	if (ReqFktAdr == 0) {
		
		//ReqFktAdr = word(Adr_High, Adr_Low); //Speichern der gefragen Lok Adresse
		
		unsigned char getLoco[] = {0xE3, 0x09, highByte(Addr), lowByte(Addr), 0x00};
		if (Addr > 99)
			getLoco[2] = highByte(Addr) | 0xC0;
		getXOR(getLoco, 5);
		return XNettransmit(getLoco, 5);
	}
	unsigned char getLoco[] = {0xE3, 0x09, highByte(Addr), lowByte(Addr), 0x00};
	if (Addr > 99)
			getLoco[2] = highByte(Addr) | 0xC0;
	getXOR(getLoco, 5);
	return XNettransmit(getLoco, 5);
}

void notifyLokFunc(uint16_t Address, uint8_t F2, uint8_t F3)
{
  //setFunctions0to4(Address, Function & 0x0F); //override F0 if active !!!
  //setFunctions5to8(Address, (Function & 0xF0) >> 4);
  //setFunctions9to12(Address, Function2);
  setFunctions13to20(Address, F2);
  setFunctions21to28(Address, F3);

  /*
  Serial.print("Func Request - Adr: ");
  Serial.print(Address, DEC);
  Serial.print(", F8-F1: ");
  Serial.println(Function, BIN);
  */
}

//--------------------------------------------------------------------------------------------
//Lok Stoppen
bool setLocoHalt(uint16_t Addr)
{
	bool ok = false;
	unsigned char setLocoStop[] = {0x92, highByte(Addr), lowByte(Addr), 0x00};
	if (Addr > 99)
			setLocoStop[2] = highByte(Addr) | 0xC0;
	getXOR(setLocoStop, 4);
	ok = XNettransmit (setLocoStop, 4);

	uint8_t Slot = LokStsgetSlot(Addr);
	LokDataUpdate[Slot].speed = 0; //STOP

	getLocoStateFull(Addr, true);
	return ok;
}

//--------------------------------------------------------------------------------------------
//Lokdaten setzten
bool setLocoDrive(uint16_t Addr, uint8_t Steps, uint8_t Speed)
{
	bool ok = false;
	unsigned char setLoco[] = {0xE4, 0x10, highByte(Addr), lowByte(Addr), Speed, 0x00};
	if (Addr > 99)
		setLoco[2] = highByte(Addr) | 0xC0;
	setLoco[1] |= Steps;
	
	getXOR(setLoco, 6);
	ok = XNettransmit (setLoco, 6);

	uint8_t Slot = LokStsgetSlot(Addr);
	LokDataUpdate[Slot].mode = (LokDataUpdate[Slot].mode & 0b11111100) | Steps; //Fahrstufen
	LokDataUpdate[Slot].speed = Speed & 0b01111111;
	bitWrite(LokDataUpdate[Slot].f0, 5, bitRead(Speed, 7)); //Dir

	//	getLocoStateFull(Adr_High, Adr_Low, true);	

	//Nutzung protokollieren:
	if (LokDataUpdate[Slot].state < 0xFF)
		LokDataUpdate[Slot].state++; //aktivit�t
	return ok;
}



//--------------------------------------------------------------------------------------------
//Ermitteln der Schaltstellung einer Weiche
bool getTrntInfo(uint8_t FAdr_High, uint8_t FAdr_Low)
{
	int Adr = Word(FAdr_High, FAdr_Low);
	uint8_t nibble = 0; // 0 = Weiche 0 und 1; 1 = Weiche 2 und 3
	if ((Adr & 0x03) >= 2)
		nibble = 1;
	unsigned char getTrntPos[] = {0x42, 0x00, 0x80, 0x00};
	getTrntPos[1] = Adr >> 2;
	getTrntPos[2] += nibble;
	getXOR(getTrntPos, 4);
	return XNettransmit (getTrntPos, 4);
}
//--------------------------------------------------------------------------------------------
//Schalten einer Weiche
bool setTrntPos(uint8_t FAdr_High, uint8_t FAdr_Low, uint8_t Pos)
//Pos = 0000A00P	A=Weichenausgang (Aktive/Inaktive); P=Weiche nach links oder nach rechts 
{
	int Adr = Word(FAdr_High, FAdr_Low);
	uint8_t AdrL = ((Pos & 0x0F) | 0b110) & (((Adr & 0x03) << 1) | 0b1001); //1000ABBP -> A00P = Pos | BB = Adr & 0x03 (LSB Weichenadr.)

	Adr = Adr >> 2;
	bitWrite(AdrL, 7, 1);
	unsigned char setTrnt[] = {0x52, 0x00, AdrL, 0x00};	//old: 0x52, Adr, AdrL, 0x00
	//setTrnt[1] =  (Adr >> 2) & 0xFF;
	//change by Andr� Schenk:
	setTrnt[1] = Adr;
	getXOR(setTrnt, 4);

	//getTrntInfo(FAdr_High, FAdr_Low);  //Schaltstellung abfragen
	//if (notifyTrnt)
		notifyTrnt(Adr, (Pos & 0b1) + 1);

	return XNettransmit (setTrnt, 4);
}

//--------------------------------------------------------------------------------------------
//CV-Mode CV Lesen
void readCVMode(uint8_t CV)
{
	unsigned char cvRead[] = {0x22, 0x15, CV, 0x00};
	getXOR(cvRead, 4);
	XNettransmit (cvRead, 4);
	getresultCV(); //Programmierergebnis anfordern
}

//--------------------------------------------------------------------------------------------
//Schreiben einer CV im CV-Mode
void writeCVMode(uint8_t CV, uint8_t Data)
{
	unsigned char cvWrite[] = {0x23, 0x16, CV, Data, 0x00};
	getXOR(cvWrite, 5);
	XNettransmit (cvWrite, 5);
	//getresultCV(); //Programmierergebnis anfordern

	if (notifyCVResult)
		notifyCVResult(CV, Data);
}

//--------------------------------------------------------------------------------------------
//Programmierergebnis anfordern
void getresultCV ()
{
	uint8_t getresult[] = {0x21, 0x10, 0x31};
	XNettransmit(getresult, 3);
}

// Private Methods ///////////////////////////////////////////////////////////////////////////////////////////////////
// Functions only available to other functions in this library *******************************************************

//--------------------------------------------------------------------------------------------
// send along a bunch of bytes to the Command Station
bool XNettransmit(uint8_t *dataString, uint8_t byteCount)
{
	uart_write_bytes(UART_NUM_1, dataString, byteCount);
	ESP_LOGI(XNETP_TASK_TAG, "XNET XNettransmit:");
	ESP_LOG_BUFFER_HEXDUMP(XNETP_TASK_TAG, dataString, byteCount, ESP_LOG_INFO);
	return true;
	//RAW_output(dataString, byteCount);
}

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
			XNettransmit (getLoco, 5);
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
void LokStsclear (void)	//l�scht alle Slots
{
	for (int i = 0; i < SlotMax; i++) {
		LokDataUpdate[i].adr = 0xFFFF;
		LokDataUpdate[i].mode = 0xFF;
		LokDataUpdate[i].speed = 0xFF;
		LokDataUpdate[i].f0 = 0xFF;
		LokDataUpdate[i].f1 = 0xFF;
		LokDataUpdate[i].f2 = 0xFF;
		LokDataUpdate[i].f3 = 0xFF;
		LokDataUpdate[i].state = 0x00;
	}
}

//--------------------------------------------------------------------------------------------
bool LokStsadd(uint16_t Addr, uint8_t Mode, uint8_t Speed, uint8_t FktSts) //Eintragen �nderung / neuer Slot XLok
{
	bool change = false;
	uint8_t Slot = LokStsgetSlot(Addr);
	if (LokDataUpdate[Slot].mode != Mode) {	//Busy & Fahrstufe (keine 14 Fahrstufen!)
		LokDataUpdate[Slot].mode = Mode;
		change = true;
	}
	if (LokDataUpdate[Slot].speed != Speed) {
		LokDataUpdate[Slot].speed = Speed;
		change = true;
	}
	//FktSts = X, X, Dir, F0, F4, F3, F2, F1
	if (LokDataUpdate[Slot].f0 != FktSts) {
		LokDataUpdate[Slot].f0 = FktSts;
		change = true;
	}
	if (change == true && LokDataUpdate[Slot].state < 0xFF)
		LokDataUpdate[Slot].state++;
	
	return change;
}

//--------------------------------------------------------------------------------------------
bool LokStsFunc0(uint16_t Addr, uint8_t Func) //Eintragen �nderung / neuer Slot XFunc
{
	bool change = false;
	uint8_t Slot = LokStsgetSlot(Addr);
	if ((LokDataUpdate[Slot].f0 & 0b00011111) != Func) {
		LokDataUpdate[Slot].f0 = Func | (LokDataUpdate[Slot].f0 & 0b00100000);	//Dir anh�ngen!
		change = true;
	}
	if (change == true && LokDataUpdate[Slot].state < 0xFF)
		LokDataUpdate[Slot].state++;
	return change;
}

//--------------------------------------------------------------------------------------------
bool LokStsFunc1(uint16_t Addr, uint8_t Func1) //Eintragen �nderung / neuer Slot XFunc1
{
	bool change = false;
	uint8_t Slot = LokStsgetSlot(Addr);
	if (LokDataUpdate[Slot].f1 != Func1) {
		LokDataUpdate[Slot].f1 = Func1;
		change = true;
	}
	if (change == true && LokDataUpdate[Slot].state < 0xFF)
		LokDataUpdate[Slot].state++;
	return change;
}

//--------------------------------------------------------------------------------------------
bool LokStsFunc23(uint16_t Addr, uint8_t Func2, uint8_t Func3) //Eintragen �nderung / neuer Slot
{
	bool change = false;
	uint8_t Slot = LokStsgetSlot(Addr);
	if (LokDataUpdate[Slot].f2 != Func2) {
		LokDataUpdate[Slot].f2 = Func2;
		change = true;
	}
	if (LokDataUpdate[Slot].f3 != Func3) {
		LokDataUpdate[Slot].f3 = Func3;
		change = true;
	}
	if (change == true && LokDataUpdate[Slot].state < 0xFF)
		LokDataUpdate[Slot].state++;
	return change;
}

bool LokStsBusy(uint8_t Slot)
{
	bool Busy = false;
	if (bitRead(LokDataUpdate[Slot].mode, 3) == 1)
		Busy = true;
	return Busy;
}

void LokStsSetBusy(uint16_t Addr)
{
	uint8_t Slot = LokStsgetSlot(Addr);
	bitWrite(LokDataUpdate[Slot].mode, 3, 1);
	if (LokDataUpdate[Slot].state < 0xFF)
		LokDataUpdate[Slot].state++;
}



//--------------------------------------------------------------------------------------------
int LokStsgetAdr(uint8_t Slot) //gibt Lokadresse des Slot zur�ck, wenn 0x0000 dann keine Lok vorhanden
{
	if (!LokStsIsEmpty(Slot))
		return (LokDataUpdate[Slot].adr);	//Addresse zur�ckgeben
	return 0x0000;
}

//--------------------------------------------------------------------------------------------
bool LokStsIsEmpty(uint8_t Slot) //pr�ft ob Datenpacket/Slot leer ist?
{
	if (LokDataUpdate[Slot].adr == 0xFF && LokDataUpdate[Slot].speed == 0xFF && LokDataUpdate[Slot].f0 == 0xFF && 
		LokDataUpdate[Slot].f1 == 0xFF && LokDataUpdate[Slot].f2 == 0xFF && LokDataUpdate[Slot].f3 == 0xFF && LokDataUpdate[Slot].state == 0x00)
		return true;
	return false;
}


//--------------------------------------------------------------------------------------------
uint8_t getNextSlot(uint8_t Slot) //gibt n�chsten genutzten Slot
{
	uint8_t nextS = Slot;
	for (int i = 0; i < SlotMax; i++) {
		nextS++;	//n�chste Lok
		if (nextS >= SlotMax)
			nextS = 0;	//Beginne von vorne
		if (LokStsIsEmpty(nextS) == false)
			return nextS;
	}
	return nextS;
}


//--------------------------------------------------------------------------------------------
void notifyTrnt(uint16_t cvAdr, uint8_t State)
{
	setTrntInfo(cvAdr, State);
	//setTrntInfo(Adr, State);
}

//---------------------------------------------------------------------------------
//Special Function for programming, switch and estop:

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

	//DCCPacket p((address + TrntFormat) >> 2); //9-bit Address + Change Format Roco / Intellibox
	//int8_t data[1];
	//data[0] = ((address + TrntFormat) & 0x03) << 1; //0000-CDDX
	//if (state == true)                              //SET X Weiche nach links oder nach rechts
	//  bitWrite(data[0], 0, 1);                      //set turn
	//if (activ == true)                              //SET C Ausgang aktivieren oder deaktivieren
	//bitWrite(data[0], 3, 1); //set ON

	//p.addData(data, 1);
	//p.setKind(basic_accessory_packet_kind);
	//p.setRepeat(OTHER_REPEAT);

	//if (notifyTrnt)
		notifyTrnt(address, state);

	bitWrite(BasicAccessory[address / 8], address % 8, state); //pro SLOT immer 8 Zust�nde speichern!

	//return high_priority_queue.insertPacket(&p);
	return true; //e_stop_queue.insertPacket(&p);
}

//--------------------------------------------------------------
void notifyXNetTrnt(uint16_t Address, uint8_t data)
{

	setBasicAccessoryPos(Address, data & 0x01, (bitRead(data, 3))); //Adr, left/right, activ
}
//--------------------------------------------------------------
//DCC return no ACK:
void notifyCVNack()
{

	z21setCVNack(); //send back to device and stop programming!

	//XNetsetCVNack();
}

bool opsProgDirectCV(uint16_t CV, uint8_t CV_data)
{
	//for CV#1 is the Adress 0
	//Long-preamble   0  0111CCAA  0  AAAAAAAA  0  DDDDDDDD  0  EEEEEEEE  1
	//CC=10 Bit Manipulation
	//CC=01 Verify byte
	//CC=11 Write byte

	//check if CV# is between 0 - 1023
	if (CV > 1023)
	{
		//if (notifyCVNack)
		//  notifyCVNack();
		return false;
	}

	//DCCPacket p(((CV >> 8) & B11) | B01111100);
	//uint8_t data[] = {0x00, 0x00};
	//data[0] = CV & 0xFF;
	//data[1] = CV_data;
	//p.addData(data, 2);
	//p.setKind(ops_mode_programming_kind); //always use short Adress Mode!
	//p.setRepeat(ProgRepeat);

	//if (railpower != SERVICE)      //time to wait for the relais!
	//  opsDecoderReset(RSTsRepeat); //send first a Reset Packet
	//setpower(SERVICE, true);
	//current_cv_bit = 0xFF; //write the byte!
	//current_ack_read = false;
	//ack_monitor_time = micros();
	//current_cv = CV;
	//current_cv_value = CV_data;

	//-------opsDecoderReset(RSTsRepeat);	//send first a Reset Packet
	//ops_programmming_queue.insertPacket(&p);
	//-----opsDecoderReset(RSTcRepeat);	//send a Reset while waiting to finish
	return 1; //opsVerifyDirectCV(current_cv, current_cv_value); //verify bit read
}

//--------------------------------------------------------------
void notifyXNetDirectCV(uint16_t CV, uint8_t data)
{

	opsProgDirectCV(CV, data); //return value from DCC via 'notifyCVVerify'
}

bool opsReadDirectCV(uint16_t CV) //, uint8_t bitToRead, bool bitSet)
{
	//for CV#1 is the Adress 0
	//long-preamble   0  011110AA  0  AAAAAAAA  0  111KDBBB  0  EEEEEEEE  1
	//Bit Manipulation
	//BBB represents the bit position
	//D contains the value of the bit to be verified or written
	//K=1 signifies a "Write Bit" operation and K=0 signifies a "Bit Verify"

	//check if CV# is between 0 - 1023
	//if (CV > 1023)
	//{
	//  if (notifyCVNack)
	//    notifyCVNack();
	//  return false;
	//}

	//if (current_cv_bit > 7 || bitToRead == 0)
	//{
	//  current_cv_bit = 0;
	//  bitToRead = 0;
	//}

	//if (railpower != SERVICE)      //time to wait for the relais!
	//  opsDecoderReset(RSTsRepeat); //send first a Reset Packet
	//setpower(SERVICE, true);
	//current_ack_read = false;
	//ack_monitor_time = micros();
	//current_cv = CV;

	//DCCPacket p(((CV >> 8) & B11) | B01111000);
	//uint8_t data[] = {0x00, 0x00};
	//data[0] = CV & 0xFF;
	//data[1] = B11100000 | (bitToRead & 0x07) | (bitSet << 3); //verify Bit is "bitSet"? ("1" or "0")
	//p.addData(data, 2);
	//p.setKind(ops_mode_programming_kind); //always use short Adress Mode!
	//p.setRepeat(ProgRepeat);

	//if (bitToRead == 0)
	//{
	//  opsDecoderReset(RSTsRepeat); //send first a Reset Packet
	// }

	return 1; //ops_programmming_queue.insertPacket(&p);
			  //--------opsDecoderReset(RSTcRepeat);	//send a Reset while waiting to finish
			  //return opsDecoderReset(RSTcRepeat);	//send a Reset while waiting to finish
}

//--------------------------------------------------------------
void notifyXNetDirectReadCV(uint16_t cvAdr)
{

	opsReadDirectCV(cvAdr); //read cv
}

bool opsProgramCV(uint16_t address, uint16_t CV, uint8_t CV_data)
{
	//format of packet:
	// {preamble} 0 [ AAAAAAAA ] 0 111011VV 0 VVVVVVVV 0 DDDDDDDD 0 EEEEEEEE 1 (write)
	// {preamble} 0 [ AAAAAAAA ] 0 111001VV 0 VVVVVVVV 0 DDDDDDDD 0 EEEEEEEE 1 (verify/read)
	// {preamble} 0 [ AAAAAAAA ] 0 111010VV 0 VVVVVVVV 0 DDDDDDDD 0 EEEEEEEE 1 (bit manipulation)
	// only concerned with "write" form here!!!

	if (address == 0) //check if Adr is ok?
		return false;

	//DCCPacket p(address);
	//uint8_t data[] = {0x00, 0x00, 0x00};

	// split the CV address up among data uint8_ts 0 and 1
	//data[0] = ((CV >> 8) & 0x11) | 0x11101100;
	//data[1] = CV & 0xFF;
	//data[2] = CV_data;

	//p.addData(data, 3);
	//p.setKind(pom_mode_programming_kind);
	//.setRepeat(ProgRepeat);

	//return low_priority_queue.insertPacket(&p);	//Standard

	//setpower(SERVICE, true);

	//opsDecoderReset();	//send first a Reset Packet
	return 1; // ops_programmming_queue.insertPacket(&p);
			  //return e_stop_queue.insertPacket(&p);
}
//--------------------------------------------------------------
void notifyXNetPOMwriteByte(uint16_t Adr, uint16_t CV, uint8_t data)
{

	opsProgramCV(Adr, CV, data); //set decoder byte
}

bool opsPOMwriteBit(uint16_t address, uint16_t CV, uint8_t Bit_data)
{
	//format of packet:
	// {preamble} 0 [ AAAAAAAA ] 0 111011VV 0 VVVVVVVV 0 DDDDDDDD 0 EEEEEEEE 1 (write)
	// {preamble} 0 [ AAAAAAAA ] 0 111001VV 0 VVVVVVVV 0 DDDDDDDD 0 EEEEEEEE 1 (verify/read)
	// {preamble} 0 [ AAAAAAAA ] 0 111010VV 0 VVVVVVVV 0 DDDDDDDD 0 EEEEEEEE 1 (bit manipulation)
	// only concerned with "write" form here!!!

	if (address == 0) //check if Adr is ok?
		return false;

	//DCCPacket p(address);
	//uint8_t data[] = {0x00, 0x00, 0x00};

	// split the CV address up among data uint8_ts 0 and 1
	//data[0] = ((CV >> 8) & 0b11) | 0b11101000;
	//data[1] = CV & 0xFF;
	//data[2] = Bit_data & 0x0F;

	//p.addData(data, 3);
	//p.setKind(pom_mode_programming_kind);
	//p.setRepeat(ProgRepeat);

	//return low_priority_queue.insertPacket(&p);	//Standard

	//setpower(SERVICE, true);

	//opsDecoderReset();	//send first a Reset Packet
	return 1; // ops_programmming_queue.insertPacket(&p);
			  //return e_stop_queue.insertPacket(&p);
}

//--------------------------------------------------------------
void notifyXNetPOMwriteBit(uint16_t Adr, uint16_t CV, uint8_t data)
{

	opsPOMwriteBit(Adr, CV, data); //set decoder bit
}

//--------------------------------------------------------------------------------------------
//Zustand der Gleisversorgung setzten
void XpressNetsetPower(uint8_t Power)
{
	switch (Power)
	{
	case csNormal:
	{
		unsigned char PowerAn[] = {0x21, 0x81, 0xA0};
		XNettransmit(PowerAn, 3);
	}
	case csEmergencyStop:
	{
		unsigned char EmStop[] = {0x80, 0x80};
		XNettransmit(EmStop, 2);
	}
	case csTrackVoltageOff:
	{
		unsigned char PowerAus[] = {0x21, 0x80, 0xA1};
		XNettransmit(PowerAus, 3);
	}
		/*		case csShortCircuit:
			return false;
		case csServiceMode:
			return false;	*/
	}
	//return false;
}