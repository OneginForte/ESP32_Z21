/*

  Z21 Ethernet and LocoNet Master  
  Use the Z21 App as a Throttle controller for your Intellibox via LocoNet DIRECT
  
  Write Data direct into the controled SLOT of the Loco in the Master!

- LocoNet with Timer 1 (at MEGA Timer 5) and Loconet.h library
- XpressNet via LOOP-Function with XpressNetMaster.h library

 */
 
//************************************************************
//Z21 LAN Protokoll V1.05 (21.01.2015)
//Firmware-Version der Z21:
#define FWVersionMSB 0x01
#define FWVersionLSB 0x26
//Hardware-Typ der Z21: 0x00000201 // Z21 (Hardware-Variante ab 2013)
#define HWTypeMSB 0x02
#define HWTypeLSB 0x01

//************************************************************
//To see DATA on Serial, reduce the SLOT Array size for UNO use first!!
//#define DEBUG      

//************************************************************
//Website to configure IP Address and Number of S88 Bus Module
//#define HTTPCONF   //Website to configure IP Address

//************************************************************
//XpressNet Master Interface
#define XPRESSNET  
#include <XpressNetMaster.h>

//---------------------------------------------------------------------------- 
#define maxIP 50        //Speichergröße für IP-Adressen (APPs and PC)
#define maxSlot 120     //Speicher LocoNet Slot, max. 120 (1 - 119 for active locos)
//---------------------------------------------------------------------------- 

#include <LocoNet.h>

uint8_t mac[6] = {0x54,0x55,0x58,0x10,0x00,0x25};
uint8_t ip[4] = {192,168,188,111};

//For use with Arduino_UIP and Enc28j60
//#define UseEnc28
//#include <UIPEthernet.h>

//For use with Standard W5100 Library
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library

#define localPort 21105      // Z21 local port to listen on

EthernetUDP Udp;

#if defined(HTTPCONF)
EthernetServer server(80);  // (port 80 is default for HTTP)
#endif

// buffers for receiving and sending data
#define UDP_TX_MAX_SIZE 10
unsigned char packetBuffer[UDP_TX_MAX_SIZE]; //buffer to hold incoming packet

#define ActTimeIP 10    //Aktivhaltung einer IP für (sec./2)
#define interval 2000   //interval at milliseconds

long previousMillis = 0;        // will store last time of IP decount updated

struct TypeActIP {
  byte ip0;    // Byte IP
  byte ip1;    // Byte IP
  byte ip2;    // Byte IP
  byte ip3;    // Byte IP
//  byte BCFlag;  //BoadCastFlag 4. Byte Speichern
  byte time;  //Zeit
};
TypeActIP ActIP[maxIP];    //Speicherarray für IPs

// certain global XPressnet status indicators
#define csNormal 0x00 // Normal Operation Resumed ist eingeschaltet
#define csEmergencyStop 0x02 // Der Nothalt ist eingeschaltet
#define csTrackVoltageOff 0x01 // Die Gleisspannung ist abgeschaltet
#define csShortCircuit 0x04 // Kurzschluss
#define csServiceMode 0x20 // Der Programmiermodus ist aktiv - Service Mode
uint8_t power = csTrackVoltageOff;

//LocoNet:
#define LNTxPin 7    //Sending Pin for LocoNet
lnMsg        *LnPacket;
LnBuf        LnTxBuffer;

/*Slots:
Nr. 	    Description
0 	    dispatch
1 - 119     active locos
120 - 127   reserved for System and Master control
123 	    Fast Clock
124 	    Programming Track
127 	    Command Station Options */

struct SlotType {
  byte STAT1;    //Status
  byte ADR;    //Adresse LOW
  byte SPD;    //Geschwindigkeit 
  byte DIRF;    //0,0, DIR, F0, F4, F3, F2, F1
//  byte TRK;  //
//  byte SS2;  //Slot Status 2
  byte ADR2;  //Adress HIGH
  byte SND;  //F8, F7, F6, F5
//  byte ID1;  //Fahrregler/PC ID1
//  byte ID2;  //Fahrregler/PC ID2
};
SlotType SlotData[maxSlot];    //Speicherarray für Slot DATA

//XpressNet
#if defined(XPRESSNET)
  XpressNetMasterClass XpressNet;
  //Pin Config:
  #define XNetTxRxPin  9       //Port for Send/Receive on MAX485
  #define XNetFahrstufen Loco128    //Standard Fahrstufen Setup
#endif

//--------------------------------------------------------------------------------------------
void setup() {
  #if defined(UseEnc28)
    /* Disable SD card */
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
  #endif  
  
  Ethernet.begin(mac,IPAddress(ip));

  byte success = Udp.begin(localPort);
  
  #if defined(HTTPCONF)
    server.begin();    //HTTP Server
  #endif

  #if defined(DEBUG)
    Serial.begin(115200); 
    Serial.print("Z21-LocoNet ");
    Serial.println(success ? "success" : "failed");
    Serial.print("RAM: ");
    Serial.println(freeRam());  
  #endif

  // First initialize the LocoNet interface
  LocoNet.init(LNTxPin);

  #if defined(XPRESSNET)  
    XpressNet.setup(XNetFahrstufen, XNetTxRxPin);    //Initialisierung XNet Serial und Send/Receive-PIN  
  #endif
}

//--------------------------------------------------------------------------------------------
void loop() {
  
  Ethreceive();
  
  #if WebConfig
    Webconfig();    //Webserver for Configuration
  #endif

  #if defined(XPRESSNET)  
    XpressNet.update();
  #endif 
 
  LNreceive();  
  
  //Nicht genutzte IP's aus Speicher löschen
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;   
    for (int i = 0; i < maxIP; i++) {
      if (ActIP[i].ip3 != 0) {  //Slot nicht leer?
        if (ActIP[i].time > 0) 
          ActIP[i].time--;    //Zeit herrunterrechnen
        else {
          #if defined(DEBUG) 
            Serial.print("Clear IP ");
            Serial.println(ActIP[i].ip3);
          #endif  
          clearIPSlot(i);   //clear IP DATA
        }
      }
    }
  }
}

//--------------------------------------------------------------------------------------------
void clearIPSlots() {
  for (int i = 0; i < maxIP; i++)
    clearIPSlot(i);
}

//--------------------------------------------------------------------------------------------
//Slot mit Nummer "i" löschen
void clearIPSlot(byte i) {
  ActIP[i].ip0 = 0;
  ActIP[i].ip1 = 0;
  ActIP[i].ip2 = 0;
  ActIP[i].ip3 = 0;
//  ActIP[i].BCFlag = 0;
  ActIP[i].time = 0;
}

//--------------------------------------------------------------------------------------------
byte addIPToSlot (byte ip0, byte ip1, byte ip2, byte ip3, byte BCFlag) {
  byte Slot = maxIP;
  for (byte i = 0; i < maxIP; i++) {
    if (ActIP[i].ip0 == ip0 && ActIP[i].ip1 == ip1 && ActIP[i].ip2 == ip2 && ActIP[i].ip3 == ip3) {
      ActIP[i].time = ActTimeIP;
//      if (BCFlag != 0)    //Falls BC Flag übertragen wurde diesen hinzufügen!
//        ActIP[i].BCFlag = BCFlag;
      return i;    //Slot of IP
    }
    else if (ActIP[i].time == 0 && Slot == maxIP)
      Slot = i;
  }
  clearIPSlot(Slot);
  ActIP[Slot].ip0 = ip0;
  ActIP[Slot].ip1 = ip1;
  ActIP[Slot].ip2 = ip2;
  ActIP[Slot].ip3 = ip3;
  ActIP[Slot].time = ActTimeIP;
  Z21sendPower();
  return Slot;   //Slot of IP
}

//--------------------------------------------------------------------------------------------
void Ethreceive() {
  if(Udp.parsePacket() > 0) {  //packetSize
    Udp.read(packetBuffer,UDP_TX_MAX_SIZE);  // read the packet into packetBufffer
    byte IPSlot = addIPToSlot(Udp.remoteIP()[0],Udp.remoteIP()[1],Udp.remoteIP()[2],Udp.remoteIP()[3], 0x00);
    // send a reply, to the IP address and port that sent us the packet we received
    //    int datalen = (packetBuffer[1]<<8) + packetBuffer[0];
//    byte data[7] = {0, 0, 0, 0, 0, 0, 0}; 
    switch (word(packetBuffer[3], packetBuffer[2])) {  //HEADER
    case 0x10: {
      #if defined(DEBUG) 
        Serial.println("LAN_GET_SERIAL_NUMBER");  
      #endif
      byte sn[4] = {0xF3, 0x0A, 0, 0}; //Seriennummer 32 Bit (little endian)
      EthSend (0x08, 0x10, sn, false, 0x00);
      break; }
    case 0x1A: {
      #if defined(DEBUG) 
        Serial.println("LAN_GET_HWINFO"); 
      #endif
      byte HWinfo[] = {HWTypeLSB,HWTypeMSB,0x00,0x00, FWVersionLSB,FWVersionMSB,0x00,0x00};  //HwType 32 Bit, FW Version 32 Bit
      EthSend (0x0C, 0x1A, HWinfo, false, 0x00);
      break; } 
    case 0x30:
      #if defined(DEBUG) 
        Serial.println("LAN_LOGOFF"); 
      #endif
      clearIPSlot(IPSlot);
      //Antwort von Z21: keine
      break; 
      case (0x40):
      switch (packetBuffer[4]) { //X-Header
      case 0x21: 
        switch (packetBuffer[5]) {  //DB0
        case 0x21: {
          #if defined(DEBUG) 
            Serial.println("LAN_X_GET_VERSION"); 
          #endif
          byte XVer[] = {0x63,0x21, 0x30, 0x12}; 
          //3.Byte = X-Bus Version
          //4. Byte = ID der Zentrale
          EthSend (0x09, 0x40, XVer, true, 0x00);
          break; }
        case 0x24: { //LAN_X_GET_STATUS
          byte state[] = {0x62,0x22, power};
          EthSend (0x08, 0x40, state, true, 0x00);
          break; }
        case 0x80:
            #if defined(DEBUG) 
              Serial.println("LAN_X_SET_TRACK_POWER_OFF"); 
            #endif
            power = csTrackVoltageOff;
            LNsendPower();
            #if defined(XPRESSNET)
              XpressNet.setPower(power);
            #endif
          break;
        case 0x81:
            #if defined(DEBUG) 
              Serial.println("LAN_X_SET_TRACK_POWER_ON"); 
            #endif
            power = csNormal;
            LNsendPower();
            #if defined(XPRESSNET)
              XpressNet.setPower(power);
            #endif
          break;  
        }
        break;
      case 0x23:
/*        if (packetBuffer[5] == 0x11) {  //DB0
          #if defined(DEBUG) 
            Serial.println("LAN_X_CV_READ"); 
          #endif
          byte CV_MSB = packetBuffer[6];
          byte CV_LSB = packetBuffer[7];
          XpressNet.readCVMode(CV_LSB+1);
        }  */
        break;             
      case 0x24:
/*        if (packetBuffer[5] == 0x12) {  //DB0
          #if defined(DEBUG) 
            Serial.println("LAN_X_CV_WRITE"); 
          #endif
          byte CV_MSB = packetBuffer[6];
          byte CV_LSB = packetBuffer[7];
          byte value = packetBuffer[8]; 
          XpressNet.writeCVMode(CV_LSB+1, value);
        }  */
        break;             
      case 0x43:
        #if defined(DEBUG) 
          Serial.println("LAN_X_GET_TURNOUT_INFO"); 
        #endif
//        XpressNet.getTrntInfo(packetBuffer[5], packetBuffer[6]);
        break;             
      case 0x53:
        #if defined(DEBUG) 
          Serial.println("LAN_X_SET_TURNOUT"); 
        #endif
//        XpressNet.setTrntPos(packetBuffer[5], packetBuffer[6], packetBuffer[7] & 0x0F);
        break;  
      case 0x80:
          #if defined(DEBUG) 
            Serial.println("LAN_X_SET_STOP"); 
          #endif
          power = csEmergencyStop;
          LNsendPower();
          #if defined(XPRESSNET)
            XpressNet.setPower(power);
          #endif
        break;  
      case 0xE3:
        if (packetBuffer[5] == 0xF0) {  //DB0
          #if defined(DEBUG) 
            Serial.print("LAN_X_GET_LOCO_INFO: "); 
            Serial.println(word(packetBuffer[6] & 0x3F, packetBuffer[7]));  //mit F1-F12
          #endif 
          
          sendLokAllInfo(packetBuffer[6] & 0x3F, packetBuffer[7], false);
        }
        break;  
      case 0xE4:
        if (packetBuffer[5] == 0xF8) {  //DB0
          #if defined(DEBUG) 
            Serial.print("LAN_X_SET_LOCO_FUNCTION: "); 
            Serial.print(packetBuffer[8] & B00011111);   //Funktionsindex
            if (packetBuffer[8] >> 5 == B10)
              Serial.println(" um");
            else {
              if (packetBuffer[8] >> 5 == B01)
                Serial.println(" ein");  
              else Serial.println(" aus");    
            }
          #endif
          
          byte Slot = getLNSlot(packetBuffer[6] & 0x3F, packetBuffer[7]);
          if (Slot == 0 || (packetBuffer[8] & B00011111 > 8)) //kein Slot or F9-F28 => STOP
            break;
          //packetBuffer[8] & B00011111 = NNNNNN Funktionsindex, 0=F0 (Licht), 1=F1 usw.
          byte setBit = packetBuffer[8] & B00011111;  //Nr der Funktion
          boolean snd = false;  //Funktion F5-F8
          if (setBit == 0)
            setBit = 4;  //F0
          else { 
             if (setBit < 5)  //F1 bis F4
               setBit = setBit - 1;
             else {
               if (setBit < 9) {  //F4 bis F8
                setBit = setBit - 4;
                snd = true;  
               }
               else break;  //!STOP bei Funktion größer F9
             }
          }
           //DB2 Bit 6+7 = TT Umschalttyp: 00=aus, 01=ein, 10=umschalten,11=nicht erlaubt
          if (packetBuffer[8] >> 5 == B10) {   //um
            if (snd)
              bitWrite(SlotData[Slot].SND, setBit, !bitRead(SlotData[Slot].SND, setBit));
            else bitWrite(SlotData[Slot].DIRF, setBit, !bitRead(SlotData[Slot].DIRF, setBit));
          }
          else {  // ein oder aus?
            if (packetBuffer[8] >> 5 == B01) { // ein
              if (snd)
                bitWrite(SlotData[Slot].SND, setBit, 1);
              else bitWrite(SlotData[Slot].DIRF, setBit, 1);  
            }
            else {  //00 = aus
              if (snd)
                bitWrite(SlotData[Slot].SND, setBit, 0);
              else bitWrite(SlotData[Slot].DIRF, setBit, 0); 
            }
          }
          if (snd)
            sendLNSND(Slot);
          else sendLNDIRF(Slot);
//          sendLNSlot(Slot); 
        }
        else {
          #if defined(DEBUG) 
            Serial.print("LAN_X_SET_LOCO_DRIVE: "); 
            Serial.print(word(packetBuffer[6] & 0x3F, packetBuffer[7])); 
            Serial.print(", SPD: ");
            Serial.print(packetBuffer[8]);
            Serial.print(", Steps:");
            Serial.println(packetBuffer[5] & 0x0F);  //0|1 = 14; 2 = 28; 3 = 128;
          #endif 
          byte Slot = getLNSlot(packetBuffer[6] & 0x3F, packetBuffer[7]);
          if (Slot == 0)
            break;
//          SlotData[Slot].SPD = packetBuffer[8] & B1111111;  //SPD
          sendLNSPD(Slot, packetBuffer[8]);
          if (bitRead(SlotData[Slot].DIRF, 5) != bitRead(packetBuffer[8], 7)) { //Change DIR
            bitWrite(SlotData[Slot].DIRF, 5, bitRead(packetBuffer[8], 7)); 
            sendLNDIRF(Slot);
          }
//          sendLNSlot(Slot); 

          //LAN_X_SET_LOCO_DRIVE            Adr_MSB          Adr_LSB      DB0          Dir+Speed
          //XpressNet.setLocoDrive(packetBuffer[6] & 0x3F, packetBuffer[7], packetBuffer[5] & B11, packetBuffer[8]);       
        }
        break;  
      case 0xE6:  /*
        if (packetBuffer[5] == 0x30) {  //DB0
          if (Option == 0xEC) {
            #if defined(DEBUG) 
              Serial.println("LAN_X_CV_POM_WRITE_BYTE"); 
            #endif
          }
          if (Option == 0xE8) {
            #if defined(DEBUG) 
              Serial.println("LAN_X_CV_POM_WRITE_BIT"); 
            #endif
            //Nicht von der APP Unterstützt
          }
        }
*/        
        break;  
      case 0xF1: {
        #if defined(DEBUG) 
          Serial.println("LAN_X_GET_FIRMWARE_VERSION"); 
        #endif
        byte FWVer[] = {0xF3, 0x0A, FWVersionMSB, FWVersionLSB}; //3. = V_MSB; 4. = V_LSB
        EthSend (0x09, 0x40, FWVer, true, 0x00);
        break;  }   
      }
      break; 
      case (0x50):
/*        #if defined(DEBUG) 
          Serial.print("LAN_SET_BROADCASTFLAGS: "); 
          Serial.println(packetBuffer[4], BIN); // 1=BC Power, Loco INFO, Trnt INFO; 2=BC Änderungen der Rückmelder am R-Bus
        #endif   
        addIPToSlot(Udp.remoteIP()[0], Udp.remoteIP()[1], Udp.remoteIP()[2], Udp.remoteIP()[3], packetBuffer[4]);
*/        
        Z21sendPower(); //Zustand Gleisspannung Antworten
      break;
      case (0x51):
/*        #if defined(DEBUG) 
          Serial.println("LAN_GET_BROADCASTFLAGS"); 
        #endif
        data[3] = ActIP[IPSlot].BCFlag;   //BC Flag 4. Byte Rückmelden
        EthSend (0x08, 0x51, data, false, 0x00);   */
      break;
      case (0x60): //LAN_GET_LOCOMODE
      break;
      case (0x61): //LAN_SET_LOCOMODE
      break;
      case (0x70): //LAN_GET_TURNOUTMODE
      break;
      case (0x71): //LAN_SET_TURNOUTMODE
      break;
      case (0x81):
/*        #if defined(DEBUG) 
          Serial.println("LAN_RMBUS_GETDATA"); 
        #endif
        S88sendon = 'm';    //Daten werden gemeldet!
        notifyS88Data(); */
      break;
      case (0x82): //LAN_RMBUS_PROGRAMMODULE
      break;
      case (0x85): //LAN_SYSTEMSTATE_GETDATA  
/*        data[0] = 0x00;  //MainCurrent mA
        data[1] = 0x00;  //MainCurrent mA
        data[2] = 0x00;  //ProgCurrent mA
        data[3] = 0x00;  //ProgCurrent mA        
        data[4] = 0x00;  //FilteredMainCurrent
        data[5] = 0x00;  //FilteredMainCurrent
        data[6] = 0x00;  //Temperature
        data[7] = 0x20;  //Temperature
        data[8] = 0x0F;  //SupplyVoltage
        data[9] = 0x00;  //SupplyVoltage
        data[10] = 0x00;  //VCCVoltage
        data[11] = 0x03;  //VCCVoltage
        data[12] = power;  //CentralState
        data[13] = 0x00;  //CentralStateEx
        data[14] = 0x00;  //reserved
        data[15] = 0x00;  //reserved        
        EthSend (0x14, 0x84, data, false, 0x00);   */
      break;
      case (0x89): //LAN_RAILCOM_GETDATA
      break;
      case (0xA0):
        #if defined(DEBUG) 
          Serial.println("LAN_LOCONET_RX"); 
        #endif
      break;
      case (0xA1):
        #if defined(DEBUG) 
          Serial.println("LAN_LOCONET_TX"); 
        #endif
      break;
      case (0xA2):
        #if defined(DEBUG) 
          Serial.println("LAN_LOCONET_FROM_LAN"); 
        #endif
      break;
      case (0xA3):
        #if defined(DEBUG) 
          Serial.println("LAN_LOCONET_DISPATCH_ADDR"); 
        #endif  
      break;
      case (0xA4):
        #if defined(DEBUG) 
          Serial.println("LAN_LOCONET_DETECTOR");   
        #endif  
      break;
    default: 
      #if defined(DEBUG) 
        Serial.print("LAN_UNKNOWN_COMMAND 0x"); 
        Serial.println(word(packetBuffer[3], packetBuffer[2]), HEX);  //Header
      #endif
      byte unknown[] = {0x61, 0x82};
      EthSend (0x07, 0x40, unknown, true, 0x00);
    }
  }
  Udp.flush();
}

//--------------------------------------------------------------------------------------------
void EthSend (unsigned int DataLen, unsigned int Header, byte *dataString, boolean withXOR, byte BC) {
    IPAddress IPout = Udp.remoteIP();
    for (byte i = 0; i < maxIP; i++) {
      if (ActIP[i].time > 0) { // && ActIP[i].BCFlag >= BC) {    //Noch aktiv?
        IPout[0] = ActIP[i].ip0;
        IPout[1] = ActIP[i].ip1;
        IPout[2] = ActIP[i].ip2;
        IPout[3] = ActIP[i].ip3;

      if (BC == 0)
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());    //no Broadcast
      else Udp.beginPacket(IPout, Udp.remotePort());    //Broadcast
      //--------------------------------------------        
      Udp.write(DataLen & 0xFF);
      Udp.write(DataLen >> 8);
      Udp.write(Header & 0xFF);
      Udp.write(Header >> 8);
    
      unsigned char XOR = 0;
      byte ldata = DataLen-5;  //Ohne Length und Header und XOR
      if (!withXOR)    //XOR vorhanden?
        ldata++;
      for (byte i = 0; i < (ldata); i++) {
        XOR = XOR ^ *dataString;
        Udp.write(*dataString);
        dataString++;
      }
      if (withXOR)
        Udp.write(XOR);
      //--------------------------------------------
      Udp.endPacket();
      if (BC == 0)  //END when no BC
        return;
    }
  }
}

//--------------------------------------------------------------------------------------------
void LNreceive() {
  // Check for any received LocoNet packets
  LnPacket = LocoNet.receive() ;
  if( LnPacket )
  {
    #if defined(DEBUG) 
      // First print out the packet in HEX
      Serial.print("RX: ");
      uint8_t msgLen = getLnMsgSize(LnPacket); 
      for (uint8_t x = 0; x < msgLen; x++)
      {
        uint8_t val = LnPacket->data[x];
          // Print a leading 0 if less than 16 to make 2 HEX digits
        if(val < 16)
          Serial.print('0');
          
        Serial.print(val, HEX);
        Serial.print(' ');
      }
      Serial.println();
    #endif
    switch (LnPacket->data[0])  {  //OP-Code
      case OPC_GPOFF: power = csTrackVoltageOff;
                      Z21sendPower();
                      #if defined(XPRESSNET)
                        XpressNet.setPower(power);
                      #endif
                      #if defined(DEBUG) 
                        Serial.println("LN Power OFF"); 
                      #endif
                      break;
      case OPC_GPON:  power = csNormal;
                      Z21sendPower();
                      #if defined(XPRESSNET)
                        XpressNet.setPower(power);
                      #endif
                      #if defined(DEBUG) 
                        Serial.println("LN Power ON"); 
                      #endif
                      break; 
      case OPC_IDLE:  //B'cast emerg. STOP
                      power = csEmergencyStop;
                      Z21sendPower();
                      #if defined(XPRESSNET)
                        XpressNet.setPower(power);
                      #endif
                      #if defined(DEBUG) 
                        Serial.println("LN EmStop"); 
                      #endif
                      break;
      case OPC_SL_RD_DATA: {
                      byte Slot = LnPacket->data[2];  //SLOT#
                      if (Slot >= maxSlot)
                        break;
                      SlotData[Slot].STAT1 = LnPacket->data[3];
                      SlotData[Slot].ADR = LnPacket->data[4];
                      SlotData[Slot].SPD = LnPacket->data[5];
                      SlotData[Slot].DIRF = LnPacket->data[6];
                    //  SlotData[Slot].TRK = LnPacket->data[7];
                    //  SlotData[Slot].SS2 = LnPacket->data[8];
                      SlotData[Slot].ADR2 = LnPacket->data[9];
                      SlotData[Slot].SND = LnPacket->data[10];
                    //  SlotData[Slot].ID1 = LnPacket->data[11];
                    //  SlotData[Slot].ID2 = LnPacket->data[12];
                      #if defined(DEBUG) 
                        Serial.print(Slot); 
                        Serial.print(" Slot data resp, Steps:");
                        Serial.print(SlotData[Slot].STAT1 & B111, BIN);
                        if (bitRead(SlotData[Slot].DIRF, 5) == 1)
                          Serial.print(" RWD, SPD:");
                        else Serial.print(" FWD, SPD:");  
                        Serial.println(SlotData[Slot].SPD);
                      #endif
                      sendLokAllInfo (SlotData[Slot].ADR2, SlotData[Slot].ADR, true);
                      break; }
       case OPC_LOCO_SPD: 
                      if ((LnPacket->data[1] >= maxSlot) && (SlotData[LnPacket->data[1]].SPD != LnPacket->data[2]))
                        break;     
                      SlotData[LnPacket->data[1]].SPD = LnPacket->data[2];
                      sendLokAllInfo (SlotData[LnPacket->data[1]].ADR2, SlotData[LnPacket->data[1]].ADR, true);
                      break;
       case OPC_LOCO_DIRF: 
                      if (LnPacket->data[1] >= maxSlot && SlotData[LnPacket->data[1]].DIRF != LnPacket->data[2])
                        break;     
                      SlotData[LnPacket->data[1]].DIRF = LnPacket->data[2];
                      sendLokAllInfo (SlotData[LnPacket->data[1]].ADR2, SlotData[LnPacket->data[1]].ADR, true);
                      break;
       case OPC_LOCO_SND: 
                      if (LnPacket->data[1] >= maxSlot && SlotData[LnPacket->data[1]].SND != LnPacket->data[2])
                        break; 
                      SlotData[LnPacket->data[1]].SND = LnPacket->data[2];
                      sendLokAllInfo (SlotData[LnPacket->data[1]].ADR2, SlotData[LnPacket->data[1]].ADR, true);
                      break; 
       case OPC_LONG_ACK:
                      #if defined(DEBUG) 
                        Serial.print("0x");
                        Serial.print(LnPacket->data[1], HEX); 
                        Serial.println(" Long ack"); 
                      #endif 
                      break;       
    }
     // If this packet was not a Switch or Sensor Message then print a new line 
//    if(!LocoNet.processSwitchSensorMessage(LnPacket)) {
//    }
  }
}

//--------------------------------------------------------------------------------------------------
void sendLokAllInfo (byte Adr2, byte Adr1, boolean bc) {
   byte Slot = getLNSlot( Adr2, Adr1);
   if (Slot == 0)  //Check if Data in Slot 
     return;
   #if defined(DEBUG) 
    Serial.print(Slot); 
    Serial.println(" Z21 LocoInfo"); 
   #endif 

   /*  //Speed step info:
    D2-SL_SPDEX
    D1-SL_SPD14
    D0-SL_SPD28
        010=14 step MODE
        001=28 step. Generate Trinary packets for this Mobile ADR
        000=28 step/ 3 BYTE PKT regular mode
        011=128 speed mode packets
        111=128 Step decoder, Allow Advanced DCC consisting
        100=28 Step decoder, Allow Advanced DCC consisting  */
  byte data[7];
  data[0] = 0xEF;  //X-HEADER
  data[1] = SlotData[Slot].ADR2 & 0x3F;  //Adr_High
  data[2] = SlotData[Slot].ADR;  //Adr_Low
  data[3] = SlotData[Slot].STAT1 & B111;  //Fahrstufeninformation: 0=14, 2=28, 4=128
/*  if (data[3] == B010)  //14 Fahrstufen
    data[3] = 0;
  else {
    if (data[3] == B011 || data[3] == B111)  //128 Fahrstufen
      data[3] = 4;
    else data[3] = 2;  //28 Fahrstufen  
  }
  */
  data[3] = 4; //128 Fahrstufen!!!
  data[4] = SlotData[Slot].SPD;  //Speed
  bitWrite(data[4], 7, bitRead(SlotData[Slot].DIRF, 5));  //DIR
  data[5] = SlotData[Slot].DIRF & B11111;    //0,0,0, F0, F4, F3, F2, F1
  data[6] = SlotData[Slot].SND;    //SND=0,0,0,0,F8,F7,F6,F5; normal:F12, ... , F5; Funktion F5 ist bit0 (LSB)
//  data[7] = 0x00;  //F13-F20
//  data[8] = 0x00;  //F21-F28
  if (bc == false)
    EthSend (12, 0x40, data, true, 0x00);  //Send Power und Funktions ask App  
  else EthSend (12, 0x40, data, true, 0x01);  //Send Power und Funktions to all active Apps 
}
/*
//--------------------------------------------------------------------------------------------
//send changed data back to Master
void sendLNSlot (byte Slot) {
  byte setSlot[] = {OPC_WR_SL_DATA, 0x0E, Slot, SlotData[Slot].STAT1, SlotData[Slot].ADR, SlotData[Slot].SPD, SlotData[Slot].DIRF, 
    SlotData[Slot].TRK, SlotData[Slot].SS2, SlotData[Slot].ADR2, SlotData[Slot].SND, SlotData[Slot].ID1, SlotData[Slot].ID2, 0x00 };
  LNSendPacket(setSlot, 0x0E);  //Write slot data.
  #if defined(DEBUG) 
    Serial.print(Slot); 
    Serial.println(" Write slot data"); 
  #endif
}
*/
//--------------------------------------------------------------------------------------------
//Set slot speed
void sendLNSPD (byte Slot, byte SPD) {
//  SlotData[Slot].SPD = SPD & B1111111;  //Save
  byte setSPD[] = {OPC_LOCO_SPD, Slot, SPD & B1111111, 0x00};
  LNSendPacket(setSPD, 4);  
  #if defined(DEBUG) 
    Serial.print(Slot); 
    Serial.println(" Set slot SPD"); 
  #endif
}

//--------------------------------------------------------------------------------------------
//Set slot direction, function 0-4 state 
void sendLNDIRF (byte Slot) {
  byte setDIRF[] = {OPC_LOCO_DIRF, Slot, SlotData[Slot].DIRF, 0x00};
  LNSendPacket(setDIRF, 4);  
  #if defined(DEBUG) 
    Serial.print(Slot); 
    Serial.println(" Set slot DIRF"); 
  #endif
}

//--------------------------------------------------------------------------------------------
//Set slot second function
void sendLNSND (byte Slot) {
  byte setSND[] = {OPC_LOCO_SND, Slot, SlotData[Slot].SND, 0x00};
  LNSendPacket(setSND, 4);  
  #if defined(DEBUG) 
    Serial.print(Slot); 
    Serial.println(" Set slot SND"); 
  #endif
}

//--------------------------------------------------------------------------------------------
//Find Slot via Address
byte getLNSlot (byte Adr2, byte Adr1) {
  if (Adr1 == 0 && Adr2 == 0)
    return 0;
  for (byte Slot = 1; Slot < maxSlot; Slot++) {
    if ((SlotData[Slot].ADR == Adr1) && (SlotData[Slot].ADR2 == Adr2)) { // && (getLNSlotState(Slot) != 0)) {
//      byte getSlotData[] = { OPC_RQ_SL_DATA, Slot, 0x00, 0x00 }; //Request slot data/status block 
//      LNSendPacket(getSlotData, 4);
      return Slot;    //Vorhanden!
    }
  }
  //If not: Master puts Address into a SLOT
  byte getSlot[] = {OPC_LOCO_ADR, Adr2, Adr1, 0x00};  //Request loco address
  LNSendPacket(getSlot, 4);  
  return 0;
}

/*
//--------------------------------------------------------------------------------------------
//read state out of Slot
byte getLNSlotState (byte Slot) {
  /*SlotData[Slot].STAT1 -> 2 BITS for BUSY/ACTIVE
    D5-SL_BUSY
    D4-SL_ACTIVE
        11=IN_USE loco adr in SLOT -REFRESHED
        10=IDLE loco adr in SLOT, not refreshed
        01=COMMON loco adr IN SLOT, refreshed
        00=FREE SLOT, no valid DATA, not refreshed 
  return (SlotData[Slot].STAT1 >> 4) & B11;
}
*/

//--------------------------------------------------------------------------------------------
//send Z21 POWER Configuration:
void Z21sendPower() {
  byte data[] = { 0x61, 0x00 };
  switch (power) {
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
  EthSend(0x07, 0x40, data, true, 0x01);
}

//--------------------------------------------------------------------------------------------------
//send LN POWER Configuration:
void LNsendPower() {
  byte code[] = { OPC_GPOFF, 0x00};
  if (power == csNormal)
    code[0] = OPC_GPON;
  if (power == csEmergencyStop)
    code[0] = OPC_IDLE;  //B'cast emerg. STOP
    
  LNSendPacket(code,2);  
}

//--------------------------------------------------------------------------------------------------
//send bytes into LocoNet
bool LNSendPacket (byte *data, byte length) 
{
  byte XOR = 0xFF;
  for (byte i = 0; i < (length-1); i++) {
    addByteLnBuf( &LnTxBuffer, *data);
    XOR = XOR ^ *data;
    data++;
  }
  addByteLnBuf( &LnTxBuffer, XOR ) ;    //Trennbit
  addByteLnBuf( &LnTxBuffer, 0xFF ) ;    //Trennbit
  // Check to see if we have received a complete packet yet
  LnPacket = recvLnMsg( &LnTxBuffer );    //Senden vorbereiten
  if(LnPacket ) {        //korrektheit Prüfen
    LocoNet.send( LnPacket );  // Send the received packet from the PC to the LocoNet
    return true;
  }  
  return false;
}

//--------------------------------------------------------------------------------------------
#if defined(XPRESSNET)  
//--------------------------------------------------------------
//Change Power Status
void notifyXNetPower(uint8_t State) {
  #if defined(DEBUG)
  Serial.print("XNetPower: ");
  Serial.println(State, HEX);
  #endif
  power = State;
  LNsendPower();
  Z21sendPower();
}
 
//--------------------------------------------------------------
void notifyXNetgiveLocoInfo(uint8_t UserOps, uint16_t Address) {
  byte Slot = getLNSlot( Address >> 8, Address & 0xFF);
  if (Slot == 0)  //Check if Data in Slot 
    return;
  XpressNet.SetLocoInfo(UserOps, 0x00, SlotData[Slot].DIRF & B11111, SlotData[Slot].SND); //UserOps,Speed,F0,F1
}

/** 
//--------------------------------------------------------------
void notifyXNetgiveLocoFunc(uint8_t UserOps, uint16_t Address) {
  XpressNet.SetFktStatus(UserOps, 0x00, 0x00); //Fkt4, Fkt5
}
**/
 
//--------------------------------------------------------------
void notifyXNetLocoDrive14(uint16_t Address, uint8_t Speed) {
  #if defined(DEBUG)
  Serial.print("XNet A:");
  Serial.print(Address);
  Serial.print(", S14:");
  Serial.println(Speed, BIN);
  #endif
  notifyXNetLocoDrive128(Address, map(Speed, -14, 14, -128, 128));
}
 
//--------------------------------------------------------------
void notifyXNetLocoDrive28(uint16_t Address, uint8_t Speed) {
  #if defined(DEBUG)
  Serial.print("XNet A:");
  Serial.print(Address);
  Serial.print(", S28:");
  Serial.println(Speed, BIN);
  #endif
  notifyXNetLocoDrive128(Address, map(Speed, -28, 28, -128, 128));
}
 
//--------------------------------------------------------------
void notifyXNetLocoDrive128(uint16_t Address, uint8_t Speed) {
  #if defined(DEBUG)
    Serial.print("XNet A:");
    Serial.print(Address);
    Serial.print(", S128:");
    Serial.println(Speed, BIN);
  #endif
  byte Slot = getLNSlot(Address << 8, Address & 0xFF);
  if (Slot == 0)
    return;
  sendLNSPD(Slot, Speed);
  if (bitRead(SlotData[Slot].DIRF, 5) != bitRead(Speed, 7)) { //Change DIR
    bitWrite(SlotData[Slot].DIRF, 5, bitRead(Speed, 7)); 
    sendLNDIRF(Slot);
  }
}
 
//--------------------------------------------------------------
void notifyXNetLocoFunc1(uint16_t Address, uint8_t Func1) {
  //Func1: - F0 F4 F3 F2 F1
  #if defined(DEBUG)
    Serial.print("XNet A:");
    Serial.print(Address);
    Serial.print(", F1:");
    Serial.println(Func1, BIN);
  #endif
  byte Slot = getLNSlot( Address >> 8, Address & 0xFF);
  if (Slot == 0)  //Check if Data in Slot 
    return;
  SlotData[Slot].DIRF = (SlotData[Slot].DIRF & B11111) | Func1;
  sendLNDIRF(Slot);
}
 
//--------------------------------------------------------------
void notifyXNetLocoFunc2(uint16_t Address, uint8_t Func2) {
  //Func2: - F8 F7 F6 F5
  #if defined(DEBUG)
    Serial.print("XNet A:");
    Serial.print(Address);
    Serial.print(", F2:");
    Serial.println(Func2, BIN);
  #endif
  byte Slot = getLNSlot( Address >> 8, Address & 0xFF);
  if (Slot == 0)  //Check if Data in Slot 
    return;
  SlotData[Slot].SND = Func2;
  sendLNSND(Slot); 
}

/**
//--------------------------------------------------------------
void notifyXNetLocoFunc3(uint16_t Address, uint8_t Func3) {
  //Func3: - F12 F11 F10 F9
  #if defined(DEBUG)
    Serial.print("XNet A:");
    Serial.print(Address);
    Serial.print(", F3:");
    Serial.println(Func3, BIN);
  #endif
}
 
//--------------------------------------------------------------
void notifyXNetTrntInfo(uint8_t UserOps, uint8_t Address, uint8_t data) {
  byte inc = 0;  //lower Nibble
  if (data == 0x01)
    inc = 2;  //upper Nibble
  byte pos = 0x00;
  if (dcc.getBasicAccessoryInfo(Address+inc) == true)
    bitWrite(pos, 0, 1);
  if (dcc.getBasicAccessoryInfo(Address+inc+1) == true)
    bitWrite(pos, 1, 1);  
  XpressNet.SetTrntStatus(UserOps, Address, pos);
}

//--------------------------------------------------------------
void notifyXNetTrnt(uint16_t Address, uint8_t data) {
  #if defined(DEBUG)
    Serial.print("XNet TA:");
    Serial.print(Address);
    Serial.print(", P:");
    Serial.println(data, BIN);
  #endif
}
**/ 

#endif
//XpressNet ENDE -----------------------------------

//--------------------------------------------------------------------------------------------
#if defined(DEBUG)
int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#endif

