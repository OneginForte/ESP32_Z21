//--------------------------------------------------------------
/*

  XpressNetMaster Interface for Arduino
  
Funktionsumfang:  
- Fahren per LokMaus2 und MultiMaus
- Schalten von DCC Weichen mit der MultiMaus (not tested 15.04.15)

  Copyright (c) by Philipp Gahtow, year 2021
*/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

//**************************************************************
//byte XNetUserOps = 0x00;
//byte XNetReturnLoco = 0x00;

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

//--------------------------------------------------------------------------------------------
// calculate the parity bit in the call byte for this guy
uint8_t callByteParity(uint8_t me)
{
  uint8_t parity = (1 == 0);
  uint8_t vv;
  me &= 0x7f;
  vv = me;

  while (vv)
  {
    parity = !parity;
    vv &= (vv - 1);
  }
  if (parity)
    me |= 0x80;
  return me;
}

//--------------------------------------------------------------
//Change Power Status
void notifyXNetPower(uint8_t State) {
  if (Railpower != State) {
    globalPower(State);
  }
}

//--------------------------------------------------------------
void notifyXNetgiveLocoInfo(uint8_t UserOps, uint16_t Address) {
  //XNetReturnLoco |= 0x01;
  //XNetUserOps = UserOps;
  
  //dcc.getLocoStateFull(Address, false); //request for XpressNet only!
  uint8_t ldata[6];
  getLocoData(Address, ldata);  //uint8_t Steps[0], uint8_t Speed[1], uint8_t F0[2], uint8_t F1[3], uint8_t F2[4], uint8_t F3[5]
  if (ldata[0] == 0x03)  //128 Steps?
      ldata[0]++;  //set Steps to 0x04
  SetLocoInfo(UserOps, ldata[0], ldata[1], ldata[2], ldata[3]); //UserOps,Steps,Speed,F0,F1
  
}

//--------------------------------------------------------------
void notifyXNetgiveLocoFunc(uint8_t UserOps, uint16_t Address) {
  //XNetReturnLoco |= 0x02;
  //XNetUserOps = UserOps;
  
  //dcc.getLocoStateFull(Address, false); //request for XpressNet only!
  SetFktStatus(UserOps, getFunktion13to20(Address), getFunktion21to28(Address)); //Fkt4, Fkt5
  
}

//--------------------------------------------------------------
void notifyXNetgiveLocoMM(uint8_t UserOps, uint16_t Address) {
  //XNetReturnLoco |= 0x04;
  //XNetUserOps = UserOps;
  
  //dcc.getLocoStateFull(Address, false); //request for XpressNet only!
  //uint8_t ldata[6];
  //dcc.getLocoData(Address, ldata);  //uint8_t Steps[0], uint8_t Speed[1], uint8_t F0[2], uint8_t F1[3], uint8_t F2[4], uint8_t F3[5]
  //if (ldata[0] == 0x03)  //128 Steps?
  //    ldata[0]++;  //set Steps to 0x04
  //SetLocoInfoMM(UserOps, ldata[0], ldata[1], ldata[2], ldata[3], ldata[4], ldata[5]); //Steps,Speed,F0,F1,F2,F3
  
}

//--------------------------------------------------------------
void notifyXNetLocoDrive14(uint16_t Address, uint8_t Speed) {

  if (Speed == 0) 
    setSpeed14(Address, (getLocoDir(Address) << 7) | (Speed & 0b01111111));
  else setSpeed14(Address, Speed);
  //dcc.getLocoStateFull(Address);      //request for other devices

  
  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoDrive28(uint16_t Address, uint8_t Speed) {
 
  if (Speed == 0)
  setSpeed28(Address, (getLocoDir(Address) << 7) | (Speed & 0b01111111));
  else setSpeed28(Address, Speed);
  //dcc.getLocoStateFull(Address);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoDrive128(uint16_t Address, uint8_t Speed) {


  //if ((Speed & 0x7F) == 0) 
//    dcc.setSpeed128(Address, (dcc.getLocoDir(Address) << 7) | (Speed & B01111111));
  //else 
  setSpeed128(Address, Speed);
  //dcc.getLocoStateFull(Address);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc1(uint16_t Address, uint8_t Func1) {


  setFunctions0to4(Address, Func1);	//- F0 F4 F3 F2 F1
  //dcc.getLocoStateFull(Address);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc2(uint16_t Address, uint8_t Func2) {

  setFunctions5to8(Address, Func2);	//- F8 F7 F6 F5
  //dcc.getLocoStateFull(Address);      //request for other devices
  
  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc3(uint16_t Address, uint8_t Func3) {

  setFunctions9to12(Address, Func3);	//- F12 F11 F10 F9
  //dcc.getLocoStateFull(Address);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc4(uint16_t Address, uint8_t Func4) {
  

  setFunctions13to20(Address, Func4);	//F20 F19 F18 F17 F16 F15 F14 F13
  //dcc.getLocoStateFull(Address);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc5(uint16_t Address, uint8_t Func5) {

  setFunctions21to28(Address, Func5);	//F28 F27 F26 F25 F24 F23 F22 F21
  //dcc.getLocoStateFull(Address);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetTrntInfo(uint8_t UserOps, uint8_t Address, uint8_t data) {
  int adr = ((Address * 4) + ((data & 0x01) * 2));
  uint8_t pos = data << 4;
  bitWrite(pos, 7, 1);  //command completed!
  if (getBasicAccessoryInfo(adr) == false)
    bitWrite(pos, 0, 1);
  else bitWrite(pos, 1, 1);  
  if (getBasicAccessoryInfo(adr+1) == false)
    bitWrite(pos, 2, 1);  
  else bitWrite(pos, 3, 1);    
  SetTrntStatus(UserOps, Address, pos);

}

//--------------------------------------------------------------
void notifyXNetTrnt(uint16_t Address, uint8_t data) {

  setBasicAccessoryPos(Address,data & 0x01, bitRead(data,3));    //Adr, left/right, activ

}

//--------------------------------------------------------------
//DCC return no ACK:
void notifyCVNack()
{

  z21setCVNack(); //send back to device and stop programming!

  XNetsetCVNack();


}

//--------------------------------------------------------------
void notifyXNetDirectCV(uint16_t CV, uint8_t data) {

  opsProgDirectCV(CV,data);  //return value from DCC via 'notifyCVVerify'

}

bool opsReadDirectCV(uint16_t CV, uint8_t bitToRead, bool bitSet)
{
  //for CV#1 is the Adress 0
  //long-preamble   0  011110AA  0  AAAAAAAA  0  111KDBBB  0  EEEEEEEE  1
  //Bit Manipulation
  //BBB represents the bit position
  //D contains the value of the bit to be verified or written
  //K=1 signifies a "Write Bit" operation and K=0 signifies a "Bit Verify"

  //check if CV# is between 0 - 1023
  if (CV > 1023)
  {
    if (notifyCVNack)
      notifyCVNack();
    return false;
  }

  if (current_cv_bit > 7 || bitToRead == 0)
  {
    current_cv_bit = 0;
    bitToRead = 0;
  }

  if (railpower != SERVICE)      //time to wait for the relais!
    opsDecoderReset(RSTsRepeat); //send first a Reset Packet
  setpower(SERVICE, true);
  current_ack_read = false;
  ack_monitor_time = micros();
  current_cv = CV;

  DCCPacket p(((CV >> 8) & B11) | B01111000);
  uint8_t data[] = {0x00, 0x00};
  data[0] = CV & 0xFF;
  data[1] = B11100000 | (bitToRead & 0x07) | (bitSet << 3); //verify Bit is "bitSet"? ("1" or "0")
  p.addData(data, 2);
  p.setKind(ops_mode_programming_kind); //always use short Adress Mode!
  p.setRepeat(ProgRepeat);

  if (bitToRead == 0)
  {
    opsDecoderReset(RSTsRepeat); //send first a Reset Packet
  }

  return 1;//ops_programmming_queue.insertPacket(&p);
  //--------opsDecoderReset(RSTcRepeat);	//send a Reset while waiting to finish
  //return opsDecoderReset(RSTcRepeat);	//send a Reset while waiting to finish
}

//--------------------------------------------------------------
void notifyXNetDirectReadCV(uint16_t cvAdr) {

  opsReadDirectCV(cvAdr);  //read cv

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
  uint8_t data[] = {0x00, 0x00, 0x00};

  // split the CV address up among data uint8_ts 0 and 1
  data[0] = ((CV >> 8) & 0x11) | 0x11101100;
  data[1] = CV & 0xFF;
  data[2] = CV_data;

  //p.addData(data, 3);
  //p.setKind(pom_mode_programming_kind);
  //.setRepeat(ProgRepeat);

  //return low_priority_queue.insertPacket(&p);	//Standard

  //setpower(SERVICE, true);

  //opsDecoderReset();	//send first a Reset Packet
  return 1;// ops_programmming_queue.insertPacket(&p);
  //return e_stop_queue.insertPacket(&p);
}
//--------------------------------------------------------------
void notifyXNetPOMwriteByte (uint16_t Adr, uint16_t CV, uint8_t data) {
  
  opsProgramCV(Adr, CV, data);  //set decoder byte

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
  uint8_t data[] = {0x00, 0x00, 0x00};

  // split the CV address up among data uint8_ts 0 and 1
  data[0] = ((CV >> 8) & 0b11) | 0b11101000;
  data[1] = CV & 0xFF;
  data[2] = Bit_data & 0x0F;

  //p.addData(data, 3);
  //p.setKind(pom_mode_programming_kind);
  //p.setRepeat(ProgRepeat);

  //return low_priority_queue.insertPacket(&p);	//Standard

  //setpower(SERVICE, true);

  //opsDecoderReset();	//send first a Reset Packet
  return 1;// ops_programmming_queue.insertPacket(&p);
  //return e_stop_queue.insertPacket(&p);
}

//--------------------------------------------------------------
void notifyXNetPOMwriteBit (uint16_t Adr, uint16_t CV, uint8_t data) {

  opsPOMwriteBit(Adr, CV, data);  //set decoder bit

}


