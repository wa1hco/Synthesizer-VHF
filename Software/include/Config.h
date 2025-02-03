#ifndef Config_h
#define Config_h

#include <Arduino.h>

// Configuration structure used for program and EEPROM
struct sConfig_t {
  // overall 
  uint32_t     frequency;
  uint32_t     ref_clk;
  uint32_t     reg[6];

  // register fields
  // reg 1
  uint16_t     MOD;

  // reg 2
  uint8_t      muxout;
  bool         ref_doubler;
  bool         rdif2;
  uint16_t     r_counter;

  // reg 3
  uint8_t      noise_mode;

  // reg 4
  uint8_t      feedback_sel;
  uint8_t      rf_div_sel;

  uint16_t     CRC16;       // check for valid configuration table
}; 
#endif