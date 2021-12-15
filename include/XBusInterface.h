//--------------------------------------------------------------
/*

  XpressNetMaster Interface for Arduino
  
Funktionsumfang:  
- Fahren per LokMaus2 und MultiMaus
- Schalten von DCC Weichen mit der MultiMaus (not tested 15.04.15)

  Copyright (c) by Philipp Gahtow, year 2021
*/
#ifndef XBusinterface_h
#define XBusinterface_h

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "z21.h"
#include "XpressNet.h"


//Trnt message paket format (inc)
#define ROCO 0
#define IB 4
uint8_t TrntFormat; // The Addressing of BasicAccessory Messages

//Fahrstufen:


#define lowByte(w)                     ((w) & 0xFF)
#define highByte(w)                    (((w) >> 8) & 0xFF)
#define bitRead(value, bit)            (((value) >> (bit)) & 0x01)
#define bitSet(value, bit)             ((value) |= (1 << (bit)))// x | (1<<n)
#define bitClear(value, bit)           ((value) &= ~(1 << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit):bitClear(value, bit))
#define bit(b)                         (1UL << (b))





//--------------------------------------------------------------
void notifyXNetLocoDrive14(uint16_t Address, uint8_t Speed) {

  if (Speed == 0) 
    setSpeed14(Address, (getLocoDir(Address) << 7) | (Speed & 0b01111111));
  else setSpeed14(Address, Speed);
  getLocoStateFull(Address, 1);      //request for other devices

  
  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoDrive28(uint16_t Address, uint8_t Speed) {
 
  if (Speed == 0)
  setSpeed28(Address, (getLocoDir(Address) << 7) | (Speed & 0b01111111));
  else setSpeed28(Address, Speed);
  getLocoStateFull(Address, 1);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoDrive128(uint16_t Address, uint8_t Speed) {


  if ((Speed & 0x7F) == 0) 
    setSpeed128(Address, (getLocoDir(Address) << 7) | (Speed & 0b01111111));
  else 
  setSpeed128(Address, Speed);
  getLocoStateFull(Address, 1);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc1(uint16_t Address, uint8_t Func1) {


  setFunctions0to4(Address, Func1);	//- F0 F4 F3 F2 F1
  getLocoStateFull(Address, 1);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc2(uint16_t Address, uint8_t Func2) {

  setFunctions5to8(Address, Func2);	//- F8 F7 F6 F5
  getLocoStateFull(Address,1);      //request for other devices
  
  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc3(uint16_t Address, uint8_t Func3) {

  setFunctions9to12(Address, Func3);	//- F12 F11 F10 F9
  getLocoStateFull(Address,1);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc4(uint16_t Address, uint8_t Func4) {
  

  setFunctions13to20(Address, Func4);	//F20 F19 F18 F17 F16 F15 F14 F13
  getLocoStateFull(Address,1);      //request for other devices

  setLocoStateExt (Address);
  
}

//--------------------------------------------------------------
void notifyXNetLocoFunc5(uint16_t Address, uint8_t Func5) {

  setFunctions21to28(Address, Func5);	//F28 F27 F26 F25 F24 F23 F22 F21
  getLocoStateFull(Address,1);      //request for other devices

  setLocoStateExt (Address);
}



#endif