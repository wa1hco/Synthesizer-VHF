#ifndef Config_h
#define Config_h

#include <Arduino.h>

// Configuration structure used for program and EEPROM
struct sConfig_t {
  // Settings
  uint32_t     Fout;  
  uint32_t     Fref;
  uint32_t     reg[6];  // ADF registers
  uint16_t     CRC16;
 }; 

#endif