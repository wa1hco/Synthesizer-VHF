#include <Arduino.h>
#include <EEPROM.h>
#include <CRC.h>

#include "HardwareConfig.h"
#include "UserInterface.h"
#include "Global.h"
#include "adf4351_calc.h"

// de SQ3SWF 2019
// modified by wa1hco 2025
//   bit banging SPI 
//   change user interface
//   EEPROM storage of settings

// debug

char Msg[120];
sConfig_t Config;

// fPFD = REFIN * (1 + D) / (R * (1 + T))
// RFOUT = [INT + FRAC/MOD] * fPFD / RF_divider     (fundamental feedback)
// INT 23 to 65535 for the 4/5 prescaler, 75 to 65535 for the 8/9 prescaler

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
  Fout                 =     144170000; // 144.170 MHz
  Fref                 =      ADF4351_REF_HZ; // 25 MHz TCXO
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

static void fields_from_globals(Adf4351Fields *f) {
  f->INT = INT;
  f->FRAC = FRAC;
  f->phase_adj = phase_adj;
  f->prescaler = prescaler;
  f->phase = phase;
  f->MOD = MOD;
  f->noise_mode = noise_mode;
  f->muxout = muxout;
  f->ref_doubler = ref_doubler;
  f->ref_by_2 = ref_by_2;
  f->r_counter = r_counter;
  f->dbl_buf = dbl_buf;
  f->charge_pump_current = charge_pump_current;
  f->ldf = ldf;
  f->ldp = ldp;
  f->pd_polarity = pd_polarity;
  f->powerdown = powerdown;
  f->cp_three_state = cp_three_state;
  f->counter_reset = counter_reset;
  f->band_sel_clk_mode = band_sel_clk_mode;
  f->abp = abp;
  f->chg_cancel = chg_cancel;
  f->cycle_slip_reduce = cycle_slip_reduce;
  f->clk_div_mode = clk_div_mode;
  f->clock_divider = clock_divider;
  f->feedback_sel = feedback_sel;
  f->rf_div_sel = rf_div_sel;
  f->band_select_clkdiv = band_select_clkdiv;
  f->vco_pwrdown = vco_pwrdown;
  f->mute_till_ld = mute_till_ld;
  f->aux_out_sel = aux_out_sel;
  f->aux_out_ena = aux_out_ena;
  f->aux_out_pwr = aux_out_pwr;
  f->rf_out_ena = rf_out_ena;
  f->rf_out_pwr = rf_out_pwr;
  f->ld_pinmode = ld_pinmode;
}

static void fields_to_globals(const Adf4351Fields *f) {
  INT = f->INT;
  FRAC = f->FRAC;
  phase_adj = f->phase_adj;
  prescaler = f->prescaler;
  phase = f->phase;
  MOD = f->MOD;
  noise_mode = f->noise_mode;
  muxout = f->muxout;
  ref_doubler = f->ref_doubler;
  ref_by_2 = f->ref_by_2;
  r_counter = f->r_counter;
  dbl_buf = f->dbl_buf;
  charge_pump_current = f->charge_pump_current;
  ldf = f->ldf;
  ldp = f->ldp;
  pd_polarity = f->pd_polarity;
  powerdown = f->powerdown;
  cp_three_state = f->cp_three_state;
  counter_reset = f->counter_reset;
  band_sel_clk_mode = f->band_sel_clk_mode;
  abp = f->abp;
  chg_cancel = f->chg_cancel;
  cycle_slip_reduce = f->cycle_slip_reduce;
  clk_div_mode = f->clk_div_mode;
  clock_divider = f->clock_divider;
  feedback_sel = f->feedback_sel;
  rf_div_sel = f->rf_div_sel;
  band_select_clkdiv = f->band_select_clkdiv;
  vco_pwrdown = f->vco_pwrdown;
  mute_till_ld = f->mute_till_ld;
  aux_out_sel = f->aux_out_sel;
  aux_out_ena = f->aux_out_ena;
  aux_out_pwr = f->aux_out_pwr;
  rf_out_ena = f->rf_out_ena;
  rf_out_pwr = f->rf_out_pwr;
  ld_pinmode = f->ld_pinmode;
}

bool calculate_fields() {
  Adf4351Fields f;
  fields_from_globals(&f);
  Fref = ADF4351_REF_HZ;
  if (!adf4351_calculate(Fout, Fref, &f)) {
    Serial.println("calculate_fields: frequency out of range");
    return false;
  }
  fields_to_globals(&f);
  Fpfd = (r_counter == 0) ? 0 : (Fref / r_counter);

  snprintf(Msg, 120,
           "calculate_fields: %s Fout %lu Fpfd %lu INT %u R %u FRAC %u MOD %u div %u",
           (FRAC == 0) ? "integer-N" : "fractional-N",
           (unsigned long)Fout, (unsigned long)Fpfd,
           INT, r_counter, FRAC, MOD, (unsigned)(1u << rf_div_sel));
  Serial.println(Msg);
  return true;
}

// assemble register fields
void assemble_registers() {
  Adf4351Fields f;
  fields_from_globals(&f);
  adf4351_assemble(&f, reg);
  snprintf(Msg, 80, "assemble_registers: INT %u, FRAC %u, R %u, reg[0] %lx",
           INT, FRAC, r_counter, (unsigned long)reg[0]);
  Serial.println(Msg);
}

// fill in global variables from register fields
void disassemble_registers() {
  Adf4351Fields f;
  adf4351_disassemble(reg, &f);
  fields_to_globals(&f);
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

  // 32 bits, MSB first, latched on the rising edge of LE (datasheet p.12).
  bitBangData((uint8_t)(reg[reg_id] >> 24));
  bitBangData((uint8_t)(reg[reg_id] >> 16));
  bitBangData((uint8_t)(reg[reg_id] >> 8));
  bitBangData((uint8_t)(reg[reg_id]));
  
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

  if (calculate_fields()) {
    assemble_registers();
    updateAllRegisters();
  }
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
  
  if (calculate_fields()) {
    assemble_registers();
    updateAllRegisters();
  }
  PrintConfig();    

  // LED debug
  digitalWrite(LEDD7Pin, digitalRead(LDPin));
  digitalWrite(LEDD8Pin, digitalRead(MUXPin));
  
}
