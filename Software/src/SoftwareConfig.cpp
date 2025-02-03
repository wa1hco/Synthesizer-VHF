
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
  Config.frequency         = 222 - 28; // MHz
  Config.feedback_sel; 
  Config.noise_mode;
  Config.MOD;
  Config.muxout;
  Config.r_counter;
  Config.rdif2;
  Config.ref_clk;
  Config.ref_doubler;
  Config.rf_div_sel;
  Config.reg                = reg;
  Config.CRC16              = CalcCRC(Config);

  PrintConfig(Config);
  return Config;
}

// pretty print the memory configuration on serial port
void PrintConfig(sConfig_t Config) {
  Serial.println("Tiny Sequencer, (c) wa1hco, V0.5, Creative Commons\n");
  for(int ii = 0; ii < 4; ii++) {
    char Polarity_str[15] = "              ";
    if (Config.Step[ii].RxPolarity == OPEN) {
      strcpy(Polarity_str, "OPEN on RX");
    } else {
      strcpy(Polarity_str, "CLOSED on RX");
    }
    snprintf(Msg, 80, "Step %d, Contacts %s, TX delay %d, RX delay %d", ii, Polarity_str, Config.Step[ii].Tx_msec, Config.Step[ii].Rx_msec);
    Serial.println(Msg);
  }

  Serial.print("RTS   ");
  if (Config.RTSEnable == true){
    Serial.print("Enabled, ");
  } else {
    Serial.print("Disabled, ");
  }
  Serial.println();

  Serial.print("CTS   ");
  if (Config.CTSEnable == true){
    Serial.print("Enabled");
  } else {
    Serial.print("Disabled");
  }
  Serial.println();

  Serial.print("Tx Timer ");
  uint16_t Timeout = Config.Timeout;
  if (Timeout == 0) {
    Serial.println("Disabled");
  } else {
    Serial.print(Timeout);
    Serial.println(" sec");
  }

  // DEBUG
  Serial.print("CRC ");
  Serial.print(Config.CRC16, HEX);
  Serial.println();

} // PrintConfig()
