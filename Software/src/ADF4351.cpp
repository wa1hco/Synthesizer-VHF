#include <Arduino.h>
#include <EEPROM.h>
#include <CRC.h>

#include "HardwareConfig.h"
#include "UserInterface.h"
#include "Global.h"

// de SQ3SWF 2019
// modified by wa1hco 2025
//   bit banging SPI 
//   change user interface
//   EEPROM storage of settings

// debug

#define SHL(x,y) ((uint32_t)1<<y)*x

char Msg[120];

// fpfd = REFin * (1 + dbl) / (R * (1 + divby2)))
// RFout = fpfd * (INT + (FRAC/MOD))
// INT 23 to 65535 for 4/5 prescaler, 75 to 65535 for 8/9 prescalar

uint32_t frequency            = 50300000; 
uint32_t ref_clk              = 25000000;

// create the register array
uint32_t reg[6]               = {0,0,0,0,0,0}; // 6 32 bit values

// Initialize the register fields in global variables
// Register 0: VCO to PFD, divide by N, N = INT + FRAC/MOD 
uint16_t INT                  =      0;   // 16 bit integer  {23, 75} to 65535
uint16_t FRAC                 =      0;   // 12 bit fraction 0 to (MOD - 1)

// Values for register fields
// Register 1: 
uint8_t  phase_adj            =      0;  //  1 bit
uint8_t  prescaler            =      0;  //  1 bit 0 4/5 up to 3.6GHz, 1 8/9 up to 
uint16_t phase                =      1;  // 12 bits PHASE counter, double buffered, recommended value
uint16_t MOD                  =   4095;  // 12 bits MOD, double buffered

// Register 2:
uint8_t  noise_mode           =      0;  //  2 bits 0 low noise, 3 low spur
uint8_t  muxout               =      3;  //  3 bits 0 float, 1 DVdd, 2 DGND, 3 R out, 4 N out, 5 analog LD, 6 digital LD, reserved
uint8_t  ref_doubler          =      0;  //  1 bit 
uint8_t  ref_by_2             =      1;  //  1 bit RDIV/2
uint16_t r_counter            =     10;  // 10 bits R counter, double buffered
uint8_t  dbl_buf              =      0;  //  1 bit 0 disabled, 1 enabled
uint8_t  charge_pump_current  = 0b0111;  //  4 bits 0 .31 mA, 7 2.5 mA, 15 5.0 mA, assuming 5.1K resistor
uint8_t  ldf                  =      1;  //  1 bit lock det function 0 FRAC-N, 1 INT-N
uint8_t  ldp                  =      0;  //  1 bit lock det precision 0 10 ns, 1 6 ns
uint8_t  pd_polarity          =      1;  //  1 bit 0 negative, 1 positive
uint8_t  powerdown            =      0;  //  1 bit 0 disabled, 1 enabled
uint8_t  cp_three_state       =      0;  //  1 bit 0 disabled, 1 enabled
uint8_t  counter_reset        =      0;  //  1 bit 0 disabled, 1 enabled

// Register 3:
uint8_t  band_sel_clk_mode    =      0;  //  1 bit 0 low, 1 high
uint8_t  anti_backlash_pw     =      0;  //  1 bit antibacklash pulse 0 6 ns (frac N), 1 3 ns (int N)
uint8_t  chg_cancel           =      0;  //  1 bit charge pump cancelation 0 disabled, 1 enabled (int N)
uint8_t  cycle_slip_reduce    =      0;  //  1 bit cycle slip 0 disabled, 1 enabled
uint8_t  clk_div_mode         =      0;  //  2 bits 0 div off, 1 fast lock, 2 resync, 3 reserved
uint16_t clock_divider        =    150;  // 12 bits clock divider value

// Register 4:
uint8_t  feedback_sel         =      1;  //   1 bit 0 divided, 1 fundamental
uint8_t  rf_div_sel           =      5;  //   3 bits 0 /1, 1 /2, 2 /4, 3 /8 4 /16, 5 /32, 6 /64, 7 reserved
uint8_t  band_select_clkdiv   =      4;  //   8 bits band select 1 to 255
uint8_t  vco_pwrdown          =      0;  //   1 bit power 0 up, 1 down
uint8_t  mute_till_ld         =      1;  //   1 bit mute till LD 0 disabled, 1 enabled
uint8_t  aux_out_sel          =      0;  //   1 bit 0 divided out, 1 fundamental
uint8_t  aux_out_ena          =      1;  //   1 bit 0 disabled, 1 enabled
uint8_t  aux_out_pwr          =      3;  //   2 bits 0 -4 dBm, 1 -1 dBm, 2 +2 dBm, 3 +5 dBm
uint8_t  rf_out_ena           =      1;  //   1 bit 0 disabled, 1 enabled
uint8_t  rf_out_pwr           =      3;  //   2 bits 0 -4 dBm, 1 -1 dBm, 2 +2 dBm, 3 +5 dBm

// Register 5:
uint8_t  ld_pinmode           =      1;  //   2 bits, 0 low, 1 digital LD, 2 low, 3 high

uint32_t pfd_freq             = (ref_clk * (1 + ref_doubler )) / (r_counter * ((1 + ref_by_2)));


void prepare_registers() {
  if(frequency >= 2200000000) rf_div_sel = 0;
  if(frequency  < 2200000000) rf_div_sel = 1;
  if(frequency  < 1100000000) rf_div_sel = 2;
  if(frequency  <  550000000) rf_div_sel = 3;
  if(frequency  <  275000000) rf_div_sel = 4;
  if(frequency  <  137500000) rf_div_sel = 5;
  if(frequency  <   68750000) rf_div_sel = 6;
  
  // calculate VCO to PFD divide by N
  INT  =   (frequency * (1 << rf_div_sel)) / pfd_freq;
  FRAC = (((frequency * (1 << rf_div_sel)) % pfd_freq) * 4095) / pfd_freq;
  
  // Positions for register fields
  // assemble the register fragments into register array
  //Serial.println("prep_reg: ");
  reg[0] = SHL(INT,                15) | // 16 bits /N from VCO to PFD
           SHL(FRAC,                3) | // 12 bits
           0b000;;
  //Serial.printHexln(reg[0]);
  reg[1] = SHL(phase_adj,          28) | //  1 bit 0 off, 1 on
           SHL(prescaler,          27) | //  1 bit 0 4/5, 1 8/9
           SHL(phase,              15) | // 12 bits 
           SHL(MOD,                 3) | // 12 bits
           0b001;
  //Serial.printHexln(reg[1]);
  reg[2] = SHL(noise_mode,     29) | //  2 bits
           SHL(muxout,             26) | //  3 bits
           SHL(ref_doubler,        25) | //  1
           SHL(ref_by_2,              24) | //  1
           SHL(r_counter,          14) | // 10 bits
           SHL(dbl_buf,            13) | //  1
           SHL(charge_pump_current, 9) | //  4 bits
           SHL(ldf,                 8) | //  1
           SHL(ldp,                 7) | //  1
           SHL(pd_polarity,         6) | //  1
           SHL(powerdown,           5) | //  1
           SHL(cp_three_state,      4) | //  1
           SHL(counter_reset,       3) | //  1
           0b010;
  //Serial.printHexln(reg[2]);
  reg[3] = SHL(band_sel_clk_mode,   23) | //  1
           SHL(anti_backlash_pw,                22) | //  1
           SHL(chg_cancel,         21) | //  1
           SHL(cycle_slip_reduce,                18) | //  1
           SHL(clk_div_mode,        15) | //  2 bits
           SHL(clock_divider,       3) | // 12 bits
           0b011;
  //Serial.printHexln(reg[3]);
  reg[4] = SHL(feedback_sel,       23) | //  1
           SHL(rf_div_sel,         20) | //  3 bits
           SHL(band_select_clkdiv, 12) | //  8 bits
           SHL(vco_pwrdown,        11) | //  1
           SHL(mute_till_ld,               10) | //  1
           SHL(aux_out_sel,          9) | //  1
           SHL(aux_out_ena,          8) | //  1
           SHL(aux_out_pwr,             6) | //  2 bits
           SHL(rf_out_ena,              5) | //  1 bit
           SHL(rf_out_pwr,             3) | //  2 bits
           0b100;
  //Serial.printHexln(reg[4]);
  reg[5] = SHL(ld_pinmode,         22) | //  2 bits
           SHL(0b11,               19) | //  2 bits datasheet reserved, but 0b11
           0b101;
  //Serial.printHexln(reg[5]);
}

void bitBangData(byte _send)  // This function transmit the data via bitbanging
{
  for(int i=7; i>=0; i--)  // 8 bits in a byte
  {
    // #define  bitRead(value, bit) (((value) >> (bit)) & 0x01)
    digitalWrite(MOSIPin, bitRead(_send, i)); Serial.print(bitRead(_send, i));   // Set MOSI
    digitalWrite(J6_3Pin, bitRead(_send, i)); // set test port

    digitalWrite(SCKPin,  HIGH);                  // SCK high
    digitalWrite(J6_2Pin, HIGH);

    //bitWrite(_receive, i, digitalRead(MISOPin)); // Capture MISO
    digitalWrite(SCKPin,  LOW);                   // SCK low
    digitalWrite(J6_2Pin, LOW);
  } 
  Serial.print(" ");
  return;        // Return the received data
}

void sendRegisterToAdf(uint16_t reg_id) {
  digitalWrite(SSPin, LOW);
  delayMicroseconds(10);

  bitBangData((uint8_t)(reg[reg_id] >> 24)); Serial.printHex((uint8_t) (reg[reg_id] >> 24)); Serial.print(" ");
  bitBangData((uint8_t)(reg[reg_id] >> 16)); Serial.printHex((uint8_t) (reg[reg_id] >> 16)); Serial.print(" ");
  bitBangData((uint8_t)(reg[reg_id] >> 8));  Serial.printHex((uint8_t) (reg[reg_id] >>  8)); Serial.print(" ");
  bitBangData((uint8_t)(reg[reg_id]     ));  Serial.printHex((uint8_t) (reg[reg_id]      )); Serial.print(" ");
  
  digitalWrite(SSPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(SSPin, LOW);
  delayMicroseconds(2500);
  Serial.println();
}

void updateAllRegisters() {
  for(int i=5; i>=0; i--) {
    sendRegisterToAdf(i);
  }
  Serial.println();
  Serial.print("FREQ: "); Serial.print((uint32_t)frequency);
  Serial.println();

  uint32_t pfd_freq = (ref_clk * (ref_doubler + 1)) / (r_counter * (ref_by_2 + 1));
  Serial.print("r_counter ");
  Serial.print(r_counter);
  Serial.print(" pfd_freq ");
  Serial.println(pfd_freq);
}

void setup() {

  InitPins();

  Serial.begin(57600);
  
  delay(500);

  prepare_registers();
  updateAllRegisters();
}

char key, cwmode=0;

void loop() {

  UserConfig();

  while (Serial.available() == 0) {}
  key = Serial.read();
  Serial.print("key ");
  Serial.println(key);

  if(key =='x') frequency -= 10;
  if(key =='c') frequency += 10;
  if(key =='s') frequency -= 100;
  if(key =='d') frequency += 100;
  if(key =='w') frequency -= 1000;
  if(key =='e') frequency += 1000;
  if(key =='2') frequency -= 10000; // 10K
  if(key =='3') frequency += 10000;
  if(key =='1') frequency -= 100000; // 100K
  if(key =='4') frequency += 100000;
  if(key =='5') frequency -= 10000000; // 10M
  if(key =='6') frequency += 10000000;
  if(key =='/') rf_out_ena     = 1 - rf_out_ena; // invert

  if(key == 'm') {
    muxout = (muxout + 1) % 8;
    Serial.print("muxout ");
    Serial.print(muxout);
    //0 float, 1 DVdd, 2 DGND, 3 R out, 4 N out, 5 analod LD, 6 digital LD, reserved
    if(muxout == 0)  Serial.println(" float");
    if(muxout == 1)  Serial.println(" DVdd");
    if(muxout == 2)  Serial.println(" DGND");
    if(muxout == 3)  Serial.println(" R out");
    if(muxout == 4)  Serial.println(" N out");
    if(muxout == 5)  Serial.println(" analog LD");
    if(muxout == 6)  Serial.println(" digital LD");
    if(muxout == 7)  Serial.println(" reserved");
  }

  if(key == 'l') {
    ld_pinmode = (ld_pinmode + 1) % 4;
    Serial.print("ld_pinmode ");
    Serial.print(ld_pinmode);
    if (ld_pinmode == 0) Serial.println(" low");
    if (ld_pinmode == 1) Serial.println(" Dig LD");
    if (ld_pinmode == 2) Serial.println(" low");
    if (ld_pinmode == 3) Serial.println(" high");
  }
  // Toggle the power down bit

  if(key == '+') { 
    rf_out_pwr = (rf_out_pwr + 1) % 4;
    Serial.println(rf_out_pwr);
  }

  if(key == 'p') {
    Serial.println();
    powerdown = (uint8_t) !((bool) powerdown); // invert power down bit
    Serial.print("!powerdown ");
    Serial.println(powerdown);
    prepare_registers();  // prepare all, but only use [2]
    Serial.print("reg[2] ");
    Serial.println(reg[2]);
    sendRegisterToAdf(2); // prints bits and hex
  } else {
    prepare_registers();
    updateAllRegisters();
  }

  // LED debug
  digitalWrite(LEDD7Pin, digitalRead(LDPin));
  digitalWrite(LEDD8Pin, digitalRead(MUXPin));
  
}
