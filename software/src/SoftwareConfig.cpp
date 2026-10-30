// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <EEPROM.h>
#include <CRC16.h>
#include <CRC.h>

#include "SoftwareConfig.h"
#include "HardwareConfig.h"
#include "Global.h"
#include "adf4351_calc.h"


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
  Config.Fout           = 144170000;  // Hz, 144.170 MHz
  Config.Fref           = ADF4351_REF_HZ;  // 25 MHz TCXO
  for (int ii = 0; ii < 6; ii++) {
    Config.reg[ii] = reg[ii];
  }
  Config.CRC16             = CalcCRC(Config);

  return Config;
}
