#ifndef GLOBAL_H
#define GLOBAL_H

#include <Arduino.h>
#include "Config.h"


// declare useful macros
#define SHL(x,y) ((uint32_t)1<<y)*x
extern uint32_t ref_cll;

// declare the global variables
extern sConfig_t Config;
extern char      Msg[];

extern uint32_t frequency;            // setting
extern uint32_t ref_clk;              // configuration

extern uint32_t pfd_freq;             // derived

extern uint32_t reg[6];               // 6 32 bit values

// Register fields
// Register 0: VCO to PFD, divide by N, N = INT + FRAC/MOD 
extern uint16_t INT;                  // 16 bit integer  {23, 75} to 65535
extern uint16_t FRAC;                 // 12 bit fraction 0 to (MOD - 1)

// Values for register fields
// Register 1: 
extern uint8_t  phase_adj;            //  1 bit
extern uint8_t  prescaler;            //  1 bit 0 4/5 up to 3.6GHz, 1 8/9 up to 
extern uint16_t phase;                // 12 bits PHASE counter, double buffered, recommended value
extern uint16_t MOD;                  // 12 bits MOD, double buffered

// Register 2:
extern uint8_t  noise_mode;           //  2 bits 0 low noise, 3 low spur
extern uint8_t  muxout;               //  3 bits 0 float, 1 DVdd, 2 DGND, 3 R out, 4 N out, 5 analog LD, 6 digital LD, reserved
extern uint8_t  ref_doubler;          //  1 bit 
extern uint8_t  ref_by_2;             //  1 bit RDIV/2
extern uint16_t r_counter;            // 10 bits R counter, double buffered
extern uint8_t  dbl_buf;              //  1 bit 0 disabled, 1 enabled
extern uint8_t  charge_pump_current;  //  4 bits 0 .31 mA, 7 2.5 mA, 15 5.0 mA, assuming 5.1K resistor
extern uint8_t  ldf;                  //  1 bit lock det function 0 FRAC-N, 1 INT-N
extern uint8_t  ldp;                  //  1 bit lock det precision 0 10 ns, 1 6 ns
extern uint8_t  pd_polarity;          //  1 bit 0 negative, 1 positive
extern uint8_t  powerdown;            //  1 bit 0 disabled, 1 enabled
extern uint8_t  cp_three_state;       //  1 bit 0 disabled, 1 enabled
extern uint8_t  counter_reset;        //  1 bit 0 disabled, 1 enabled

// Register 3:
extern uint8_t  band_sel_clk_mode;    //  1 bit 0 low, 1 high
extern uint8_t  anti_backlash_pw;     //  1 bit antibacklash pulse 0 6 ns (frac N), 1 3 ns (int N)
extern uint8_t  chg_cancel;           //  1 bit charge pump cancelation 0 disabled, 1 enabled (int N)
extern uint8_t  cycle_slip_reduce;    //  1 bit cycle slip 0 disabled, 1 enabled
extern uint8_t  clk_div_mode;         //  2 bits 0 div off, 1 fast lock, 2 resync, 3 reserved
extern uint16_t clock_divider;        // 12 bits clock divider value

// Register 4:
extern uint8_t  feedback_sel;         //   1 bit 0 divided, 1 fundamental
extern uint8_t  rf_div_sel;           //   3 bits 0 /1, 1 /2, 2 /4, 3 /8 4 /16, 5 /32, 6 /64, 7 reserved
extern uint8_t  band_select_clkdiv;   //   8 bits band select 1 to 255
extern uint8_t  vco_pwrdown;          //   1 bit power 0 up, 1 down
extern uint8_t  mute_till_ld;         //   1 bit mute till LD 0 disabled, 1 enabled
extern uint8_t  aux_out_sel;          //   1 bit 0 divided out, 1 fundamental
extern uint8_t  aux_out_ena;          //   1 bit 0 disabled, 1 enabled
extern uint8_t  aux_out_pwr;          //   2 bits 0 -4 dBm, 1 -1 dBm, 2 +2 dBm, 3 +5 dBm
extern uint8_t  rf_out_ena;           //   1 bit 0 disabled, 1 enabled
extern uint8_t  rf_out_pwr;           //   2 bits 0 -4 dBm, 1 -1 dBm, 2 +2 dBm, 3 +5 dBm

// Register 5:
extern uint8_t  ld_pinmode;           //   2 bits, 0 low, 1 digital LD, 2 low, 3 high
#endif