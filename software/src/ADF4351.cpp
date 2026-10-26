#include <Arduino.h>
#include <EEPROM.h>
#include <CRC.h>

#include "HardwareConfig.h"
#include "UserInterface.h"
#include "Global.h"
#include "rational_approximation.h"

// de SQ3SWF 2019
// modified by wa1hco 2025
//   bit banging SPI 
//   change user interface
//   EEPROM storage of settings

// debug

#define SHL(x,y) ((uint32_t)1<<y)*x

char Msg[120];
sConfig_t Config;

// fpfd = REFin * (1 + dbl) / (R * (1 + divby2)))
// RFout = fpfd * (INT + (FRAC/MOD))
// INT 23 to 65535 for 4/5 prescaler, 75 to 65535 for 8/9 prescalar

// declare user configuration
uint32_t Fout;           // output frequency

// declare board configuration
uint32_t Fref;             // reference clock

// declare derived configuration
uint32_t Fpfd;

// declare the register array
uint32_t reg[6];              // 6 32 bit values

// declare global register fields
// Register 0: VCO to PFD, divide by N, N = INT + FRAC/MOD 
uint16_t INT;                 // 16 bit integer {23, 75} to 65535
uint16_t FRAC;                // 12 bit fraction 0 to (MOD - 1)

// Values for register fields
// Register 1: 
uint8_t  phase_adj;           //  1 bit
uint8_t  prescaler;           //  1 bit 0 4/5 up to 3.6GHz, 1 8/9 up to 
uint16_t phase;               // 12 bits PHASE counter, double buffered, recommended value
uint16_t MOD;                 // 12 bits MOD, double buffered

// Register 2:
uint8_t  noise_mode;          //  2 bits 0 low noise, 3 low spur
uint8_t  muxout;              //  3 bits 0 float, 1 DVdd, 2 DGND, 3 R out, 4 N out, 5 analog LD, 6 digital LD, reserved
uint8_t  ref_doubler;         //  1 bit 
uint8_t  ref_by_2;            //  1 bit RDIV/2
uint16_t r_counter;           // 10 bits R counter, double buffered
uint8_t  dbl_buf;             //  1 bit 0 disabled, 1 enabled
uint8_t  charge_pump_current; //  4 bits 0 .31 mA, 7 2.5 mA, 15 5.0 mA, assuming 5.1K resistor
uint8_t  ldf;                 //  1 bit lock det function 0 FRAC-N, 1 INT-N
uint8_t  ldp;                 //  1 bit lock det precision 0 10 ns, 1 6 ns
uint8_t  pd_polarity;         //  1 bit 0 negative, 1 positive
uint8_t  powerdown;           //  1 bit 0 disabled, 1 enabled
uint8_t  cp_three_state;      //  1 bit 0 disabled, 1 enabled
uint8_t  counter_reset;       //  1 bit 0 disabled, 1 enabled

// Register 3:
uint8_t  band_sel_clk_mode;   //  1 bit 0 low, 1 high
uint8_t  abp;                 //  1 bit antibacklash pulse 0 6 ns (frac N), 1 3 ns (int N)
uint8_t  chg_cancel;          //  1 bit charge pump cancelation 0 disabled, 1 enabled (int N)
uint8_t  cycle_slip_reduce;   //  1 bit cycle slip 0 disabled, 1 enabled
uint8_t  clk_div_mode;        //  2 bits 0 div off, 1 fast lock, 2 resync, 3 reserved
uint16_t clock_divider;       // 12 bits clock divider value

// Register 4:
uint8_t  feedback_sel;        //   1 bit 0 divided, 1 fundamental
uint8_t  rf_div_sel;          //   3 bits 0 /1, 1 /2, 2 /4, 3 /8 4 /16, 5 /32, 6 /64, 7 reserved
uint8_t  band_select_clkdiv;  //   8 bits band select 1 to 255
uint8_t  vco_pwrdown;         //   1 bit power 0 up, 1 down
uint8_t  mute_till_ld;        //   1 bit mute till LD 0 disabled, 1 enabled
uint8_t  aux_out_sel;         //   1 bit 0 divided out, 1 fundamental
uint8_t  aux_out_ena;         //   1 bit 0 disabled, 1 enabled
uint8_t  aux_out_pwr;         //   2 bits 0 -4 dBm, 1 -1 dBm, 2 +2 dBm, 3 +5 dBm
uint8_t  rf_out_ena;          //   1 bit 0 disabled, 1 enabled
uint8_t  rf_out_pwr;          //   2 bits 0 -4 dBm, 1 -1 dBm, 2 +2 dBm, 3 +5 dBm

// Register 5:
uint8_t  ld_pinmode;          //   2 bits, 0 low, 1 digital LD, 2 low, 3 high

// Initialize the register fields in global variables
void InitFields() {
  Fout                 =      50200000; // 50.2 MHz
  Fref                 =      25000000; // 25 MHz
  // Register 0: VCO to PFD, divide by N, N = INT + FRAC/MOD 
  INT                  =      0;   // 16 bit integer  {23, 75} to 65535
  FRAC                 =      0;   // 12 bit fraction 0 to (MOD - 1)

  // Values for register fields
  // Register 1: 
  phase_adj            =      0;  //  1 bit
  prescaler            =      0;  //  1 bit 0 4/5 up to 3.6GHz, 1 8/9 up to 
  phase                =      1;  // 12 bits PHASE counter, double buffered, recommended value
  MOD                  =   4095;  // 12 bits MOD, double buffered

  // Register 2:
  noise_mode           =      0;  //  2 bits 0 low noise, 3 low spur
  muxout               =      3;  //  3 bits 0 float, 1 DVdd, 2 DGND, 3 R out, 4 N out, 5 analog LD, 6 digital LD, reserved
  ref_doubler          =      0;  //  1 bit 
  ref_by_2             =      0;  //  1 bit RDIV/2
  r_counter            =     10;  // 10 bits R counter, double buffered
  dbl_buf              =      0;  //  1 bit 0 disabled, 1 enabled
  charge_pump_current  = 0b0111;  //  4 bits 0 .31 mA, 7 2.5 mA, 15 5.0 mA, assuming 5.1K resistor
  ldf                  =      1;  //  1 bit lock det function 0 FRAC-N, 1 INT-N
  ldp                  =      0;  //  1 bit lock det precision 0 10 ns, 1 6 ns
  pd_polarity          =      1;  //  1 bit 0 negative, 1 positive
  powerdown            =      0;  //  1 bit 0 disabled, 1 enabled
  cp_three_state       =      0;  //  1 bit 0 disabled, 1 enabled
  counter_reset        =      0;  //  1 bit 0 disabled, 1 enabled

  // Register 3:
  band_sel_clk_mode    =      0;  //  1 bit 0 low, 1 high
  abp                  =      0;  //  1 bit antibacklash pulse 0 6 ns (frac N), 1 3 ns (int N)
  chg_cancel           =      0;  //  1 bit charge pump cancelation 0 disabled, 1 enabled (int N)
  cycle_slip_reduce    =      0;  //  1 bit cycle slip 0 disabled, 1 enabled
  clk_div_mode         =      0;  //  2 bits 0 div off, 1 fast lock, 2 resync, 3 reserved
  clock_divider        =    150;  // 12 bits clock divider value

  // Register 4:
  feedback_sel         =      0;  //   1 bit 0 divided, 1 fundamental
  rf_div_sel           =      5;  //   3 bits 0 /1, 1 /2, 2 /4, 3 /8 4 /16, 5 /32, 6 /64, 7 reserved
  band_select_clkdiv   =      4;  //   8 bits band select 1 to 255
  vco_pwrdown          =      0;  //   1 bit power 0 up, 1 down
  mute_till_ld         =      0;  //   1 bit mute till LD 0 disabled, 1 enabled
  aux_out_sel          =      0;  //   1 bit 0 divided out, 1 fundamental
  aux_out_ena          =      1;  //   1 bit 0 disabled, 1 enabled
  aux_out_pwr          =      3;  //   2 bits 0 -4 dBm, 1 -1 dBm, 2 +2 dBm, 3 +5 dBm
  rf_out_ena           =      1;  //   1 bit 0 disabled, 1 enabled
  rf_out_pwr           =      3;  //   2 bits 0 -4 dBm, 1 -1 dBm, 2 +2 dBm, 3 +5 dBm

  // Register 5:
  ld_pinmode           =      1;  //   2 bits, 0 low, 1 digital LD, 2 low, 3 high
}

void calculate_fields() {
  // divide vco range down to include output frequency
  if(Fout >= 2200000000ul) rf_div_sel = 0;
  if(Fout  < 2200000000ul) rf_div_sel = 1;
  if(Fout  < 1100000000ul) rf_div_sel = 2;
  if(Fout  <  550000000ul) rf_div_sel = 3;
  if(Fout  <  275000000ul) rf_div_sel = 4;
  if(Fout  <  137500000ul) rf_div_sel = 5;
  if(Fout  <   68750000ul) rf_div_sel = 6; 
  
  // Fvco 2200 to 4400 MHz 
  uint32_t Fvco = Fout * (1 << rf_div_sel);
  float target;

  // Fpfd common frequency after N and R dividers

  bool IntN = true; // is integer N mode feasible

  // find a greatest common divsor between Fout and Fref, integer N mode
  uint32_t Fpfd = binaryGCD(Fout, Fref);
  uint16_t Ffrac;
  INT = Fout / Fpfd;
  uint16_t R = Fref / Fpfd;

  if (Fpfd > 125000) {
    band_select_clkdiv = Fpfd / 125000;
    if (band_select_clkdiv > 256) {
      Serial.print("calculate_fields: band_select_clkdiv ");
      Serial.print(band_select_clkdiv);
      Serial.println("greater than 255");
    }
  }
  
  if (R > 1023) {
    snprintf(Msg, 80, "calculate_fields: R %d > 1023", R);
    Serial.println(Msg);
    IntN = false;
  }
  else if (INT > 65356) {
    snprintf(Msg, 80, "calculate_fields: INT %d > 65536", INT);
    Serial.println(Msg);
    IntN = false;
  }
  else if (Fpfd > 32000000) {
    snprintf(Msg, 80, "calculate_fields: Fpdf %d > 32 MHz", Fpfd);
    Serial.println(Msg);
    IntN = false;
  } 
  else if (Fpfd < 100000) {
    snprintf(Msg, 80, "calculate_fields: Fpdf %d < 100000", Fpfd);
    Serial.println(Msg);
    IntN = false;
  }
  else {
    IntN = true;
  }

  if (IntN) {
    Serial.println("calculate_fields: Integer N possible");
    // calculate VCO to PFD divide by N
    FRAC = 0;
    ldf = 1; // monitor 40 pfd cycles
    ldp = 1; // 6 ns
    band_sel_clk_mode = 0; // Fpfd <= 125 KHz
    abp = 1; // antibacklash pulse width 3 ns.
  }
  else  { // fractional N required
    Serial.println("calculate_fields: Fractional N required");
    ldf = 0; // monitor 5 pfd cycles
    ldp = 0; // 10 ns

    R = 25; // initial default for fractional N
    Fpfd = Fref / (uint32_t) R;

    band_sel_clk_mode = 1; // Fpfd > 125 KHz, band select clock divider <= 254
    abp = 0; // antibacklash pulse width 6 ns

    INT = Fout / Fpfd;         // floor(Fout/Fpfd)
    Ffrac = (uint16_t) (Fout - (uint32_t) INT * Fpfd); // remainder
    target = (float) Ffrac / (float) Fpfd;
    rational_t result = rational_approximation(target, 4095);
    // scale denominator up as much as possible
    uint16_t scale = (uint16_t) 4095 / result.denominator; // floor, rounded down 
    FRAC = scale * result.numerator;
    MOD  = scale * result.denominator;  
    snprintf(Msg, 80, "ass_reg: num %d, denom %d", result.numerator, result.denominator);
    Serial.println(Msg);
  }

  snprintf(Msg, 80, "calculate_fields(): Fout %lu, Fpfd %lu, INT %d, R %d", Fout, Fpfd, INT, R);
  Serial.print(Msg);
  if(!IntN){
    snprintf(Msg, 80, ", Ffrac %d, target %f, FRAC %d, MOD %d", Ffrac, target, FRAC, MOD);
    Serial.print(Msg);
  }
  Serial.println();  
}

// assemble register fields
void assemble_registers() {
  // 38.375 to 68.75 MHz fed to N counter from 2200 to 4400 MHz VCO
  if(Fout >= 2200000000ul) rf_div_sel = 0;
  if(Fout  < 2200000000ul) rf_div_sel = 1;
  if(Fout  < 1100000000ul) rf_div_sel = 2;
  if(Fout  <  550000000ul) rf_div_sel = 3;
  if(Fout  <  275000000ul) rf_div_sel = 4;
  if(Fout  <  137500000ul) rf_div_sel = 5;
  if(Fout  <   68750000ul) rf_div_sel = 6; 
    
  // Positions for register fields
  // assemble the register fragments into register array
  //Serial.println("prep_reg: ");
  reg[0] = SHL(INT,                15) | // 16 bits /N from VCO to PFD
           SHL(FRAC,                3) | // 12 bits
           0b000;;

  snprintf(Msg, 80, "assemble_registers: INT %d, FRAC %d, reg[0] %lx", INT, FRAC, reg[0]);
  Serial.println(Msg);

  reg[1] = SHL(phase_adj,          28) | //  1 bit 0 off, 1 on
           SHL(prescaler,          27) | //  1 bit 0 4/5, 1 8/9
           SHL(phase,              15) | // 12 bits 
           SHL(MOD,                 3) | // 12 bits
           0b001;
  //Serial.printHexln(reg[1]);
  reg[2] = SHL(noise_mode,         29) | //  2 bits
           SHL(muxout,             26) | //  3 bits
           SHL(ref_doubler,        25) | //  1
           SHL(ref_by_2,           24) | //  1
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
  reg[3] = SHL(band_sel_clk_mode,  23) | //  1
           SHL(abp,                22) | //  1
           SHL(chg_cancel,         21) | //  1
           SHL(cycle_slip_reduce,  18) | //  1
           SHL(clk_div_mode,       15) | //  2 bits
           SHL(clock_divider,       3) | // 12 bits
           0b011;
  //Serial.printHexln(reg[3]);
  reg[4] = SHL(feedback_sel,       23) | //  1
           SHL(rf_div_sel,         20) | //  3 bits
           SHL(band_select_clkdiv, 12) | //  8 bits
           SHL(vco_pwrdown,        11) | //  1
           SHL(mute_till_ld,       10) | //  1
           SHL(aux_out_sel,         9) | //  1
           SHL(aux_out_ena,         8) | //  1
           SHL(aux_out_pwr,         6) | //  2 bits
           SHL(rf_out_ena,          5) | //  1 bit
           SHL(rf_out_pwr,          3) | //  2 bits
           0b100;
  //Serial.printHexln(reg[4]);
  reg[5] = SHL(ld_pinmode,         22) | //  2 bits
           SHL(0b11,               19) | //  2 bits datasheet reserved, but 0b11
           0b101;
  //Serial.printHexln(reg[5]);
}

// fill in global variables from register fields
void disassemble_registers() {
  INT                  = (reg[0] >> 15) & 0xFFFF;
  FRAC                 = (reg[0] >>  3) & 0xFFF;

  phase_adj            = (reg[1] >> 28) & 0x01;
  prescaler            = (reg[1] >> 27) & 0x01;
  phase                = (reg[1] >> 15) & 0xFFF;
  MOD                  = (reg[1] >>  3) * 0xFFF;

  noise_mode           = (reg[2] >> 29) & 0x03;
  muxout               = (reg[2] >> 26) & 0x03;
  ref_doubler          = (reg[2] >> 25) & 0x01;
  ref_by_2             = (reg[2] >> 24) & 0x01;
  r_counter            = (reg[2] >> 14) & 0x3FF;
  dbl_buf              = (reg[2] >> 13) & 0x01;
  charge_pump_current  = (reg[2] >>  9) & 0x0f;
  ldf                  = (reg[2] >>  8) & 0x01;
  ldp                  = (reg[2] >>  7) & 0x01;
  pd_polarity          = (reg[2] >>  6) & 0x01; 
  powerdown            = (reg[2] >>  5) & 0x01;
  cp_three_state       = (reg[2] >>  4) & 0x01;
  counter_reset        = (reg[2] >>  3) & 0x01;
  
  band_sel_clk_mode    = (reg[3] >> 23) & 0x01;
  abp                  = (reg[3] >> 22) & 0x01; 
  chg_cancel           = (reg[3] >> 21) & 0x01; 
  cycle_slip_reduce    = (reg[3] >> 18) & 0x01; 
  clk_div_mode         = (reg[3] >> 15) & 0x03;
  clock_divider        = (reg[3] >>  3) & 0x0FFF;

  feedback_sel         = (reg[4] >> 23) & 0x01;
  rf_div_sel           = (reg[4] >> 20) & 0x07;
  band_select_clkdiv   = (reg[4] >> 12) & 0xFF;
  vco_pwrdown          = (reg[4] >> 11) & 0x01;
  mute_till_ld         = (reg[4] >> 10) & 0x01;
  aux_out_sel          = (reg[4] >>  9) & 0x01;
  aux_out_ena          = (reg[4] >>  8) & 0x01;
  aux_out_pwr          = (reg[4] >>  6) & 0x03;
  rf_out_ena           = (reg[4] >>  5) & 0x01;
  rf_out_pwr           = (reg[4] >>  3) & 0x03;

  ld_pinmode           = (reg[5] >> 22) & 0x03;
}

void bitBangData(byte _send)  // This function transmit the data via bitbanging
{
  for(int i=7; i>=0; i--)  // 8 bits in a byte
  {
    // #define  bitRead(value, bit) (((value) >> (bit)) & 0x01)
    digitalWrite(MOSIPin, bitRead(_send, i));      // Set MOSI
    digitalWrite(J6_3Pin, bitRead(_send, i));      // set test port

    digitalWrite(SCKPin,  HIGH);                   // SCK high
    digitalWrite(J6_2Pin, HIGH);

    digitalWrite(SCKPin,  LOW);                    // SCK low
    digitalWrite(J6_2Pin, LOW);
  } 
  return;
}

void sendRegisterToAdf(uint16_t reg_id) {
  digitalWrite(SSPin, LOW);
  delayMicroseconds(10);

  //bitBangData((uint8_t)(reg[reg_id] >> 24)); Serial.printHex((uint8_t) (reg[reg_id] >> 24)); Serial.print(" ");
  //bitBangData((uint8_t)(reg[reg_id] >> 16)); Serial.printHex((uint8_t) (reg[reg_id] >> 16)); Serial.print(" ");
  //bitBangData((uint8_t)(reg[reg_id] >> 8));  Serial.printHex((uint8_t) (reg[reg_id] >>  8)); Serial.print(" ");
  //bitBangData((uint8_t)(reg[reg_id]     ));  Serial.printHex((uint8_t) (reg[reg_id]      )); Serial.print(" ");
  //Serial.println();
  
  digitalWrite(SSPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(SSPin, LOW);
  delayMicroseconds(2500);
}

void updateAllRegisters() {
  for(int i=5; i>=0; i--) {
    sendRegisterToAdf(i);
  }
  //Serial.println();
  //Serial.print("FREQ: "); Serial.print((uint32_t)Fout);
  //Serial.println();

  //Serial.print("r_counter ");
  //Serial.print(r_counter);
  //Serial.print(" Fpfd ");
  //Serial.println(Fpfd);
}

void setup() {
  InitPins();    // initialize hardware pins.
  InitFields();  // initialize variables for register fields.

  Serial.begin(57600);
  
  delay(7000);

  //test_rational_approx();

  // read saved registers from eeprom
  // check checksum
  // if good, disassemble register fields into global variables
  // if bad, initialize global variables from program defaults, save to eeprom
  // display global variable and register status

  calculate_fields();
  assemble_registers();
  updateAllRegisters();
}

char key, cwmode=0;

void loop() {
//  UserConfig();

  while (Serial.available() == 0) {}
  key = Serial.read();
  Serial.print("key ");
  Serial.println(key);

  if(key =='x') Fout -= 10;
  if(key =='c') Fout += 10;
  if(key =='s') Fout -= 100;
  if(key =='d') Fout += 100;
  if(key =='w') Fout -= 1000;
  if(key =='e') Fout += 1000;
  if(key =='2') Fout -= 10000; // 10K
  if(key =='3') Fout += 10000;
  if(key =='1') Fout -= 100000; // 100K
  if(key =='4') Fout += 100000;
  if(key =='5') Fout -= 10000000; // 10M
  if(key =='6') Fout += 10000000;
  if(key =='/') rf_out_ena = 1 - rf_out_ena; // invert

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
    Serial.print("powerdown ");
    Serial.println(powerdown);
    assemble_registers();  // prepare all, but only use [2]
    Serial.print("reg[2] ");
    Serial.println(reg[2], HEX);
    sendRegisterToAdf(2); // prints bits and hex
  } 
  
  calculate_fields();
  assemble_registers();
  updateAllRegisters();
  PrintConfig();    

  // LED debug
  digitalWrite(LEDD7Pin, digitalRead(LDPin));
  digitalWrite(LEDD8Pin, digitalRead(MUXPin));
  
}
