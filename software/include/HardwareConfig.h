#ifndef HardwareConfig_h
#define HardwareConfig_h

#include <Arduino.h> // get the pin names
#include "Config.h"

//  ATTinyX16, QFN-20, Hardware Pin Definitions, lower case means secondary pin location
//----------|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
// Chip Pin |1  |2  |3  |4  |5  |6  |7  |8  |9  |10 |11 |12 |13 |14 |15 |16 |17 |18 |19 |20 |
// Port #   |15 |16 |   |   |0  |1  |2  |3  |4  |5  |6  |7  |6  |0  |10 |11 |12 |13 |17 |14 |
// Port     |PA2|PA3|GND|VCC|PA4|PA5|PA6|PA7|PB5|PB4|PB3|PB2|PB1|PB0|PC0|PC1|PC2|PC3|PA0|PA1|
//----------|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
// Power    |   |   |GND|VCC|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
// TCA0     |   |   |   |   |WO4|WO5|   |   |wo2|wo1|wo0|WO2|WO1|WO0|   |   |   |wo3|WO3|   |
// UPDI     |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |PDI|   |
// ADC0     |A2 |A3 |A2 |   |A4 |A5 |A6 |A7 |A8 |A9 |   |   |A10|A11|   |A17|A14|A15|A0 |A1 |
// ADC1     |   |   |   |   |A0 |A1 |A2 |A3 |   |   |   |   |   |   |A6 |A7 |A8 |A9 |   |   |
// TWI0     |scl|   |   |   |   |   |   |   |   |   |   |   |SDA|SCL|   |   |   |   |   |sda|
// USART0   |rxd|xck|   |   |   |   |   |   |   |   |RXD|TXD|XCK|DIR|   |   |   |   |   |txd|
// CLOCK    |   |EXT|   |   |   |   |   |   |   |   |OSC|OSC|   |   |   |   |   |   |   |   |
//----------|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|

// Sequencer board use of pins
//----------|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
// Pins     |1  |2  |3  |4  |5  |6  |7  |8  |9  |10 |11 |12 |13 |14 |15 |16 |17 |18 |19 |20 |  
// Port     |PA2|PA3|GND|VCC|PA4|PA5|PA6|PA7|PB5|PB4|PB3|PB2|PB1|PB0|PC0|PC1|PC2|PC3|PA0|PA1|
//----------|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|  
// ADF4351  |   |   |   |   |   |   |   |   |   |   |   |   |CE |LE |DAT|CLK|MUX|EN |   |LD |
// Serial   |   |   |   |   |   |   |   |   |   |   |RXD|TXD|   |   |   |   |   |   |   |   |
// LED      |LED|LED|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
// UPDI     |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |PDI|   |
// TstPts   |   |   |   |   |   |   |   |   |J32|J33|   |   |   |   |   |   |   |   |   |   |
//--------- |---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
// Unused   |   |   |   |   |5  |6  |7  |8  |   |   |   |   |13 |   |   |   |   |   |   |   |
//--------- |---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|

// Hardware Connections for ATTinyX16
// Pin definitions 
//      Pin Name  Port      Name           
#define SCKPin    PIN_PC1   // CLK            
#define MOSIPin   PIN_PC0   // DATA           
#define SSPin     PIN_PB0   // LE            
#define RFENPin  PIN_PC3   // RF Enable   
#define MUXPin    PIN_PC2   // MUX input   
#define LDPin     PIN_PA1   // Lock Detect 
#define CEPin     PIN_PB1   // Chip Enable 
#define LEDD7Pin  PIN_PA2   // LED D7      
#define LEDD8Pin  PIN_PA3   // LED D8      
#define J6_3Pin   PIN_PB4   // J3_3        
#define J6_2Pin   PIN_PB5   // J3_2    

// Hardware design 
// defines how the optoisolators connect to MCU, 
// Steps are numbered from 0 to 3

// Public function
void InitPins();

#endif