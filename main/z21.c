/*
*****************************************************************************
  *		z21.cpp - library for Roco Z21 LAN protocoll
  *		Copyright (c) 2013-2017 Philipp Gahtow  All right reserved.
  *
  *
*****************************************************************************
  * IMPORTANT:
  * 
  * 	Please contact Roco Inc. for more details.
*****************************************************************************
*/

// include this library's description file
#include <z21.h>
#include <z21header.h>
#include "esptimer.h"

//need to include eeprom


// Function that handles the creation and setup of instances



void z21Class()
{
	// initialize this instance's variables
	z21IPpreviousMillis = 0;
	Railpower = csTrackVoltageOff;
	clearIPSlots();
}

bool bitRead(uint8_t order, uint8_t num)
{
	return (order & (1 << num));
}

//*********************************************************************************************
//Daten ermitteln und Auswerten
void receive(uint8_t client, uint8_t *packet)
{
	addIPToSlot(client, 0);
	// send a reply, to the IP address and port that sent us the packet we received
	int header = (packet[3] << 8) + packet[2];
	uint8_t data[16]; //z21 send storage

	switch (header)
	{
	case LAN_GET_SERIAL_NUMBER:
		ESP_LOGI(Z21_PARSER_TAG, "GET_SERIAL_NUMBER");

		//ESP_LOGI(Z21_PARSER_TAG, "Socket created");


		data[0] = z21SnLSB;
		data[1] = z21SnMSB;
		data[2] = 0x00;
		data[3] = 0x00;
		EthSend(client, 0x08, LAN_GET_SERIAL_NUMBER, data, false, Z21bcNone); //Seriennummer 32 Bit (little endian)
		break;
	case LAN_GET_HWINFO:
		ESP_LOGI(Z21_PARSER_TAG, "GET_HWINFO");

		data[0] = z21HWTypeLSB; //HwType 32 Bit
		data[1] = z21HWTypeMSB;
		data[2] = 0x00;
		data[3] = 0x00;
		data[4] = z21FWVersionLSB; //FW Version 32 Bit
		data[5] = z21FWVersionMSB;
		data[6] = 0x00;
		data[7] = 0x00;
		EthSend(client, 0x0C, LAN_GET_HWINFO, data, false, Z21bcNone);
		break;
	case LAN_LOGOFF:
		ESP_LOGI(Z21_PARSER_TAG, "LOGOFF");

		clearIPSlot(client);
		//Antwort von Z21: keine
		break;
	case LAN_GET_CODE: //SW Feature-Umfang der Z21
		/*#define Z21_NO_LOCK        0x00  // keine Features gesperrt 
			#define z21_START_LOCKED   0x01  // �z21 start�: Fahren und Schalten per LAN gesperrt 
			#define z21_START_UNLOCKED 0x02  // �z21 start�: alle Feature-Sperren aufgehoben */
		data[0] = 0x00; //keine Features gesperrt
		EthSend(client, 0x05, LAN_GET_CODE, data, false, Z21bcNone);
		break;
	case (LAN_X_Header):
		switch (packet[4])
		{ //X-Header
		case LAN_X_GET_SETTING:
			switch (packet[5])
			{ //DB0
			case 0x21:
				ESP_LOGI(Z21_PARSER_TAG, "X_GET_VERSION");

				data[0] = LAN_X_GET_VERSION; //X-Header: 0x63
				data[1] = 0x21;							 //DB0
				data[2] = 0x30;							 //X-Bus Version
				data[3] = 0x12;							 //ID der Zentrale
				EthSend(client, 0x09, LAN_X_Header, data, true, Z21bcNone);
				break;
			case 0x24:
				data[0] = LAN_X_STATUS_CHANGED; //X-Header: 0x62
				data[1] = 0x22;									//DB0
				data[2] = Railpower;						//DB1: Status
				//ZDebug.print("X_GET_STATUS ");
				//csEmergencyStop  0x01 // Der Nothalt ist eingeschaltet
				//csTrackVoltageOff  0x02 // Die Gleisspannung ist abgeschaltet
				//csShortCircuit  0x04 // Kurzschluss
				//csProgrammingModeActive 0x20 // Der Programmiermodus ist aktiv
				EthSend(client, 0x08, LAN_X_Header, data, true, Z21bcNone);
				break;
			case 0x80:
				ESP_LOGI(Z21_PARSER_TAG, "X_SET_TRACK_POWER_OFF");

				if (notifyz21RailPower)
					notifyz21RailPower(csTrackVoltageOff);
				break;
			case 0x81:
				ESP_LOGI(Z21_PARSER_TAG, "X_SET_TRACK_POWER_ON");

				if (notifyz21RailPower)
					notifyz21RailPower(csNormal);
				break;
			}
			break; //ENDE DB0
		case LAN_X_CV_READ:
			if (packet[5] == 0x11)
			{ //DB0
				ESP_LOGI(Z21_PARSER_TAG, "X_CV_READ");

				if (notifyz21CVREAD)
					notifyz21CVREAD(packet[6], packet[7]); //CV_MSB, CV_LSB
			}
			break;
		case LAN_X_CV_WRITE:
			if (packet[5] == 0x12)
			{ //DB0
				ESP_LOGI(Z21_PARSER_TAG, "X_CV_WRITE");

				if (notifyz21CVWRITE)
					notifyz21CVWRITE(packet[6], packet[7], packet[8]); //CV_MSB, CV_LSB, value
			}
			break;
		case LAN_X_CV_POM:
			if (packet[5] == 0x30)
			{ //DB0
				uint8_t Adr = ((packet[6] & 0x3F) << 8) + packet[7];
				uint8_t CVAdr = ((packet[8] & 0b11000000) << 8) + packet[9];
				uint8_t value = packet[10];
				if ((packet[8] >> 2) == 0b11101100)
				{
					ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_WRITE_BYTE");

					if (notifyz21CVPOMWRITEBYTE)
						notifyz21CVPOMWRITEBYTE(Adr, CVAdr, value); //set decoder
				}
				else if ((packet[8] >> 2) == 0b11101000 && value == 0)
				{
					ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_WRITE_BIT");

				}
				else
				{
					ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_READ_BIYTE");

					if (notifyz21CVPOMREADBYTE)
						notifyz21CVPOMREADBYTE(Adr, CVAdr); //set decoder
				}
			}
			else if (packet[5] == 0x31)
			{ //DB0
				ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_ACCESSORY");

			}
			break;
		case LAN_X_GET_TURNOUT_INFO:
		{
			ESP_LOGI(Z21_PARSER_TAG, "X_GET_TURNOUT_INFO ");

			if (notifyz21AccessoryInfo)
			{
				data[0] = 0x43;			 //X-HEADER
				data[1] = packet[5]; //High
				data[2] = packet[6]; //Low
				if (notifyz21AccessoryInfo((packet[5] << 8) + packet[6]) == true)
					data[3] = 0x02; //active
				else
					data[3] = 0x01;																						 //inactive
				EthSend(client, 0x09, LAN_X_Header, data, true, Z21bcAll_s); //BC new 23.04. !!! (old = 0)
			}
			break;
		}
		case LAN_X_SET_TURNOUT:
		{
			ESP_LOGI(Z21_PARSER_TAG, "X_SET_TURNOUT");

			//bool TurnOnOff = bitRead(packet[7],3);  //Spule EIN/AUS
			if (notifyz21Accessory)
			{
				notifyz21Accessory((packet[5] << 8) + packet[6], bitRead(packet[7], 0), bitRead(packet[7], 3));
			} //	Addresse					Links/Rechts			Spule EIN/AUS
			break;
		}
		case LAN_X_SET_STOP:
			ESP_LOGI(Z21_PARSER_TAG, "X_SET_STOP");

			if (notifyz21RailPower)
				notifyz21RailPower(csEmergencyStop);
			break;
		case LAN_X_GET_LOCO_INFO:
			if (packet[5] == 0xF0)
			{ //DB0
				//ZDebug.print("X_GET_LOCO_INFO: ");
				//Antwort: LAN_X_LOCO_INFO  Adr_MSB - Adr_LSB
				if (notifyz21getLocoState)
					notifyz21getLocoState(((packet[6] & 0x3F) << 8) + packet[7], false);
				//Antwort via "setLocoStateFull"!
			}
			break;
		case LAN_X_SET_LOCO:
			if (packet[5] == LAN_X_SET_LOCO_FUNCTION)
			{ //DB0
				//LAN_X_SET_LOCO_FUNCTION  Adr_MSB        Adr_LSB            Type (00=AUS/01=EIN/10=UM)      Funktion
				//word(packet[6] & 0x3F, packet[7]), packet[8] >> 6, packet[8] & 0b00111111
				
				uint16_t WORD = (((uint16_t)packet[6] & 0x3F) << 8) | ((uint16_t)packet[7]);

				if (notifyz21LocoFkt)
					notifyz21LocoFkt(WORD, packet[8] >> 6, packet[8] & 0b00111111);
				//uint16_t Adr, uint8_t type, uint8_t fkt
			}
			else
			{ //DB0
				//ZDebug.print("X_SET_LOCO_DRIVE ");
				uint8_t steps = 14;
				if ((packet[5] & 0x03) == 3)
					steps = 128;
				else if ((packet[5] & 0x03) == 2)
					steps = 28;
				
				uint16_t WORD = (((uint16_t)packet[6] & 0x3F) << 8) | ((uint16_t)packet[7]);
				if (notifyz21LocoSpeed)
					notifyz21LocoSpeed(WORD, packet[8], steps);
			}
			break;
		case LAN_X_GET_FIRMWARE_VERSION:
			ESP_LOGI(Z21_PARSER_TAG, "X_GET_FIRMWARE_VERSION");

			data[0] = 0xF3;						 //identify Firmware (not change)
			data[1] = 0x0A;						 //identify Firmware (not change)
			data[2] = z21FWVersionMSB; //V_MSB
			data[3] = z21FWVersionLSB; //V_LSB
			EthSend(client, 0x09, LAN_X_Header, data, true, Z21bcNone);
			break;
		}
		break;
	case (LAN_SET_BROADCASTFLAGS):
	{
		unsigned long bcflag = packet[7];
		bcflag = packet[6] | (bcflag << 8);
		bcflag = packet[5] | (bcflag << 8);
		bcflag = packet[4] | (bcflag << 8);
		addIPToSlot(client, getLocalBcFlag(bcflag));
		//no inside of the protokoll, but good to have:
		if (notifyz21RailPower)
			notifyz21RailPower(Railpower); //Zustand Gleisspannung Antworten
		ESP_LOGI(Z21_PARSER_TAG, "SET_BROADCASTFLAGS: ");
		//ZDebug.println(addIPToSlot(client, 0x00), BIN);
// 1=BC Power, Loco INFO, Trnt INFO; 2=BC �nderungen der R�ckmelder am R-Bus

		break;
	}
	case (LAN_GET_BROADCASTFLAGS):
	{
		unsigned long flag = getz21BcFlag(addIPToSlot(client, 0x00));
		data[0] = flag;
		data[1] = flag >> 8;
		data[2] = flag >> 16;
		data[3] = flag >> 24;
		EthSend(client, 0x08, LAN_GET_BROADCASTFLAGS, data, false, Z21bcNone);
		ESP_LOGI(Z21_PARSER_TAG, "GET_BROADCASTFLAGS: ");
		//ZDebug.println(flag, BIN);

		break;
	}
	case (LAN_GET_LOCOMODE):
		break;
	case (LAN_SET_LOCOMODE):
		break;
	case (LAN_GET_TURNOUTMODE):
		break;
	case (LAN_SET_TURNOUTMODE):
		break;
	case (LAN_RMBUS_GETDATA):
		if (notifyz21S88Data)
		{
			ESP_LOGI(Z21_PARSER_TAG, "RMBUS_GETDATA");

			//ask for group state 'Gruppenindex'
			notifyz21S88Data(packet[4]); //normal Antwort hier nur an den anfragenden Client! (Antwort geht hier an alle!)
		}
		break;
	case (LAN_RMBUS_PROGRAMMODULE):
		break;
	case (LAN_SYSTEMSTATE_GETDATA):
	{ //System state
			ESP_LOGI(Z21_PARSER_TAG,"LAN_SYS-State");

		if (notifyz21getSystemInfo)
			notifyz21getSystemInfo(client);
		break;
	}
	case (LAN_RAILCOM_GETDATA):
	{
		uint16_t Adr = 0;
		if (packet[4] == 0x01)
		{ //RailCom-Daten f�r die gegebene Lokadresse anfordern
			//Adr = word(packet[6], packet[5]);
			Adr = (((uint16_t)packet[6]) << 8) | ((uint16_t)packet[5]);
		}
		if (notifyz21Railcom)
			Adr = notifyz21Railcom(); //return global Railcom Adr
		data[0] = Adr >> 8;					//LocoAddress
		data[1] = Adr & 0xFF;				//LocoAddress
		data[2] = 0x00;							//UINT32 ReceiveCounter Empfangsz�hler in Z21
		data[3] = 0x00;
		data[4] = 0x00;
		data[5] = 0x00;
		data[6] = 0x00; //UINT32 ErrorCounter Empfangsfehlerz�hler in Z21
		data[7] = 0x00;
		data[8] = 0x00;
		data[9] = 0x00;
		/*
			  data[10] = 0x00;	//UINT8 Reserved1 experimentell, siehe Anmerkung 
			  data[11] = 0x00;	//UINT8 Reserved2 experimentell, siehe Anmerkung 
			  data[12] = 0x00;	//UINT8 Reserved3 experimentell, siehe Anmerkung 
			  */
		EthSend(client, 0x0E, LAN_RAILCOM_DATACHANGED, data, false, Z21bcNone);
		break;
	}
	case (LAN_LOCONET_FROM_LAN):
	{
		ESP_LOGI(Z21_PARSER_TAG, "LOCONET_FROM_LAN");

		if (notifyz21LNSendPacket)
		{
			uint8_t LNdata[packet[0] - 0x04]; //n Bytes
			for (uint8_t i = 0; i < (packet[0] - 0x04); i++)
				LNdata[i] = packet[0x04 + i];
			notifyz21LNSendPacket(LNdata, packet[0] - 0x04);
			//Melden an andere LAN-Client das Meldung auf LocoNet-Bus geschrieben wurde
			EthSend(client, packet[0], LAN_LOCONET_FROM_LAN, packet, false, Z21bcLocoNet_s); //LAN_LOCONET_FROM_LAN
		}
		break;
	}
	case (LAN_LOCONET_DISPATCH_ADDR):
	{
		if (notifyz21LNdispatch)
		{
			data[0] = packet[4];
			data[1] = packet[5];
			data[2] = notifyz21LNdispatch(packet[5], packet[4]); //dispatchSlot
			ESP_LOGI(Z21_PARSER_TAG, "LOCONET_DISPATCH_ADDR ");
			//ZDebug.print(word(packet[5], packet[4]));
			//ZDebug.print(",");
			//ZDebug.println(data[2]);

			EthSend(client, 0x07, LAN_LOCONET_DISPATCH_ADDR, data, false, Z21bcNone);
		}
		break;
	}
	case (LAN_LOCONET_DETECTOR):
		if (notifyz21LNdetector)
		{
			ESP_LOGI(Z21_PARSER_TAG, "LOCONET_DETECTOR Abfrage");
			uint16_t WORD = (((uint16_t)packet[6]) << 8) | ((uint16_t)packet[5]);
			notifyz21LNdetector(packet[4], WORD); //Anforderung Typ & Reportadresse
		}
		break;
	case (LAN_CAN_DETECTOR):
		if (notifyz21CANdetector)
		{
			ESP_LOGI(Z21_PARSER_TAG, "CAN_DETECTOR Abfrage");
			uint16_t WORD = (((uint16_t)packet[6]) << 8) | ((uint16_t)packet[5]);
			notifyz21CANdetector(packet[4], WORD); //Anforderung Typ & CAN-ID
		}
		break;
	case (0x12): //configuration read
		// <-- 04 00 12 00
		// 0e 00 12 00 01 00 01 03 01 00 03 00 00 00
		data[0] = 0x0e;
		data[1] = 0x00;
		data[2] = 0x12;
		data[3] = 0x00;
		data[4] = 0x01;
		data[5] = 0x00;
		data[6] = 0x01;
		data[7] = 0x03;
		data[8] = 0x01;
		data[9] = 0x00;
		data[10] = 0x03;
		data[11] = 0x00;
		data[12] = 0x00;
		//for (uint8_t i = 0; i < 10; i++)
		//{
		//data[i] = FSTORAGE.read(CONF1STORE + i);
		//}
		EthSend(client, 0x0e, 0x12, data, false, Z21bcNone);
		ESP_LOGI(Z21_PARSER_TAG, "Z21 Eins(read) ");
		/*
		ZDebug.print("RailCom: ");
		ZDebug.print(data[0], HEX);
		ZDebug.print(", PWR-Button: ");
		ZDebug.print(data[2], HEX);
		ZDebug.print(", ProgRead: ");
		switch (data[3])
		{
		case 0x00:
			ZDebug.print("nothing");
			break;
		case 0x01:
			ZDebug.print("Bit");
			break;
		case 0x02:
			ZDebug.print("Byte");
			break;
		case 0x03:
			ZDebug.print("both");
			break;
		}
		ZDebug.println();
		*/
		break;
	case (0x13):
	{ //configuration write
//<-- 0e 00 13 00 01 00 01 03 01 00 03 00 00 00
//0x0e = Length; 0x12 = Header
/* Daten:
			(0x01) RailCom: 0=aus/off, 1=ein/on
			(0x00)
			(0x01) Power-Button: 0=Gleisspannung aus, 1=Nothalt
			(0x03) Auslese-Modus: 0=Nichts, 1=Bit, 2=Byte, 3=Beides
			*/
		ESP_LOGI(Z21_PARSER_TAG, "Z21 Eins(write) ");
/*
ZDebug.print("RailCom: ");
ZDebug.print(packet[4], HEX);
ZDebug.print(", PWR-Button: ");
ZDebug.print(packet[6], HEX);
ZDebug.print(", ProgRead: ");
switch (packet[7])
{
case 0x00:
	ZDebug.print("nothing");
	break;
case 0x01:
	ZDebug.print("Bit");
	break;
case 0x02:
	ZDebug.print("Byte");
	break;
case 0x03:
	ZDebug.print("both");
	break;
		}
		ZDebug.println();
*/
		
		packet[4] = 0x0e;
		packet[5] = 0x00;
		packet[6] = 0x13;
		packet[7] = 0x00;
		packet[8] = 0x01;
		packet[9] = 0x00;
		packet[10] = 0x01;
		packet[11] = 0x03;
		packet[12] = 0x01;
		packet[13] = 0x00;
		packet[14] = 0x03;
		packet[15] = 0x00;
		packet[16] = 0x00;
		packet[17] = 0x00;

		//for (uint8_t i = 0; i < 10; i++)
		//{
		//	FSTORAGE.FSTORAGEMODE(CONF1STORE + i, packet[4 + i]);
		//}
		//Request DCC to change
		if (notifyz21UpdateConf)
			notifyz21UpdateConf();
		break;
	}
	case (0x16): //configuration read
		//<-- 04 00 16 00
		//14 00 16 00 19 06 07 01 05 14 88 13 10 27 32 00 50 46 20 4e
	data[0] = 0x14;
	data[1] = 0x00;
	data[2] = 0x16;
	data[3] = 0x00;
	data[4] = 0x19;
	data[5] = 0x06;
	data[6] = 0x07;
	data[7] = 0x01;
	data[8] = 0x05;
	data[9] = 0x14;
	data[10] = 0x88;
	data[11] = 0x13;
	data[12] = 0x10;
	data[13] = 0x27;
	data[14] = 0x32;
	data[15] = 0x00;
	data[16] = 0x50;
	data[17] = 0x46;
	data[18] = 0x20;
	data[19] = 0x4e;
	// for (uint8_t i = 0; i < 16; i++)
	//{
	//	data[i] = FSTORAGE.read(CONF2STORE + i);
	//}
	EthSend(client, 0x14, 0x16, data, false, Z21bcNone);
	ESP_LOGI(Z21_PARSER_TAG, "Z21 Eins(read) ");
	/*
		ZDebug.print("RstP(s): ");
		ZDebug.print(data[0]);
		ZDebug.print(", RstP(f): ");
		ZDebug.print(data[1]);
		ZDebug.print(", ProgP: ");
		ZDebug.print(data[2]);
		ZDebug.print(", MainV: ");
		ZDebug.print(word(data[13], data[12]));
		ZDebug.print(", ProgV: ");
		ZDebug.print(word(data[15], data[14]));
		ZDebug.println();
		*/
	break;
	case (0x17):
	{ //configuration write
//<-- 14 00 17 00 19 06 07 01 05 14 88 13 10 27 32 00 50 46 20 4e
//0x14 = Length; 0x16 = Header(read), 0x17 = Header(write)
/* Daten:
			(0x19) Reset Packet (starten) (25-255)
			(0x06) Reset Packet (fortsetzen) (6-64)
			(0x07) Programmier-Packete (7-64)
			(0x01) ?
			(0x05) ?
			(0x14) ?
			(0x88) ?
			(0x13) ?
			(0x10) ?
			(0x27) ?
			(0x32) ?
			(0x00) ?
			(0x50) Hauptgleis (LSB) (12-22V)
			(0x46) Hauptgleis (MSB)
			(0x20) Programmiergleis (LSB) (12-22V): 20V=0x4e20, 21V=0x5208, 22V=0x55F0
			(0x4e) Programmiergleis (MSB)
			*/
			ESP_LOGI(Z21_PARSER_TAG, "Z21 Eins(write) ");
			/*
ZDebug.print("RstP(s): ");
ZDebug.print(packet[4]);
ZDebug.print(", RstP(f): ");
ZDebug.print(packet[5]);
ZDebug.print(", ProgP: ");
ZDebug.print(packet[6]);
ZDebug.print(", MainV: ");
ZDebug.print(word(packet[17], packet[16]));
ZDebug.print(", ProgV: ");
ZDebug.print(word(packet[19], packet[18]));
ZDebug.println();
*/
			//for (uint8_t i = 0; i < 16; i++)
			//{
			//		FSTORAGE.FSTORAGEMODE(CONF2STORE + i, packet[4 + i]);
			//}
			packet[4] = 0x14;
			packet[5] = 0x00;
			packet[6] = 0x17;
			packet[7] = 0x00;
			packet[8] = 0x19;
			packet[9] = 0x06;
			packet[10] = 0x07;
			packet[11] = 0x01;
			packet[12] = 0x05;
			packet[13] = 0x14;
			packet[14] = 0x88;
			packet[15] = 0x13;
			packet[16] = 0x10;
			packet[17] = 0x27;
			packet[18] = 0x32;
			packet[19] = 0x00;
			packet[20] = 0x50;
			packet[21] = 0x46;
			packet[22] = 0x20;
			packet[23] = 0x4e;
			//Request DCC to change
			if (notifyz21UpdateConf)
				notifyz21UpdateConf();
			break;
	}
	default:
		ESP_LOGI(Z21_PARSER_TAG, "UNKNOWN_COMMAND");
		//	for (uint8_t i = 0; i < packet[0]; i++) {
		//		ZDebug.print(" 0x");
		//		ZDebug.print(packet[i], HEX);
		//	}

		data[0] = 0x61;
		data[1] = 0x82;
		EthSend(client, 0x07, LAN_X_Header, data, true, Z21bcNone);
	}
	//---------------------------------------------------------------------------------------
	//check if IP is still used:
	unsigned long currentMillis = (unsigned long)(esp_timer_get_time() / 1000);
	if ((currentMillis - z21IPpreviousMillis) > z21IPinterval)
	{
		z21IPpreviousMillis = currentMillis;
		for (uint8_t i = 0; i < z21clientMAX; i++)
		{
			if (ActIP[i].time > 0)
			{
				ActIP[i].time--; //Zeit herrunterrechnen
			}
			else
			{
				clearIP(i); //clear IP DATA
										//send MESSAGE clear Client
			}
		}
	}
}

//--------------------------------------------------------------------------------------------
//Zustand der Gleisversorgung setzten
void setPower(uint8_t state)
{
	uint8_t data[] = {LAN_X_BC_TRACK_POWER, 0x00};
	Railpower = state;
	switch (state)
	{
	case csNormal:
		data[1] = 0x01;
		break;
	case csTrackVoltageOff:
		data[1] = 0x00;
		break;
	case csServiceMode:
		data[1] = 0x02;
		break;
	case csShortCircuit:
		data[1] = 0x08;
		break;
	case csEmergencyStop:
		data[0] = 0x81;
		data[1] = 0x00;
		break;
	}
	EthSend(0, 0x07, LAN_X_Header, data, true, Z21bcAll_s);
	ESP_LOGI(Z21_PARSER_TAG, "set_X_BC_TRACK_POWER ");
	//ZDebug.println(state, HEX);

}

//--------------------------------------------------------------------------------------------
//Abfrage letzte Meldung �ber Gleispannungszustand
uint8_t getPower()
{
	return Railpower;
}

//--------------------------------------------------------------------------------------------
//return request for POM read uint8_t
void setCVPOMBYTE(uint16_t CVAdr, uint8_t value)
{
	uint8_t data[5];
	data[0] = 0x64;								 //X-Header
	data[1] = 0x14;								 //DB0
	data[2] = (CVAdr >> 8) & 0x3F; //CV_MSB;
	data[3] = CVAdr & 0xFF;				 //CV_LSB;
	data[4] = value;
	EthSend(0, 0x0A, LAN_X_Header, data, true, 0x00);
}

//--------------------------------------------------------------------------------------------
//Gibt aktuellen Lokstatus an Anfragenden Zur�ck
void setLocoStateFull(int Adr, uint8_t steps, uint8_t speed, uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3, bool bc)
{
	uint8_t data[9];
	data[0] = LAN_X_LOCO_INFO; //0xEF X-HEADER
	data[1] = (Adr >> 8) & 0x3F;
	data[2] = Adr & 0xFF;
	// Fahrstufeninformation: 0=14, 2=28, 4=128
	if (steps & 0x03 == DCCSTEP14)
		data[3] = 0; // 14 steps
	if (steps & 0x03 == DCCSTEP28)
		data[3] = 2; // 28 steps
	if (steps & 0x03 == DCCSTEP128)
		data[3] = 4;																													// 128 steps
	data[4] = speed;																												//DSSS SSSS
	data[5] = F0;																														//F0, F4, F3, F2, F1
	data[6] = F1;																														//F5 - F12; Funktion F5 ist bit0 (LSB)
	data[7] = F2;																														//F13-F20
	data[8] = F3;																														//F21-F28
	if (bc)																																	//BC?
		EthSend(0, 14, LAN_X_Header, data, true, Z21bcAll_s | Z21bcNetAll_s); //Send Power und Funktions to all active Apps
	else
		EthSend(0, 14, LAN_X_Header, data, true, Z21bcNone); //Send Power und Funktions to request App
}

//--------------------------------------------------------------------------------------------
//return state of S88 sensors
void setS88Data(uint8_t *data, uint8_t modules)
{
	// split data into 11 uint8_ts blocks (1 packet address + 10 data)
	uint8_t MAdr = 1;  // module number in packet
	uint8_t datasend[11]; // array holding the data to be sent (1 packet address + 10 modules data)
	datasend[0] = 0;	 // fisrt byte is the packet address
	for (uint8_t m = 0; m < modules; m++)
	{
		datasend[MAdr] = data[m];
		MAdr++;
		// check if we have filled the data buffer and send it
		if (MAdr >= 11)
		{
			EthSend(0, 0x0F, LAN_RMBUS_DATACHANGED, datasend, false, Z21bcRBus_s); //RMBUS_DATACHANED
			MAdr = 1;																															 // reset the module number in packet
			datasend[0]++;																												 //increment the next packet's address
		}
	}
	if (MAdr < 11 && MAdr > 1) // if we still have a partial packet, fill it with 0 and send
	{
		while (MAdr < 11)
		{
			datasend[MAdr] = 0x00; // 0 values
			MAdr++;								 // next module address
		}
		EthSend(0, 0x0F, LAN_RMBUS_DATACHANGED, datasend, false, Z21bcRBus_s); //RMBUS_DATACHANED
	}
}

//--------------------------------------------------------------------------------------------
//return state from LN detector
void setLNDetector(uint8_t *data, uint8_t DataLen)
{
	EthSend(0, 0x04 + DataLen, LAN_LOCONET_DETECTOR, data, false, Z21bcLocoNetGBM_s); //LAN_LOCONET_DETECTOR
}

//--------------------------------------------------------------------------------------------
//LN Meldungen weiterleiten
void setLNMessage(uint8_t *data, uint8_t DataLen, uint8_t bcType, bool TX)
{
	if (TX)																																 //Send by Z21 or Receive a Packet?
		EthSend(0, 0x04 + DataLen, LAN_LOCONET_Z21_TX, data, false, bcType); //LAN_LOCONET_Z21_TX
	else
		EthSend(0, 0x04 + DataLen, LAN_LOCONET_Z21_RX, data, false, bcType); //LAN_LOCONET_Z21_RX
}

//--------------------------------------------------------------------------------------------
//return state from CAN detector
void setCANDetector(uint16_t NID, uint16_t Adr, uint8_t port, uint8_t typ, uint16_t v1, uint16_t v2)
{
	uint8_t data[10];
	data[0] = NID & 0x08;
	data[1] = NID >> 8;
	data[2] = Adr & 0x08;
	data[3] = Adr >> 8;
	data[4] = port;
	data[5] = typ;
	data[6] = v1 & 0x08;
	data[7] = v1 >> 8;
	data[8] = v2 & 0x08;
	data[9] = v2 >> 8;
	EthSend(0, 0x0E, LAN_CAN_DETECTOR, data, false, Z21bcCANDetector_s); //CAN_DETECTOR
}

//--------------------------------------------------------------------------------------------
//Return the state of accessory
void setTrntInfo(uint16_t Adr, bool State)
{
	uint8_t data[4];
	data[0] = LAN_X_TURNOUT_INFO; //0x43 X-HEADER
	data[1] = Adr >> 8;						//High
	data[2] = Adr & 0xFF;					//Low
	data[3] = State + 1;
	//  if (State == true)
	//    data[3] = 2;
	//  else data[3] = 1;
	EthSend(0, 0x09, LAN_X_Header, data, true, Z21bcAll_s);
}

//--------------------------------------------------------------------------------------------
//Return CV Value for Programming
void setCVReturn(uint16_t CV, uint8_t value)
{
	uint8_t data[5];
	data[0] = 0x64;			 //X-Header
	data[1] = 0x14;			 //DB0
	data[2] = CV >> 8;	 //CV_MSB;
	data[3] = CV & 0xFF; //CV_LSB;
	data[4] = value;
	EthSend(0, 0x0A, LAN_X_Header, data, true, Z21bcAll_s);
}

//--------------------------------------------------------------------------------------------
//Return no ACK from Decoder
void setCVNack()
{
	uint8_t data[2];
	data[0] = 0x61; //X-Header
	data[1] = 0x13; //DB0
	EthSend(0, 0x07, LAN_X_Header, data, true, Z21bcAll_s);
}

//--------------------------------------------------------------------------------------------
//Return Short while Programming
void setCVNackSC()
{
	uint8_t data[2];
	data[0] = 0x61; //X-Header
	data[1] = 0x12; //DB0
	EthSend(0, 0x07, LAN_X_Header, data, true, Z21bcAll_s);
}

//--------------------------------------------------------------------------------------------
//Send Changing of SystemInfo
void sendSystemInfo(uint8_t client, uint16_t maincurrent, uint16_t mainvoltage, uint16_t temp)
{
	uint8_t data[16];
	data[0] = maincurrent & 0xFF; //MainCurrent mA
	data[1] = maincurrent >> 8;		//MainCurrent mA
	data[2] = data[0];						//ProgCurrent mA
	data[3] = data[1];						//ProgCurrent mA
	data[4] = data[0];						//FilteredMainCurrent
	data[5] = data[1];						//FilteredMainCurrent
	data[6] = temp & 0xFF;				//Temperature
	data[7] = temp >> 8;					//Temperature
	data[8] = mainvoltage & 0xFF; //SupplyVoltage
	data[9] = mainvoltage >> 8;		//SupplyVoltage
	data[10] = data[8];						//VCCVoltage
	data[11] = data[9];						//VCCVoltage
	data[12] = Railpower;					//CentralState
																/*Bitmasken f�r CentralState: 
	#define csEmergencyStop  0x01 // Der Nothalt ist eingeschaltet 
	#define csTrackVoltageOff  0x02 // Die Gleisspannung ist abgeschaltet 
	#define csShortCircuit  0x04 // Kurzschluss 
	#define csProgrammingModeActive 0x20 // Der Programmiermodus ist aktiv 	
*/
	data[13] = 0x00;							//CentralStateEx
																/* Bitmasken f�r CentralStateEx: 
	#define cseHighTemperature  0x01 // zu hohe Temperatur 
	#define csePowerLost  0x02 // zu geringe Eingangsspannung 
	#define cseShortCircuitExternal 0x04 // am externen Booster-Ausgang 
	#define cseShortCircuitInternal 0x08 // am Hauptgleis oder Programmiergleis 	
*/
	data[14] = 0x00;							//reserved
	data[15] = 0x00;							//reserved
	if (client > 0)
	EthSend(client, 0x14, LAN_SYSTEMSTATE_DATACHANGED, data, false, Z21bcNone);	//only to the request client
	EthSend(0, 0x14, LAN_SYSTEMSTATE_DATACHANGED, data, false, Z21bcSystemInfo_s); //all that select this message (Abo)
}

// Private Methods ///////////////////////////////////////////////////////////////////////////////////////////////////
// Functions only available to other functions in this library *******************************************************

//--------------------------------------------------------------------------------------------
void EthSend(uint8_t client, unsigned int DataLen, unsigned int Header, uint8_t *dataString, bool withXOR, uint8_t BC)
{
	uint8_t data[24]; //z21 send storage
	uint8_t clientOut = client;

	for (uint8_t i = 0; i < z21clientMAX; i++)
	{
		if ((BC == 0) || (BC == Z21bcAll_s) || ((ActIP[i].time > 0) && ((BC & ActIP[i].BCFlag) > 0)))
		{ //Boradcast & Noch aktiv

			if (BC != 0)
			{
				if (BC == Z21bcAll_s)
					clientOut = 0; //ALL
				else
					clientOut = ActIP[i].client;
			}
			//--------------------------------------------

			data[0] = DataLen & 0xFF;
			data[1] = DataLen >> 8;
			data[2] = Header & 0xFF;
			data[3] = Header >> 8;
			data[DataLen - 1] = 0; //XOR

			for (uint8_t i = 0; i < (DataLen - 5 + !withXOR); i++)
			{ //Ohne Length und Header und XOR
				if (withXOR)
					data[DataLen - 1] = data[DataLen - 1] ^ *dataString;
				//Udp->write(*dataString);
				data[i + 4] = *dataString;
				dataString++;
			}
			//--------------------------------------------
			//Udp->endPacket();
			if (notifyz21EthSend)
				notifyz21EthSend(clientOut, data); //, DataLen);

			ESP_LOGI(Z21_PARSER_TAG, "ETX ");
			uint8_t
				/*
			ZDebug.print(clientOut);
			ZDebug.print(" BC:");
			ZDebug.print(BC & ActIP[i].BCFlag, BIN);
			ZDebug.print(" : ");
			for (uint8_t i = 0; i < data[0]; i++)
			{
				ZDebug.print(data[i], HEX);
				ZDebug.print(" ");
			}
			ZDebug.println();
			*/

				if (BC == 0 || (BC == Z21bcAll_s && clientOut == 0)) //END when no BC
				return;
		}
	}
}

//--------------------------------------------------------------------------------------------
//Convert local stored flag back into a Z21 Flag
unsigned long getz21BcFlag(uint8_t flag)
{
	unsigned long outFlag = 0;
	if ((flag & Z21bcAll_s) != 0)
		outFlag |= Z21bcAll;
	if ((flag & Z21bcRBus_s) != 0)
		outFlag |= Z21bcRBus;
	if ((flag & Z21bcSystemInfo_s) != 0)
		outFlag |= Z21bcSystemInfo;
	if ((flag & Z21bcNetAll_s) != 0)
		outFlag |= Z21bcNetAll;
	if ((flag & Z21bcLocoNet_s) != 0)
		outFlag |= Z21bcLocoNet;
	if ((flag & Z21bcLocoNetLocos_s) != 0)
		outFlag |= Z21bcLocoNetLocos;
	if ((flag & Z21bcLocoNetSwitches_s) != 0)
		outFlag |= Z21bcLocoNetSwitches;
	if ((flag & Z21bcLocoNetGBM_s) != 0)
		outFlag |= Z21bcLocoNetGBM;
	return outFlag;
}

//--------------------------------------------------------------------------------------------
//Convert Z21 LAN BC flag to local stored flag
uint8_t getLocalBcFlag(uint32_t flag)
{
	uint8_t outFlag = 0;
	if ((flag & Z21bcAll) != 0)
		outFlag |= Z21bcAll_s;
	if ((flag & Z21bcRBus) != 0)
		outFlag |= Z21bcRBus_s;
	if ((flag & Z21bcSystemInfo) != 0)
		outFlag |= Z21bcSystemInfo_s;
	if ((flag & Z21bcNetAll) != 0)
		outFlag |= Z21bcNetAll_s;
	if ((flag & Z21bcLocoNet) != 0)
		outFlag |= Z21bcLocoNet_s;
	if ((flag & Z21bcLocoNetLocos) != 0)
		outFlag |= Z21bcLocoNetLocos_s;
	if ((flag & Z21bcLocoNetSwitches) != 0)
		outFlag |= Z21bcLocoNetSwitches_s;
	if ((flag & Z21bcLocoNetGBM) != 0)
		outFlag |= Z21bcLocoNetGBM_s;
	return outFlag;
}

//--------------------------------------------------------------------------------------------
// delete the stored IP-Address
void clearIP(uint8_t pos)
{
	ActIP[pos].client = 0;
	ActIP[pos].BCFlag = 0;
	ActIP[pos].time = 0;
}

//--------------------------------------------------------------------------------------------
void clearIPSlots()
{
	for (int i = 0; i < z21clientMAX; i++)
		clearIP(i);
}

//--------------------------------------------------------------------------------------------
void clearIPSlot(uint8_t client)
{
	for (int i = 0; i < z21clientMAX; i++)
	{
		if (ActIP[i].client == client)
		{
			clearIP(i);
			return;
		}
	}
}

//--------------------------------------------------------------------------------------------
uint8_t addIPToSlot(uint8_t client, uint8_t BCFlag)
{
	uint8_t Slot = z21clientMAX;
	for (uint8_t i = 0; i < z21clientMAX; i++)
	{
		if (ActIP[i].client == client)
		{
			ActIP[i].time = z21ActTimeIP;
			if (BCFlag != 0) //Falls BC Flag �bertragen wurde diesen hinzuf�gen!
				ActIP[i].BCFlag = BCFlag;
			return ActIP[i].BCFlag; //BC Flag 4. uint8_t R�ckmelden
		}
		else if (ActIP[i].time == 0 && Slot == z21clientMAX)
			Slot = i;
	}
	ActIP[Slot].client = client;
	ActIP[Slot].time = z21ActTimeIP;
	setPower(Railpower);
	return ActIP[Slot].BCFlag; //BC Flag 4. uint8_t R�ckmelden
}

// Store IP in list and return it's index
uint8_t addIP(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint16_t port)
{
	//suche ob IP schon vorhanden?
	for (uint8_t i = 0; i < storedIP; i++)
	{
		if (mem[i].IP0 == ip0 && mem[i].IP1 == ip1 && mem[i].IP2 == ip2 && mem[i].IP3 == ip3)
			return i + 1;
	}
	if (storedIP >= maxIP)
		return 0;
	mem[storedIP].IP0 = ip0;
	mem[storedIP].IP1 = ip1;
	mem[storedIP].IP2 = ip2;
	mem[storedIP].IP3 = ip3;
	mem[storedIP].port = port;
	storedIP++;
	return storedIP;
}

//--------------------------------------------------------------------------------------------
// z21 library callback functions
//--------------------------------------------------------------------------------------------
void notifyz21RailPower(uint8_t State)
{
#if defined(DEBUGSERIAL)
	DEBUGSERIAL.print("Power: ");
	DEBUGSERIAL.println(State, HEX);
#endif
	XpressNet.setPower(State);
}

//--------------------------------------------------------------------------------------------
void notifyz21EthSend(uint8_t client, uint8_t *data)
{
	if (client == 0)
	{ //all stored
		for (uint8_t i = 0; i < storedIP; i++)
		{
			IPAddress ip(mem[i].IP0, mem[i].IP1, mem[i].IP2, mem[i].IP3);
			//Udp.beginPacket(ip, mem[i].port); //Broadcast
			//Udp.write(data, data[0]);
			//Udp.endPacket();
		}
	}
	else
	{
		IPAddress ip(mem[client - 1].IP0, mem[client - 1].IP1, mem[client - 1].IP2, mem[client - 1].IP3);
		//Udp.beginPacket(ip, mem[client - 1].port); //no Broadcast
		//Udp.write(data, data[0]);
		//Udp.endPacket();
	}
}

void notifyz21getSystemInfo(uint8_t client)
{
	uint8_t data[16];
	data[0] = 0x00;					 //MainCurrent mA
	data[1] = 0x00;					 //MainCurrent mA
	data[2] = 0x00;					 //ProgCurrent mA
	data[3] = 0x00;					 //ProgCurrent mA
	data[4] = 0x00;					 //FilteredMainCurrent
	data[5] = 0x00;					 //FilteredMainCurrent
	data[6] = 0x20;					 //Temperature
	data[7] = 0x20;					 //Temperature
	data[8] = 0x0F;					 //SupplyVoltage
	data[9] = 0x00;					 //SupplyVoltage
	data[10] = 0x00;				 //VCCVoltage
	data[11] = 0x03;				 //VCCVoltage
	data[12] = XpressNet.getPower(); //CentralState
	data[13] = 0x00;				 //CentralStateEx
	data[14] = 0x00;				 //reserved
	data[15] = 0x00;				 //reserved
	notifyz21EthSend(client, data);
}

//--------------------------------------------------------------------------------------------
static void udp_server_task(void *pvParameters)
{
	char rx_buffer[128];
	char tx_buffer[128];
	char addr_str[128];
	int addr_family = (int)pvParameters;
	int ip_protocol = 0;
	struct sockaddr_in6 dest_addr;

	while (1)
	{
		struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
		dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
		dest_addr_ip4->sin_family = AF_INET;
		dest_addr_ip4->sin_port = htons(PORT);
		ip_protocol = IPPROTO_IP;

		int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
		if (sock < 0)
		{
			ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket created");

		int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
		if (err < 0)
		{
			ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
		}
		ESP_LOGI(TAG, "Socket bound, port %d", PORT);

		while (1)
		{

			ESP_LOGI(TAG, "Waiting for data");
			struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
			socklen_t socklen = sizeof(source_addr);
			int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

			// Error occurred during receiving
			if (len < 0)
			{
				ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
				break;
			}
			// Data received
			else
			{
				// Get the sender's ip address as string

				ip4addr_ntoa_r((const ip4_addr_t *)&(((struct sockaddr_in *)&source_addr)->sin_addr), addr_str, sizeof(addr_str) - 1);

				rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
				ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
				ESP_LOGI(TAG, "%s", rx_buffer);
				ESP_LOG_BUFFER_HEXDUMP(TAG, rx_buffer, len, ESP_LOG_INFO);

				int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
				if (err < 0)
				{
					ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
					break;
				}
			}
		}

		if (sock != -1)
		{
			ESP_LOGE(TAG, "Shutting down socket and restarting...");
			shutdown(sock, 0);
			close(sock);
		}
	}
	vTaskDelete(NULL);
}
