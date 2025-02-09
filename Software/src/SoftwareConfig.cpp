
#include <Arduino.h>
#include <EEPROM.h>
#include <CRC16.h>
#include <CRC.h>

#include "SoftwareConfig.h"
#include "HardwareConfig.h"
#include "Global.h"


// Config structure functions
// read, write, put, verify, print

// Read Config from EEPROM
sConfig_t GetConfig(uint8_t address) {
  sConfig_t EEConf;
  EEPROM.get(address, EEConf);
  return EEConf;
}

// Update Config in EEPROM
// compare new config with eeprom and update bytes as necessary
void PutConfig(uint8_t address, sConfig_t EEConf) {
  // update the CRC
  EEConf.CRC16 = CalcCRC(EEConf);
  EEPROM.put(address, EEConf);
  return;
}

bool isConfigValid(sConfig_t Config) {
  uint16_t CRCTest = CalcCRC(Config);
  return (CRCTest == Config.CRC16);
}

uint16_t CalcCRC(sConfig_t Config) {
  return calcCRC16((uint8_t*) &Config, sizeof(Config) - sizeof(Config.CRC16));
}

// Default configuration, executed from Setup(), if necessary
// User defines contact closure state when in Rx mode not keyed
// User defines the step timing per the data sheet for the relay
// delays specifiec in msec after key asserted
sConfig_t InitDefaultConfig() {
  sConfig_t Config;
  Config.frequency         = 50200000;  // Hz, 50.2 MHz
  Config.ref_clk           = 25000000;  // Hz, 25 MHz
  for (int ii = 0; ii < 6; ii++) {
    Config.reg[ii] = reg[ii];
  }
  Config.CRC16             = CalcCRC(Config);

  PrintConfig(Config);
  return Config;
}

// pretty print synthesizer configuration on serial port
void PrintConfig(sConfig_t Config) {
  Serial.println("ADF4351 Synthesizer, (c) wa1hco, V0.1, Creative Commons\n");

  for(int ii = 0; ii < 6; ii++) {  // for each register
    byte b;
    snprintf(Msg, 80, "Reg %d ", ii);
    Serial.print(Msg);
    b = reg[ii] >> 24; snprintf(Msg, 80, "%b %x ", b, b); Serial.print(Msg);    
    b = reg[ii] >> 16; snprintf(Msg, 80, "%b %x ", b, b); Serial.print(Msg);    
    b = reg[ii] >>  6; snprintf(Msg, 80, "%b %x ", b, b); Serial.print(Msg);    
    b = reg[ii]      ; snprintf(Msg, 80, "%b %x ", b, b); Serial.print(Msg);    
    Serial.println();

    snprintf(Msg, 80, "frequency %ul, ref_clk %ul", frequency, ref_clk);  Serial.println();
  }

  Serial.println();


  // DEBUG
  Serial.print("CRC ");
  Serial.print(Config.CRC16, HEX);
  Serial.println();

} // PrintConfig()
