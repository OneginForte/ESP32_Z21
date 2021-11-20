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
#include "XBusInterface.h"
#include "XpressNet.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *Z21_PARSER_TAG = "Z21_PARSER";
//need to include eeprom

// Function that handles the creation and setup of instances

	void z21Class()
		{
			// initialize this instance's variables
			z21IPpreviousMillis = 0;
			Railpower = csTrackVoltageOff;
			clearIPSlots();
		}




//*********************************************************************************************
// Determine and evaluate data 
	void receive(uint8_t client, uint8_t * packet)
	{
		//ESP_LOGI(Z21_PARSER_TAG, "Parser start.");
		// ESP_LOG_BUFFER_HEXDUMP(Z21_PARSER_TAG, packet, rxlen, ESP_LOG_INFO);
		addIPToSlot(client, 0);
		// send a reply, to the IP address and port that sent us the packet we received
		int header = (packet[3] << 8) + packet[2];
		uint8_t data[16]; // z21 send storage

		//#if defined(ESP32)
		// portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
		//#endif

		switch (header)
		{
		case LAN_GET_SERIAL_NUMBER:
			ESP_LOGI(Z21_PARSER_TAG, "GET_SERIAL_NUMBER");

			// ESP_LOGI(Z21_PARSER_TAG, "Socket created");

			// data[0] = FSTORAGE.read(CONFz21SnLSB);
			// data[1] = FSTORAGE.read(CONFz21SnMSB);
			data[0] = z21SnLSB;
			data[1] = z21SnMSB;
			data[2] = 0x00;
			data[3] = 0x00;
			EthSend(client, 0x08, LAN_GET_SERIAL_NUMBER, data, false, Z21bcNone); // Seriennummer 32 Bit (little endian)
			break;
		case LAN_GET_HWINFO:
			ESP_LOGI(Z21_PARSER_TAG, "GET_HWINFO");

			data[0] = z21HWTypeLSB; // HwType 32 Bit
			data[1] = z21HWTypeMSB;
			data[2] = 0x00;
			data[3] = 0x00;
			data[4] = z21FWVersionLSB; // FW Version 32 Bit
			data[5] = z21FWVersionMSB;
			data[6] = 0x00;
			data[7] = 0x00;
			EthSend(client, 0x0C, LAN_GET_HWINFO, data, false, Z21bcNone);
			break;
		case LAN_LOGOFF:
			ESP_LOGI(Z21_PARSER_TAG, "LOGOFF");

			clearIPSlot(client);
			// Antwort von Z21: keine
			break;
		case LAN_GET_CODE: // SW Feature-Umfang der Z21
			/*#define Z21_NO_LOCK        0x00  // keine Features gesperrt
				#define z21_START_LOCKED   0x01  // �z21 start�: Fahren und Schalten per LAN gesperrt
				#define z21_START_UNLOCKED 0x02  // �z21 start�: alle Feature-Sperren aufgehoben */
			data[0] = 0x00; // keine Features gesperrt
			EthSend(client, 0x05, LAN_GET_CODE, data, false, Z21bcNone);
			break;
		case (LAN_X_Header):
			switch (packet[4])
			{ // X-Header
			case LAN_X_GET_SETTING:
				switch (packet[5])
				{ // DB0
				case 0x21:
					ESP_LOGI(Z21_PARSER_TAG, "X_GET_VERSION");

					data[0] = LAN_X_GET_VERSION; // X-Header: 0x63
					data[1] = 0x21;				 // DB0
					data[2] = 0x30;				 // X-Bus Version
					data[3] = 0x12;				 // ID der Zentrale
					EthSend(client, 0x09, LAN_X_Header, data, true, Z21bcNone);
					break;
				case 0x24:
					data[0] = LAN_X_STATUS_CHANGED; // X-Header: 0x62
					data[1] = 0x22;					// DB0
					data[2] = Railpower;			// DB1: Status
					// ZDebug.print("X_GET_STATUS ");
					// csEmergencyStop  0x01 // Der Nothalt ist eingeschaltet
					// csTrackVoltageOff  0x02 // Die Gleisspannung ist abgeschaltet
					// csShortCircuit  0x04 // Kurzschluss
					// csProgrammingModeActive 0x20 // Der Programmiermodus ist aktiv
					EthSend(client, 0x08, LAN_X_Header, data, true, Z21bcNone);
					break;
				case 0x80:
					ESP_LOGI(Z21_PARSER_TAG, "X_SET_TRACK_POWER_OFF");

					if (notifyz21RailPower)
						notifyz21RailPower(csTrackVoltageOff);
					break;
				case 0x81:
					ESP_LOGI(Z21_PARSER_TAG, "X_SET_TRACK_POWER_ON");

					data[0] = LAN_X_BC_TRACK_POWER;
					data[1] = 0x01;
					EthSend(client, 0x07, LAN_X_Header, data, true, Z21bcNone);

					if (notifyz21RailPower)
						notifyz21RailPower(csNormal);
					break;
				}
				break; // ENDE DB0
			case LAN_X_CV_READ:
				if (packet[5] == 0x11)
				{ // DB0
					ESP_LOGI(Z21_PARSER_TAG, "X_CV_READ");

					if (notifyz21CVREAD)
						notifyz21CVREAD(packet[6], packet[7]); // CV_MSB, CV_LSB
				}
				break;
			case LAN_X_CV_WRITE:
				if (packet[5] == 0x12)
				{ // DB0
					ESP_LOGI(Z21_PARSER_TAG, "X_CV_WRITE");

					if (notifyz21CVWRITE)
						notifyz21CVWRITE(packet[6], packet[7], packet[8]); // CV_MSB, CV_LSB, value
				}
				break;
			case LAN_X_CV_POM:
				if (packet[5] == 0x30)
				{ // DB0
					uint8_t Adr = ((packet[6] & 0x3F) << 8) + packet[7];
					uint8_t CVAdr = ((packet[8] & 0b11000000) << 8) + packet[9];
					uint8_t value = packet[10];
					if ((packet[8] & 0xFC) == 0xEC)
					{
						ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_WRITE_BYTE");

						if (notifyz21CVPOMWRITEBYTE)
							notifyz21CVPOMWRITEBYTE(Adr, CVAdr, value); // set decoder
					}
					else if ((packet[8] & 0xFC) == 0xE8)
					{
						ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_WRITE_BIT");
					}
					else
					{
						ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_READ_BIYTE");

						if (notifyz21CVPOMREADBYTE)
							notifyz21CVPOMREADBYTE(Adr, CVAdr); // set decoder
					}
				}
				else if (packet[5] == 0x31)
				{ // DB0
					ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_ACCESSORY");
				}
				break;
			case LAN_X_GET_TURNOUT_INFO:
			{
				ESP_LOGI(Z21_PARSER_TAG, "X_GET_TURNOUT_INFO ");

				if (notifyz21AccessoryInfo)
				{
					data[0] = 0x43;		 // X-HEADER
					data[1] = packet[5]; // High
					data[2] = packet[6]; // Low
					if (notifyz21AccessoryInfo((packet[5] << 8) + packet[6]) == true) //setCANDetector(uint16_t NID, uint16_t Adr, uint8_t port, uint8_t typ, uint16_t v1, uint16_t v2);
						data[3] = 0x02; // active
					else
						data[3] = 0x01;											// inactive
					EthSend(client, 0x09, LAN_X_Header, data, true, Z21bcNone); // BC new 23.04. !!! (old = 0)
				}
				break;
			}
			case LAN_X_SET_TURNOUT:
			{
				ESP_LOGI(Z21_PARSER_TAG, "X_SET_TURNOUT");

				// bool TurnOnOff = bitRead(packet[7],3);  //Spule EIN/AUS
				if (notifyz21Accessory)
				{
					notifyz21Accessory((packet[5] << 8) + packet[6], bitRead(packet[7], 0), bitRead(packet[7], 3));
				} //	Addresse					Links/Rechts			Spule EIN/AUS
				break;
			}
			case LAN_X_SET_EXT_ACCESSORY:
			{
				ESP_LOGI(Z21_PARSER_TAG, "X_SET_EXT_ACCESSORY RAdr");
				// setExtACCInfo((packet[5] << 8) + packet[6], packet[7]);
				break;
			}
			case LAN_X_GET_EXT_ACCESSORY_INFO:
			{
				ESP_LOGI(Z21_PARSER_TAG, "X_EXT_ACCESSORY_INFO RArd.");
				// setExtACCInfo((packet[5] << 8) + packet[6], packet[7]);
				break;
			}
			case LAN_X_SET_STOP:
				ESP_LOGI(Z21_PARSER_TAG, "X_SET_STOP");

				if (notifyz21RailPower)
					notifyz21RailPower(csEmergencyStop);
				break;
			case LAN_X_GET_LOCO_INFO:

				if (packet[5] == 0xF0)
				{ // DB0
					// ZDebug.print("X_GET_LOCO_INFO: ");
					// Antwort: LAN_X_LOCO_INFO  Adr_MSB - Adr_LSB
					// if (notifyz21getLocoState)
					//notifyz21getLocoState(((packet[6] & 0x3F) << 8) + packet[7], false);
					// uint16_t WORD = (((uint16_t)packet[6] & 0x3F) << 8) | ((uint16_t)packet[7]);
					returnLocoStateFull(client, ((packet[6] & 0x3F) << 8) + packet[7], false);
					// Antwort via "setLocoStateFull"!
				}
				break;
			case LAN_X_SET_LOCO:
				// setLocoBusy:
				ESP_LOGI(Z21_PARSER_TAG, "LAN_X_SET_LOCO aka setLocoBusy");
				// uint16_t WORD = (((uint16_t)packet[6] & 0x3F) << 8) | ((uint16_t)packet[7]);

				addBusySlot(client, ((packet[6] & 0x3F) << 8) + packet[7]);

				if (packet[5] == LAN_X_SET_LOCO_FUNCTION)
				{ // DB0
					// LAN_X_SET_LOCO_FUNCTION  Adr_MSB        Adr_LSB            Type (00=AUS/01=EIN/10=UM)      Funktion
					// word(packet[6] & 0x3F, packet[7]), packet[8] >> 6, packet[8] & 0b00111111

					if (notifyz21LocoFkt)
						notifyz21LocoFkt(((packet[6] & 0x3F) << 8) + packet[7], packet[8] >> 6, packet[8] & 0b00111111);
					// uint16_t Adr, uint8_t type, uint8_t fkt
				}
				else
				{ // DB0
					// ZDebug.print("X_SET_LOCO_DRIVE ");
					uint8_t steps = 14;
					if ((packet[5] & 0x03) == 3)
						steps = 128;
					else if ((packet[5] & 0x03) == 2)
						steps = 28;

					if (notifyz21LocoSpeed)
						notifyz21LocoSpeed(((packet[6] & 0x3F) << 8) + packet[7], packet[8], steps);
				}
				returnLocoStateFull(client, ((packet[6] & 0x3F) << 8) + packet[7], true);
				break;
			case LAN_X_GET_FIRMWARE_VERSION:
				ESP_LOGI(Z21_PARSER_TAG, "X_GET_FIRMWARE_VERSION");

				data[0] = 0xF3;			   // identify Firmware (not change)
				data[1] = 0x0A;			   // identify Firmware (not change)
				data[2] = z21FWVersionMSB; // V_MSB
				data[3] = z21FWVersionLSB; // V_LSB
				EthSend(client, 0x09, LAN_X_Header, data, true, Z21bcNone);
				break;
			case 0x73:
				// LAN_X_??? WLANmaus periodische Abfrage:
				// 0x09 0x00 0x40 0x00 0x73 0x00 0xFF 0xFF 0x00
				// length X-Header	XNet-Msg			  speed?
				ESP_LOGI(Z21_PARSER_TAG, "LAN-X_WLANmaus");

				// set Broadcastflags for WLANmaus:
				if (addIPToSlot(client, 0x00) == 0)
					addIPToSlot(client, Z21bcAll);
				break;
			default:

				ESP_LOGI(Z21_PARSER_TAG, "UNKNOWN_LAN-X_COMMAND");

				data[0] = 0x61;
				data[1] = 0x82;
				EthSend(client, 0x07, LAN_X_Header, data, true, Z21bcNone);
			}
			break;
			//---------------------- LAN X-Header ENDE ---------------------------
		case (LAN_SET_BROADCASTFLAGS):
		{
			unsigned long bcflag = packet[7];
			bcflag = packet[6] | (bcflag << 8);
			bcflag = packet[5] | (bcflag << 8);
			bcflag = packet[4] | (bcflag << 8);
			addIPToSlot(client, getLocalBcFlag(bcflag));
			// no inside of the protokoll, but good to have:
			if (notifyz21RailPower)
				notifyz21RailPower(Railpower); // Zustand Gleisspannung Antworten
			ESP_LOGI(Z21_PARSER_TAG, "SET_BROADCASTFLAGS: ");
			int* ptr = (int *)&bcflag;
			ESP_LOGI(Z21_PARSER_TAG, "%p", ptr);
			//ESP_LOG_BUFFER_CHAR(Z21_PARSER_TAG, &bcflag, 4);
			// 1=BC Power, Loco INFO, Trnt INFO; 2=BC �nderungen der R�ckmelder am R-Bus

			break;
		}
		case (LAN_GET_BROADCASTFLAGS):
		{
			uint16_t flag = getz21BcFlag(addIPToSlot(client, 0x00));
			data[0] = flag;
			data[1] = flag >> 8;
			data[2] = flag >> 16;
			data[3] = flag >> 24;
			EthSend(client, 0x08, LAN_GET_BROADCASTFLAGS, data, false, Z21bcNone);
			ESP_LOGI(Z21_PARSER_TAG, "GET_BROADCASTFLAGS: ");

			break;
		}
		case (LAN_GET_LOCOMODE):
			data[0] = packet[4];
			data[1] = packet[5];
			data[2] = 0;	//0=DCC Format; 1=MM Format
			EthSend (client, 0x07, LAN_GET_LOCOMODE, data, false, Z21bcNone);
		break;
	case (LAN_SET_LOCOMODE):
		break;
	case (LAN_GET_TURNOUTMODE):
				data[0] = packet[4];
			data[1] = packet[5];
			data[2] = 0;	//0=DCC Format; 1=MM Format
			EthSend (client, 0x07, LAN_GET_LOCOMODE, data, false, Z21bcNone);
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

			EthSend(client, 0x07, LAN_LOCONET_DISPATCH_ADDR, data, false, Z21bcNone);
		}
		break;
	}
	case (LAN_LOCONET_DETECTOR):
		if (notifyz21LNdetector)
		{
			ESP_LOGI(Z21_PARSER_TAG, "LOCONET_DETECTOR Abfrage");
			uint16_t ADDR = (((uint16_t)packet[6]) << 8) | ((uint16_t)packet[5]);
			notifyz21LNdetector(client, packet[4], ADDR); //Anforderung Typ & Reportadresse
		}
		break;
	case (LAN_CAN_DETECTOR):
		if (notifyz21CANdetector)
		{
			ESP_LOGI(Z21_PARSER_TAG, "CAN_DETECTOR Abfrage");
			uint16_t ADDR = (((uint16_t)packet[6]) << 8) | ((uint16_t)packet[5]);
			notifyz21CANdetector(client, packet[4], ADDR); //Anforderung Typ & CAN-ID
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

		break;
	case (0x13):
	{ 	//configuration write
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
		*/
		//Request DCC to change
		//if (notifyz21UpdateConf)
		//	notifyz21UpdateConf();
		break;
	}
	case (0x16): //configuration read
	{	//<-- 04 00 16 00
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
	
	break;
	}
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

		ESP_LOG_BUFFER_HEXDUMP(Z21_PARSER_TAG, packet, sizeof(packet), ESP_LOG_INFO);

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
//Convert local stored flag back into a Z21 Flag
uint32_t getz21BcFlag(uint8_t flag)
{
	uint32_t outFlag = 0;
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
//Zustand der Gleisversorgung setzten
void z21setPower(uint8_t state)
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
//Zustand R�ckmeldung non - Z21 device - Busy!
void setLocoStateExt (uint16_t Adr) 
{
	uint8_t ldata[6];
	if (notifyz21LocoState)
		notifyz21LocoState(Adr, ldata); //uint8_t Steps[0], uint8_t Speed[1], uint8_t F0[2], uint8_t F1[3], uint8_t F2[4], uint8_t F3[5]
	
	uint8_t data[9]; 
	data[0] = LAN_X_LOCO_INFO;  //0xEF X-HEADER
	data[1] = (Adr >> 8) & 0x3F;
	data[2] = Adr & 0xFF;
	// Fahrstufeninformation: 0=14, 2=28, 4=128 
	if ((ldata[0] & 0x03) == DCCSTEP14)
		data[3] = 0;	// 14 steps
	if ((ldata[0] & 0x03) == DCCSTEP28)
		data[3] = 2;	// 28 steps
	if ((ldata[0] & 0x03) == DCCSTEP128)		
		data[3] = 4;	// 128 steps
	data[3] = data[3] | 0x08; //BUSY!

	data[4] = (uint8_t)ldata[1];  // DSSS SSSS
	data[5] = (uint8_t)ldata[2];  // F0, F4, F3, F2, F1
	data[6] = (uint8_t)ldata[3];  // F5 - F12; Funktion F5 ist bit0 (LSB)
	data[7] = (uint8_t)ldata[4];  // F13-F20
	data[8] = (uint8_t)ldata[5];  // F21-F28

	reqLocoBusy(Adr);
	
	EthSend(0, 14, LAN_X_Header, data, true, Z21bcAll_s | Z21bcNetAll_s);  //Send Loco Status und Funktions to all active Apps 
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
	globalPower(Railpower) ;//z21setPower(Railpower);
	return ActIP[Slot].BCFlag; //BC Flag 4. uint8_t R�ckmelden
}

//--------------------------------------------------------------------------------------------
//check if there are slots with the same loco, set them to busy
void setOtherSlotBusy(uint8_t slot)
{
	for (uint8_t i = 0; i < z21clientMAX; i++)
	{
		if ((i != slot) && (ActIP[slot].adr == ActIP[i].adr))
		{					  //if in other Slot -> set busy
			ActIP[i].adr = 0; //clean slot that informed as busy & let it activ
							  //Inform with busy message:
							  //not used!
		}
	}
}

//--------------------------------------------------------------------------------------------
//Add loco to slot.
void addBusySlot(uint8_t client, uint16_t adr)
{
	for (uint8_t i = 0; i < z21clientMAX; i++)
	{
		if (ActIP[i].client == client)
		{
			if (ActIP[i].adr != adr)
			{						 //skip is already used by this client
				ActIP[i].adr = adr;	 //store loco that is used
				setOtherSlotBusy(i); //make other busy
			}
			break;
		}
	}
}


//--------------------------------------------------------------------------------------------
// used by non Z21 client
void reqLocoBusy(uint16_t adr)
{
	for (uint8_t i = 0; i < z21clientMAX; i++)
	{
		if (adr == ActIP[i].adr)
		{
			ActIP[i].adr = 0; // clear
		}
	}
}


// Store IP in list and return it's index

uint8_t Z21addIP(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint16_t port)
{
	//suche ob IP schon vorhanden?
	for (uint8_t i = 0; i < storedIP; i++)
	{
		if (mem[i].IP0 == ip0 && mem[i].IP1 == ip1 && mem[i].IP2 == ip2 && mem[i].IP3 == ip3 && mem[i].port == port)
		{
			mem[i].time = z21ActTimeIP; //setzte Zeit
			return i + 1;
		}
	}
	if (storedIP >= z21clientMAX)
	{
		for (uint8_t i = 0; i < storedIP; i++)
		{
			if (mem[i].time == 0)
			{ //Abgelaufende IP, dort eintragen!
				mem[i].IP0 = ip0;
				mem[i].IP1 = ip1;
				mem[i].IP2 = ip2;
				mem[i].IP3 = ip3;
				mem[i].port = port;
				mem[i].time = z21ActTimeIP; //setzte Zeit
				return i + 1;
			}
		}
		return 0; //keine freien IPs (never reach here!)
	}
	mem[storedIP].IP0 = ip0;
	mem[storedIP].IP1 = ip1;
	mem[storedIP].IP2 = ip2;
	mem[storedIP].IP3 = ip3;
	mem[storedIP].port = port;
	mem[storedIP].time = z21ActTimeIP; //setzte Zeit
	storedIP++;
	return storedIP;
}

//--------------------------------------------------------------------------------------------
//aktuellen Zustand aller Funktionen und Speed der Lok
void getLocoData(uint16_t adr, uint8_t data[])
{
	//uint8_t Steps, uint8_t Speed, uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3
	uint8_t Slot = LokStsgetSlot(adr);
	data[0] = LokDataUpdate[Slot].adr >> 14; //Steps
	data[1] = LokDataUpdate[Slot].speed;
	data[2] = LokDataUpdate[Slot].f0 & 0x1F; //F0 - F4
	data[3] = LokDataUpdate[Slot].f1;
	data[4] = LokDataUpdate[Slot].f2;
	data[5] = LokDataUpdate[Slot].f3;
}
//--------------------------------------------------------------------------------------------
//Gibt aktuelle Fahrtrichtung der Angefragen Lok zur�ck
uint8_t getLocoDir(uint16_t adr)
{
	return bitRead(LokDataUpdate[LokStsgetSlot(adr)].speed, 7);
}
//--------------------------------------------------------------------------------------------
//Gibt aktuelle Geschwindigkeit der Angefragen Lok zur�ck
uint8_t getLocoSpeed(uint16_t adr)
{
	return LokDataUpdate[LokStsgetSlot(adr)].speed & 0x7F;
}
//--------------------------------------------------------------------------------------------
uint8_t LokStsgetSlot(uint16_t adr) // gibt Slot f�r Adresse zur�ck / erzeugt neuen Slot (0..126)
{
	uint8_t Slot;
	for (Slot = 0; Slot < SlotMax; Slot++)
	{
		if ((LokDataUpdate[Slot].adr & 0x3FFF) == adr)
			return Slot; // Lok gefunden, Slot ausgeben
		if ((LokDataUpdate[Slot].adr & 0x3FFF) == 0)
		{
			// Empty? neuer freier Slot - keine weitern Lok's!
			LokStsSetNew(Slot, adr); // Eintragen
			return Slot;
		}
	}
	// kein Slot mehr vorhanden!
	// start am Anfang mit dem �berschreiben vorhandender Slots
	uint8_t zugriff = 0xFF;
	for (int i = 0; i < SlotMax; i++) {
		if (LokDataUpdate[i].state < zugriff) {
			Slot = i;
			zugriff = LokDataUpdate[i].state;
		}
	}
	Slot = slotFullNext;
	LokStsSetNew(Slot, adr); // clear Slot!
	slotFullNext++;
	if (slotFullNext >= SlotMax)
		slotFullNext = 0;
	return Slot;
}


bool setSpeed14(uint16_t address, uint8_t speed)
{
	if (address == 0) // check if Adr is ok?
		return false;

	uint8_t slot = LokStsgetSlot(address);
	LokDataUpdate[slot].speed = speed;			  // write Dir and Speed into register to SAVE
	if ((LokDataUpdate[slot].adr >> 14) != DCC14) // 0=>14steps, write speed steps into register
		LokDataUpdate[slot].adr = (LokDataUpdate[slot].adr & 0x3FFF) | (DCC14 << 14);

	//uint8_t speed_data_uint8_ts[] = {0x40}; // speed indecator
	/*
	if (speed == 1) //estop!
		//return eStop(address);//
		speed_data_uint8_ts[0] |= 0x01; //estop
	else if (speed == 0) //regular stop!
		speed_data_uint8_ts[0] |= 0x00; //stop
	else //movement
		speed_data_uint8_ts[0] |= map(speed, 2, 127, 2, 15); //convert from [2-127] to [1-14]
	speed_data_uint8_ts[0] |= (0x20 * bitRead(speed, 7)); //flip bit 3 to indicate direction;
	*/
	//speed_data_uint8_ts[0] |= speed & 0x1F;		   // 5 Bit Speed
	//speed_data_uint8_ts[0] |= (speed & 0x80) >> 2; // Dir

	/*
	DCCPacket p(address);
	p.addData(speed_data_uint8_ts, 1);

	p.setRepeat(SPEED_REPEAT);

	p.setKind(speed_packet_kind);

	// speed packets get refreshed indefinitely, and so the repeat doesn't need to be set.
	// speed packets go to the high proirity queue
	// return(high_priority_queue.insertPacket(&p));
	if (railpower == ESTOP) // donot send to rails now!
		return periodic_refresh_queue.insertPacket(&p);
	return repeat_queue.insertPacket(&p);
	*/
	return true;
}

bool setSpeed28(uint16_t address, uint8_t speed)
{
	if (address == 0) // check if Adr is ok?
		return false;

	uint8_t slot = LokStsgetSlot(address);
	LokDataUpdate[slot].speed = speed;			  // speed & B01111111 + Dir;	//write into register to SAVE
	if ((LokDataUpdate[slot].adr >> 14) != DCC28) // 2=>28steps, write into register
		LokDataUpdate[slot].adr = (LokDataUpdate[slot].adr & 0x3FFF) | (DCC28 << 14);

	//uint8_t speed_data_uint8_ts[] = {0x40}; // Speed indecator
	/*
	if(speed == 1) //estop!
	  //return eStop(address);//
	  speed_data_uint8_ts[0] |= 0x01; //estop
	else if (speed == 0) //regular stop!
	  speed_data_uint8_ts[0] |= 0x00; //stop
	else //movement
	{
	  speed_data_uint8_ts[0] |= map(speed, 2, 127, 2, 0x1F); //convert from [2-127] to [2-31]
	  //most least significant bit has to be shufled around
	  speed_data_uint8_ts[0] = (speed_data_uint8_ts[0]&0xE0) | ((speed_data_uint8_ts[0]&0x1F) >> 1) | ((speed_data_uint8_ts[0]&0x01) << 4);
	}
	speed_data_uint8_ts[0] |= (0x20 * bitRead(speed, 7)); //flip bit 3 to indicate direction;
	*/
	//speed_data_uint8_ts[0] |= speed & 0x1F;		   // 5 Bit Speed
	//speed_data_uint8_ts[0] |= (speed & 0x80) >> 2; // Dir
/*
	DCCPacket p(address);
	p.addData(speed_data_uint8_ts, 1);

	p.setRepeat(SPEED_REPEAT);

	p.setKind(speed_packet_kind);

	// speed packets get refreshed indefinitely, and so the repeat doesn't need to be set.
	// speed packets go to the high proirity queue
	// return(high_priority_queue.insertPacket(&p));
	if (railpower == ESTOP) // donot send to rails now!
		return periodic_refresh_queue.insertPacket(&p);
	return repeat_queue.insertPacket(&p);
	*/
	return true;
}

bool setSpeed128(uint16_t address, uint8_t speed)
{
	if (address == 0)
	{
		//	Serial.println("ERROR ADR0");
		return false;
	}
	uint8_t slot = LokStsgetSlot(address);
	LokDataUpdate[slot].speed = speed;			   // write Speed and Dir into register to SAVE
	if ((LokDataUpdate[slot].adr >> 14) != DCC128) // 3=>128steps, write into register
		LokDataUpdate[slot].adr = (LokDataUpdate[slot].adr & 0x3FFF) | (DCC128 << 14);

	//uint8_t speed_data_uint8_ts[] = {0x3F, 0x00};

	//	if (speed == 1) //estop!
	//		return eStop(address);//speed_data_uint8_ts[1] |= 0x01; //estop
	// else
	//speed_data_uint8_ts[1] = speed; // no conversion necessary.

	// why do we get things like this?
	//  03 3F 16 15 3F (speed packet addressed to loco 03)
	//  03 3F 11 82 AF  (speed packet addressed to loco 03, speed hex 0x11);
	/*
	DCCPacket p(address);
	p.addData(speed_data_uint8_ts, 2);

	p.setRepeat(SPEED_REPEAT);

	p.setKind(speed_packet_kind);

	// speed packets get refreshed indefinitely, and so the repeat doesn't need to be set.
	// speed packets go to the high proirity queue

	// return(high_priority_queue.insertPacket(&p));
	if (railpower == ESTOP) // donot send to rails now!
		return periodic_refresh_queue.insertPacket(&p);
	return repeat_queue.insertPacket(&p);
	*/
	return true;
}

//--------------------------------------------------------------------------------------------
void setSpeed(uint16_t Adr, uint8_t Steps, uint8_t Speed)
{
	//Locomotive speed and direction operation
	// 0xE4 | Ident | AH | AL | RV | XOr
	// Ident: 0x10 = F14; 0x11 = F27; 0x12 = F28; 0x13 = F128
	// RV = RVVV VVVV Dirction and Speed
	uint8_t v = Speed;
	if (Steps == Loco28 || Steps == Loco27)
	{
		v = (Speed & 0x0F) << 1;  //Speed Bit
		v |= (Speed >> 4) & 0x01; //Addition Speed Bit
		v |= 0x80 & Speed;		  //Dir
	}
	uint8_t LocoInfo[] = {0x00, 0xE4, 0x13, 0x00, 0x00, v, 0x00}; //default to 128 Steps!
	switch (Steps)
	{
	case 14:
		LocoInfo[2] = 0x10;
		break;
	case 27:
		LocoInfo[2] = 0x11;
		break;
	case 28:
		LocoInfo[2] = 0x12;
		break;
	}
	if (Adr > 99) //Xpressnet long addresses (100 to 9999: AH/AL = 0xC064 to 0xE707)
		LocoInfo[3] = (Adr >> 8) | 0xC0;
	else
		LocoInfo[3] = Adr >> 8; //short addresses (0 to 99: AH = 0x0000 and AL = 0x0000 to 0x0063)
	LocoInfo[4] = Adr & 0xFF;
	getXOR(LocoInfo, 7);
	XNettransmit(LocoInfo, 7);
}

bool dccsetSpeed(uint16_t address, uint8_t speed)
{
	// set Loco to speed with default settings!
	switch (DCCdefaultSteps)
	{
	case 14:
		return (setSpeed14(address, speed));
	case 28:
		return (setSpeed28(address, speed));
	case 128:
		return (setSpeed128(address, speed));
	}
	return false; // invalid number of steps specified.
}

//--------------------------------------------------------------------------------------------
void LokStsSetNew(uint8_t Slot, uint16_t adr) // Neue Lok eintragen mit Adresse
{
	LokDataUpdate[Slot].adr = adr | (DCCdefaultSteps << 14); // 0x4000; //0xC000;	// c = '3' => 128 Fahrstufen
	LokDataUpdate[Slot].mode = 0b1011;	//Busy und 128 Fahrstufen
	LokDataUpdate[Slot].speed = 0x80;						 // default direction is forward
	LokDataUpdate[Slot].f0 = 0x00;
	LokDataUpdate[Slot].f1 = 0x00;
	LokDataUpdate[Slot].f2 = 0x00;
	LokDataUpdate[Slot].f3 = 0x00;
	LokDataUpdate[Slot].state = 0x00;

// generate first drive information:
	dccsetSpeed(LokDataUpdate[Slot].adr, LokDataUpdate[Slot].speed);
}

//--------------------------------------------------------------------------------------------
void notifyz21LocoState(uint16_t Adr, uint8_t data[])
{
	getLocoData(Adr, data);
}

//--------------------------------------------------------------------------------------------
//Gibt aktuellen Lokstatus an Anfragenden Zuruck
void returnLocoStateFull(uint8_t client, uint16_t Adr, bool bc) 
//bc = true => to inform also other client over the change.
//bc = false => just ask about the loco state
{
	ESP_LOGI(Z21_PARSER_TAG, "returnLocoStateFull:");
	uint8_t ldata[6];
	if (notifyz21LocoState)
		notifyz21LocoState(Adr, ldata); //uint8_t Steps[0], uint8_t Speed[1], uint8_t F0[2], uint8_t F1[3], uint8_t F2[4], uint8_t F3[5]
	
	uint8_t data[9]; 
	data[0] = LAN_X_LOCO_INFO;  //0xEF X-HEADER
	data[1] = (Adr >> 8) & 0x3F;
	data[2] = Adr & 0xFF;
	// Fahrstufeninformation: 0=14, 2=28, 4=128 
	if ((ldata[0] & 0x03) == DCCSTEP14)
		data[3] = 0;	// 14 steps
	if ((ldata[0] & 0x03) == DCCSTEP28)
		data[3] = 2;	// 28 steps
	if ((ldata[0] & 0x03) == DCCSTEP128)		
		data[3] = 4;	// 128 steps
	data[3] = data[3] | 0x08; //BUSY!
		
	data[4] = (char) ldata[1];	//DSSS SSSS
	data[5] = (char) ldata[2];  //F0, F4, F3, F2, F1
	data[6] = (char) ldata[3];  //F5 - F12; Funktion F5 ist bit0 (LSB)
	data[7] = (char) ldata[4];  //F13-F20
	data[8] = (char) ldata[5];  //F21-F28

	//ESP_LOG_BUFFER_HEXDUMP(Z21_PARSER_TAG, (uint8_t *)&data, 9, ESP_LOG_INFO);
	//Info to all:
	for (uint8_t i = 0; i < z21clientMAX; i++) {
		if (ActIP[i].client != client) {
			if ((ActIP[i].BCFlag & (Z21bcAll_s | Z21bcNetAll_s)) > 0) {
				if (bc == true)
					EthSend (ActIP[i].client, 14, LAN_X_Header, data, true, Z21bcNone);  //Send Loco status und Funktions to BC Apps
				ESP_LOGI(Z21_PARSER_TAG, "to ALL!");
			}
		}
		else { //Info to client that ask:
			if (ActIP[i].adr == Adr) {
				data[3] = data[3] & 0b111;	//clear busy flag!
			}
			EthSend (client, 14, LAN_X_Header, data, true, Z21bcNone);  //Send Loco status und Funktions to request App
			ESP_LOGI(Z21_PARSER_TAG, "to Client!");
			data[3] = data[3] | 0x08; //BUSY!
		}
	}
	
}


//--------------------------------------------------------------------------------------------
uint8_t getFunktion0to4(uint16_t address)	//gibt Funktionszustand - F0 F4 F3 F2 F1 zurьck
{
	return LokDataUpdate[LokStsgetSlot(address)].f0 & 0x1F;
}

uint8_t getFunktion5to8(uint16_t address)	//gibt Funktionszustand - F8 F7 F6 F5 zurьck
{
	return LokDataUpdate[LokStsgetSlot(address)].f1 & 0x0F;
}

uint8_t getFunktion9to12(uint16_t address)	//gibt Funktionszustand - F12 F11 F10 F9 zurьck
{
	return LokDataUpdate[LokStsgetSlot(address)].f1 >> 4;
}

uint8_t getFunktion13to20(uint16_t address)	//gibt Funktionszustand F20 - F13 zurьck
{
	return LokDataUpdate[LokStsgetSlot(address)].f2;
}

uint8_t getFunktion21to28(uint16_t address)	//gibt Funktionszustand F28 - F21 zurьck
{
	return LokDataUpdate[LokStsgetSlot(address)].f3;
}
//--------------------------------------------------------------------------------------------

bool setFunctions0to4(uint16_t address, uint8_t functions)
{
	if (address == 0)	//check if Adr is ok?
		return false;


  uint8_t data[] = { 0x80 };
  
  //Obnoxiously, the headlights (F0, AKA FL) are not controlled
  //by bit 0, but in DCC via bit 4. !
  data[0] |= functions & 0x1F;		//new - normal way of DCC! F0, F4, F3, F2, F1



  LokDataUpdate[LokStsgetSlot(address)].f0 = functions & 0x1F;	//write into register to SAVE

  //return low_priority_queue.insertPacket(&p);
  return true;
}


bool setFunctions5to8(uint16_t address, uint8_t functions)
{
	if (address == 0)	//check if Adr is ok?
		return false;

  uint8_t data[] = { 0xB0 };
  data[0] |= functions & 0x0F;

  LokDataUpdate[LokStsgetSlot(address)].f1 = (LokDataUpdate[LokStsgetSlot(address)].f1 | 0x0F) & (functions | 0xF0);	//write into register to SAVE

  //return low_priority_queue.insertPacket(&p);
  return true;
}

bool setFunctions9to12(uint16_t address, uint8_t functions)
{
	if (address == 0)	//check if Adr is ok?
		return false;

  //uint8_t data[] = { 0xA0 };
  //least significant four functions (F5--F8)
  //data[0] |= functions & 0x0F;
  
  LokDataUpdate[LokStsgetSlot(address)].f1 = (LokDataUpdate[LokStsgetSlot(address)].f1 | 0xF0) & ((functions << 4) | 0x0F);	//write into register to SAVE

  //return low_priority_queue.insertPacket(&p);
  return true;
}

bool setFunctions13to20(uint16_t address, uint8_t functions)	//F20 F19 F18 F17 F16 F15 F14 F13
{
	if (address == 0)	//check if Adr is ok?
		return false;

	//uint8_t data[] = { 0b11011110, 0x00 }; 
	//data[1] = functions;	//significant functions (F20--F13)

	LokDataUpdate[LokStsgetSlot(address)].f2 = functions; //write into register to SAVE
	//return low_priority_queue.insertPacket(&p);
	return true;
}

bool setFunctions21to28(uint16_t address, uint8_t functions)	//F28 F27 F26 F25 F24 F23 F22 F21
{
	if (address == 0)	//check if Adr is ok?
		return false;

	//int8_t data[] = { 0b11011111, 0x00}; 
	//data[1] = functions; //significant functions (F28--F21)

	LokDataUpdate[LokStsgetSlot(address)].f3 = functions; //write into register to SAVE
	//return low_priority_queue.insertPacket(&p);
	return true;
}
/*
//--------------------------------------------------------------------------------------------
// Gibt aktuellen Lokstatus an Anfragenden zurьck
void getLocoStateFull(uint16_t adr)
{
	uint8_t Slot = LokStsgetSlot(adr);
	uint8_t Speed = LokDataUpdate[Slot].speed;
	if (notifyLokAll)
		notifyLokAll(adr, LokDataUpdate[Slot].adr >> 14, Speed, LokDataUpdate[Slot].f0 & 0x1F,
					 LokDataUpdate[Slot].f1, LokDataUpdate[Slot].f2, LokDataUpdate[Slot].f3);
}

*/
//--------------------------------------------------------------------------------------------
//Gibt aktuellen Lokstatus an Anfragenden Zur�ck
void getLocoStateFull(uint16_t Addr, bool bc)
{
	uint8_t Slot = LokStsgetSlot(Addr);
	uint8_t Busy = bitRead(LokDataUpdate[Slot].mode, 3);
	uint8_t Dir = bitRead(LokDataUpdate[Slot].f0, 5);
	uint8_t F0 = LokDataUpdate[Slot].f0 & 0b00011111;
	uint8_t F1 = LokDataUpdate[Slot].f1;
	uint8_t F2 = LokDataUpdate[Slot].f2;
	uint8_t F3 = LokDataUpdate[Slot].f3;
	//if (notifyLokAll)
		notifyLokAll(Slot, highByte(Addr), lowByte(Addr), Busy, LokDataUpdate[Slot].mode & 0b11, LokDataUpdate[Slot].speed, Dir, F0, F1, F2, F3, bc);
	//Nutzung protokollieren:
	if (LokDataUpdate[Slot].state < 0xFF)
		LokDataUpdate[Slot].state++; //aktivit�t
}


//--------------------------------------------------------------------------------------------
//Lokfunktion setzten
void setLocoFunc(uint16_t address, uint8_t type, uint8_t fkt)
{			//type => 0 = AUS; 1 = EIN; 2 = UM; 3 = error
	bool ok = false;	//Funktion wurde nicht gesetzt!
	bool fktbit = 0;	//neue zu дndernde fkt bit
	if (type == 1)	//ein
		fktbit = 1;
	uint8_t Slot = LokStsgetSlot(address);
	uint8_t Adr_High = highByte(address);
	uint8_t Adr_Low = lowByte(address);
	//zu дnderndes bit bestimmen und neu setzten:
	if (fkt <= 4) {
		uint8_t func = LokDataUpdate[Slot].f0 & 0x1F;	//letztes Zustand der Funktionen 000 F0 F4..F1
		if (type == 2) { //um
			if (fkt == 0)
				fktbit = !(bitRead(func, 4));
			else fktbit = !(bitRead(func, fkt - 1));
		}
		if (fkt == 0)
			bitWrite(func, 4, fktbit);
		else
			bitWrite(func, fkt - 1, fktbit);
		//Daten senden:
		//Daten �ber XNet senden:

		uint8_t setLocoFunc[] = {0xE4, 0x20, Adr_High, Adr_Low, func, 0x00}; //Gruppe1 = 0 0 0 F0 F4 F3 F2 F1
		if (address> 99)
			setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNettransmit(setLocoFunc, 6);

		setFunctions0to4(address, func);	//func = 0 0 0 F0 F4 F3 F2 F1
	}
	else if ((fkt >= 5) && (fkt <= 8)) {
		uint8_t funcG2 = LokDataUpdate[Slot].f1 & 0x0F;	//letztes Zustand der Funktionen 0000 F8..F5
		if (type == 2) //um
			fktbit = !(bitRead(funcG2, fkt - 5));
		bitWrite(funcG2, fkt - 5, fktbit);
		//Daten �ber XNet senden:

		uint8_t setLocoFunc[] = {0xE4, 0x21, Adr_High, Adr_Low, funcG2, 0x00}; //Gruppe2 = 0 0 0 0 F8 F7 F6 F5
		if (address > 99)
			setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNettransmit(setLocoFunc, 6);
		//Daten senden:
		setFunctions5to8(address, funcG2);	//funcG2 = 0 0 0 0 F8 F7 F6 F5
	}
	else if ((fkt >= 9) && (fkt <= 12)) {
		uint8_t funcG3 = LokDataUpdate[Slot].f1 >> 4;	//letztes Zustand der Funktionen 0000 F12..F9
		if (type == 2) //um
			fktbit = !(bitRead(funcG3, fkt - 9));
		bitWrite(funcG3, fkt - 9, fktbit);
		//Daten �ber XNet senden:
		uint8_t setLocoFunc[] = {0xE4, 0x22, Adr_High, Adr_Low, funcG3, 0x00}; //Gruppe3 = 0 0 0 0 F12 F11 F10 F9
		
		if (address > 99) setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNettransmit(setLocoFunc, 6);
		//Daten senden:
		setFunctions9to12(address, funcG3); 	//funcG3 = 0 0 0 0 F12 F11 F10 F9
	}
	else if ((fkt >= 13) && (fkt <= 20)) {
		uint8_t funcG4 = LokDataUpdate[Slot].f2;
		if (type == 2) //um
			fktbit = !(bitRead(funcG4, fkt - 13));
		bitWrite(funcG4, fkt - 13, fktbit);
		//Daten �ber XNet senden:
		//unsigned char setLocoFunc[] = {0xE4, 0x23, Adr_High, Adr_Low, funcG4, 0x00};	//Gruppe4 = F20 F19 F18 F17 F16 F15 F14 F13
		uint8_t setLocoFunc[] = {0xE4, 0xF3, Adr_High, Adr_Low, funcG4, 0x00}; //Gruppe4 = F20 F19 F18 F17 F16 F15 F14 F13
		//0xF3 = undocumented command is used when a mulitMAUS is controlling functions f20..f13.
		
		if (address > 99)
			setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNettransmit(setLocoFunc, 6);
		//Daten senden:
		setFunctions13to20(address, funcG4);	//funcG4 = F20 F19 F18 F17 F16 F15 F14 F13
	}
	else if ((fkt >= 21) && (fkt <= 28)) {
		uint8_t funcG5 = LokDataUpdate[Slot].f3;
		if (type == 2) //um
			fktbit = !(bitRead(funcG5, fkt - 21));
		bitWrite(funcG5, fkt - 21, fktbit);
		//Daten �ber XNet senden:
		uint8_t setLocoFunc[] = {0xE4, 0x28, Adr_High, Adr_Low, funcG5, 0x00}; //Gruppe5 = F28 F27 F26 F25 F24 F23 F22 F21
		
		if (address > 99)
			setLocoFunc[2] = Adr_High | 0xC0;
		getXOR(setLocoFunc, 6);
		ok = XNettransmit(setLocoFunc, 6);
		//Daten senden:
		setFunctions21to28(address, funcG5);	//funcG5 = F28 F27 F26 F25 F24 F23 F22 F21
	}
	getLocoStateFull(address, true);	//Alle aktiven Gerдte Senden!
	//setLocoStateExt(address);
}
//--------------------------------------------------------------------------------------------
void notifyz21LocoFkt(uint16_t Adr, uint8_t state, uint8_t fkt)
{
  setLocoFunc(Adr, state, fkt); 
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
void setLNDetector(uint8_t client, uint8_t *data, uint8_t DataLen)
{
	EthSend(client, 0x04 + DataLen, LAN_LOCONET_DETECTOR, data, false, Z21bcLocoNet_s); //LAN_LOCONET_DETECTOR
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
void z21setCVNack()
{
	uint8_t data[2];
	data[0] = LAN_X_CV_NACK; //0x61 X-Header
	data[1] = 0x13;			 //DB0
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
	if (client > 0){
	EthSend(client, 0x14, LAN_SYSTEMSTATE_DATACHANGED, data, false, Z21bcNone);	//only to the request client
	}else
	{
	EthSend(0, 0x14, LAN_SYSTEMSTATE_DATACHANGED, data, false, Z21bcSystemInfo_s); //all that select this message (Abo)
	}
}

// Private Methods ///////////////////////////////////////////////////////////////////////////////////////////////////
// Functions only available to other functions in this library *******************************************************
//void EthSend(uint8_t client, unsigned int DataLen, unsigned int Header, uint8_t *dataString, bool withXOR, uint8_t BC);
//--------------------------------------------------------------------------------------------
void EthSend(uint8_t client, uint16_t DataLen, uint16_t Header, uint8_t *dataString, bool withXOR, uint8_t BC) {
	uint8_t data[24]; 			//z21 send storage
	
	//--------------------------------------------        
	//XOR bestimmen:
	data[0] = DataLen & 0xFF;
	data[1] = DataLen >> 8;
	data[2] = Header & 0xFF;
	data[3] = Header >> 8;
	data[DataLen - 1] = 0;	//XOR

    for (uint8_t i = 0; i < (DataLen-5+!withXOR); i++) { //Ohne Length und Header und XOR
        if (withXOR)
			data[DataLen-1] = data[DataLen-1] ^ *dataString;
		data[i+4] = *dataString;
        dataString++;
    }
   //--------------------------------------------        		
   if (client > 0) {
		if (notifyz21EthSend)
			notifyz21EthSend(client, data, DataLen);
   }
   else {
	uint8_t clientOut = client;
	for (uint8_t i = 0; i < z21clientMAX; i++) {
		if ( (ActIP[i].time > 0) && ( (BC & ActIP[i].BCFlag) > 0) ) {    //Boradcast & Noch aktiv

		  if (BC != 0) {
			if (BC == Z21bcAll_s)
				clientOut = 0;	//ALL
			else clientOut = ActIP[i].client;
		  }
		  
		  //--------------------------------------------
		  if (notifyz21EthSend)
			  notifyz21EthSend(clientOut, data, DataLen);

		  if (clientOut == 0){
		  		ESP_LOGI(Z21_PARSER_TAG, "Eth bcast sended...");
				return;
		  }
		}
	}
  }
	//ESP_LOGI(Z21_PARSER_TAG, "Eth sended...");
}
//--------------------------------------------------------------
//Change Power Status
void notifyXNetPower(uint8_t State)
{
	if (Railpower != State)
	{
		globalPower(State);
	}
}

//--------------------------------------------------------------------------------------------
void notifyLokAll(uint8_t slot, uint8_t Adr_High, uint8_t Adr_Low, bool Busy, uint8_t Steps, uint8_t Speed, uint8_t Direction, uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3, bool Req ) {
	ESP_LOGI(Z21_PARSER_TAG, "notifyLokAll...");
	uint8_t DB2 = Steps;
	if (DB2 == 3) //nicht vorhanden!
		DB2 = 4;
	if (Busy)
		bitWrite(DB2, 3, 1);
	uint8_t DB3 = Speed;
	if (Direction == 1)
		bitWrite(DB3, 7, 1);
	uint8_t data[9];
	data[0] = 0xEF; //X-HEADER
	data[1] = Adr_High & 0x3F;
	data[2] = Adr_Low;
	data[3] = DB2;
	data[4] = DB3;
	data[5] = F0;	  //F0, F4, F3, F2, F1
	data[6] = F1;	  //F5 - F12; Funktion F5 ist bit0 (LSB)
	data[7] = F2;	  //F13-F20
	data[8] = F3;	  //F21-F28
	if (Req == false) //kein BC
		//void EthSend (uint8_t client, unsigned int DataLen, unsigned int Header, uint8_t *dataString, bool withXOR, uint8_t BC)
		EthSend(slot, 14, 0x40, data, true, 0x00); //Send Power und Funktions ask App
	else
		EthSend(0, 14, 0x40, data, true, 0x01); //Send Power und Funktions to all active Apps 
}

//--------------------------------------------------------------------------------------------
//POWER set configuration:
void globalPower(uint8_t state)
{
	if (Railpower != state)
	{

		setCVNackSC(); //response SHORT while Service Mode!

		Railpower = state;

		switch (state)
		{
		case csNormal:

			XpressNetsetPower(Railpower); //send to XpressNet

			break;
		case csTrackVoltageOff:

			z21setPower(Railpower);

			break;
		case csServiceMode:

			z21setPower(Railpower); //already on!

			break;
		case csShortCircuit:

			z21setPower(Railpower); //shut down via GO/STOP just for the Roco Booster

			break;
		case csEmergencyStop:

			// dcc.eStop();
			XpressNetsetPower(Railpower); //send to XpressNet

			break;
		}
		if (Railpower == csShortCircuit)
		{
			z21setPower(Railpower);

			XpressNetsetPower(Railpower); //send to XpressNet
		}
	}
}
