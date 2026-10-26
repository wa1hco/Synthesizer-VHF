

#ifndef UserInterface_h
#define UserInterface_h

#include "Config.h"
#include "Global.h"

// public functions
void UserConfig();
uint32_t binaryGCD(uint32_t u, uint32_t v);
uint32_t FreqCmd();
void     PrintConfig(); // pretty print config on serial port

#endif