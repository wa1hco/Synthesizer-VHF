// SPDX-License-Identifier: GPL-3.0-or-later
// ADF4351 frequency calculation and R0–R5 assembly.
//
// Board reference is a 25 MHz TCXO (ADF4351_REF_HZ). Output frequency follows
// datasheet equation (3), which closes the loop at the VCO (fundamental
// feedback, Register 4 DB23 = 1):
//
//   RFOUT = [INT + FRAC/MOD] × fPFD / RF_divider
//   fPFD  = REFIN × (1 + D) / (R × (1 + T))          equation (4)
//
// The VCO itself is equation (1): 2.2 GHz to 4.4 GHz before RF_divider.

#ifndef ADF4351_CALC_H
#define ADF4351_CALC_H

#include <stdint.h>

// 25 MHz TCXO on REFIN. The only reference frequency the firmware uses.
static const uint32_t ADF4351_REF_HZ = 25000000UL;

// Every field that is packed into R0–R5. calculate() rewrites the
// frequency-dependent members and leaves the output-power, muxout, and
// power-down members alone.
struct Adf4351Fields {
  uint16_t INT;
  uint16_t FRAC;
  uint8_t phase_adj;
  uint8_t prescaler;
  uint16_t phase;
  uint16_t MOD;
  uint8_t noise_mode;
  uint8_t muxout;
  uint8_t ref_doubler;
  uint8_t ref_by_2;
  uint16_t r_counter;
  uint8_t dbl_buf;
  uint8_t charge_pump_current;
  uint8_t ldf;
  uint8_t ldp;
  uint8_t pd_polarity;
  uint8_t powerdown;
  uint8_t cp_three_state;
  uint8_t counter_reset;
  uint8_t band_sel_clk_mode;
  uint8_t abp;
  uint8_t chg_cancel;
  uint8_t cycle_slip_reduce;
  uint8_t clk_div_mode;
  uint16_t clock_divider;
  uint8_t feedback_sel;
  uint8_t rf_div_sel;
  uint8_t band_select_clkdiv;
  uint8_t vco_pwrdown;
  uint8_t mute_till_ld;
  uint8_t aux_out_sel;
  uint8_t aux_out_ena;
  uint8_t aux_out_pwr;
  uint8_t rf_out_ena;
  uint8_t rf_out_pwr;
  uint8_t ld_pinmode;
};

// Choose INT/FRAC/MOD/R/prescaler/RF divider/band-select/lock-detect fields
// so the assembled registers produce fout_hz. Returns false when fout_hz is
// outside the VCO-plus-divider range (34.375 MHz to 4.4 GHz).
bool adf4351_calculate(uint64_t fout_hz, uint32_t fref_hz, Adf4351Fields *fields);

// Pack fields into the six 32-bit words. reg[n] is Register n (control bits n).
void adf4351_assemble(const Adf4351Fields *fields, uint32_t reg[6]);

// Inverse of assemble. Masks follow the Register 0–5 maps (Fig. 24–29).
void adf4351_disassemble(const uint32_t reg[6], Adf4351Fields *fields);

#endif
