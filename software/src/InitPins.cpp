#include "HardwareConfig.h"
#include "Config.h"

void InitPins() {
    pinMode(SSPin,    OUTPUT); // CE slave select pin
    pinMode(SCKPin,   OUTPUT); // CLK
    pinMode(MOSIPin,  OUTPUT); // Data
    pinMode(SSPin,    OUTPUT); // LE
    pinMode(RFENPin, OUTPUT); // RF enable
    pinMode(CEPin,    OUTPUT); // Chip Enable

    pinMode(LEDD7Pin, OUTPUT); // Test indicator
    pinMode(LEDD8Pin, OUTPUT); // Test indicator
    pinMode(J6_3Pin,  OUTPUT); // Test point
    pinMode(J6_2Pin,  OUTPUT); // Test point

    pinMode(LDPin,    INPUT);
    pinMode(MUXPin,   INPUT);

    digitalWrite(SSPin,    LOW); 
    digitalWrite(CEPin,    HIGH);
    digitalWrite(RFENPin, HIGH);
}
  