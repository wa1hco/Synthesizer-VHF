// Host-side check of the ADF4351 tuner.
//
// Decodes R0–R5 from the packed words (datasheet Fig. 24–29) and checks
//   RFOUT = [INT + FRAC/MOD] × fPFD / RF_divider
// against the request, plus the register constraints those figures spell out.
// It does not trust the field struct the calculator just filled in.

#include "adf4351_calc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int g_failed = 0;
static int g_checked = 0;

static void fail(uint64_t fout, const char *what) {
  fprintf(stderr, "FAIL fout %llu: %s\n", (unsigned long long)fout, what);
  g_failed++;
}

struct Decoded {
  uint16_t INT, FRAC, MOD, phase, r_counter, clock_divider;
  uint32_t rf_div;
  uint8_t rf_div_sel;
  uint8_t prescaler, phase_adj, feedback;
  uint8_t ldf, ldp, abp, chg_cancel, csr;
  uint8_t ref_doubler, ref_div2;
  uint8_t bs_mode, bs_div;
  uint8_t clk_div_mode;
  uint8_t noise_mode, muxout, pd_polarity;
  uint8_t charge_pump, dbl_buf, powerdown, cp_three_state, counter_reset;
  uint8_t vco_pd, mtld, aux_sel, aux_ena, aux_pwr, rf_ena, rf_pwr, ld_pin;
};

static Decoded decode(const uint32_t reg[6]) {
  Decoded d;
  memset(&d, 0, sizeof(d));
  d.INT = (uint16_t)((reg[0] >> 15) & 0xFFFF);
  d.FRAC = (uint16_t)((reg[0] >> 3) & 0xFFF);
  d.phase_adj = (uint8_t)((reg[1] >> 28) & 1);
  d.prescaler = (uint8_t)((reg[1] >> 27) & 1);
  d.phase = (uint16_t)((reg[1] >> 15) & 0xFFF);
  d.MOD = (uint16_t)((reg[1] >> 3) & 0xFFF);
  d.noise_mode = (uint8_t)((reg[2] >> 29) & 3);
  d.muxout = (uint8_t)((reg[2] >> 26) & 7);
  d.ref_doubler = (uint8_t)((reg[2] >> 25) & 1);
  d.ref_div2 = (uint8_t)((reg[2] >> 24) & 1);
  d.r_counter = (uint16_t)((reg[2] >> 14) & 0x3FF);
  d.dbl_buf = (uint8_t)((reg[2] >> 13) & 1);
  d.charge_pump = (uint8_t)((reg[2] >> 9) & 0xF);
  d.ldf = (uint8_t)((reg[2] >> 8) & 1);
  d.ldp = (uint8_t)((reg[2] >> 7) & 1);
  d.pd_polarity = (uint8_t)((reg[2] >> 6) & 1);
  d.powerdown = (uint8_t)((reg[2] >> 5) & 1);
  d.cp_three_state = (uint8_t)((reg[2] >> 4) & 1);
  d.counter_reset = (uint8_t)((reg[2] >> 3) & 1);
  d.bs_mode = (uint8_t)((reg[3] >> 23) & 1);
  d.abp = (uint8_t)((reg[3] >> 22) & 1);
  d.chg_cancel = (uint8_t)((reg[3] >> 21) & 1);
  d.csr = (uint8_t)((reg[3] >> 18) & 1);
  d.clk_div_mode = (uint8_t)((reg[3] >> 15) & 3);
  d.clock_divider = (uint16_t)((reg[3] >> 3) & 0xFFF);
  d.feedback = (uint8_t)((reg[4] >> 23) & 1);
  d.rf_div_sel = (uint8_t)((reg[4] >> 20) & 7);
  d.bs_div = (uint8_t)((reg[4] >> 12) & 0xFF);
  d.vco_pd = (uint8_t)((reg[4] >> 11) & 1);
  d.mtld = (uint8_t)((reg[4] >> 10) & 1);
  d.aux_sel = (uint8_t)((reg[4] >> 9) & 1);
  d.aux_ena = (uint8_t)((reg[4] >> 8) & 1);
  d.aux_pwr = (uint8_t)((reg[4] >> 6) & 3);
  d.rf_ena = (uint8_t)((reg[4] >> 5) & 1);
  d.rf_pwr = (uint8_t)((reg[4] >> 3) & 3);
  d.ld_pin = (uint8_t)((reg[5] >> 22) & 3);
  d.rf_div = (d.rf_div_sel <= 6) ? (1u << d.rf_div_sel) : 0;
  return d;
}

static Adf4351Fields board_defaults() {
  Adf4351Fields f;
  memset(&f, 0, sizeof(f));
  f.noise_mode = 0;
  f.muxout = 3;
  f.charge_pump_current = 7;
  f.pd_polarity = 1;
  f.aux_out_ena = 1;
  f.aux_out_pwr = 3;
  f.rf_out_ena = 1;
  f.rf_out_pwr = 3;
  f.ld_pinmode = 1;
  f.phase = 1;
  f.MOD = 4095;
  f.r_counter = 10; // the stale InitFields value; calculate must overwrite it
  return f;
}

static void check_words(uint64_t fout, const uint32_t reg[6], const uint32_t want[6]) {
  for (int i = 0; i < 6; i++) {
    if (reg[i] != want[i]) {
      char buf[80];
      snprintf(buf, sizeof(buf), "R%d got 0x%08X want 0x%08X", i, reg[i], want[i]);
      fail(fout, buf);
    }
  }
}

static void check(uint64_t fout, bool exact, const uint32_t *golden) {
  g_checked++;
  Adf4351Fields fields = board_defaults();
  if (!adf4351_calculate(fout, ADF4351_REF_HZ, &fields)) {
    fail(fout, "calculate returned false");
    return;
  }
  uint32_t reg[6];
  adf4351_assemble(&fields, reg);

  for (int i = 0; i < 6; i++) {
    if ((reg[i] & 7u) != (uint32_t)i) {
      fail(fout, "control bits");
      return;
    }
    if (reg[i] & 0x80000000u) {
      fail(fout, "DB31 reserved set");
      return;
    }
  }
  // Fig. 29 reserved pattern with the lock-detect bits removed.
  if ((reg[5] & ~0x00C00000u) != 0x00180005u) {
    fail(fout, "R5 reserved bits");
  }
  // R3 reserved: DB31:24, DB20, DB19, DB17.
  if (reg[3] & 0xFF1A0000u) {
    fail(fout, "R3 reserved bits");
  }
  if (reg[1] & 0xE0000000u) {
    fail(fout, "R1 reserved bits");
  }
  if (reg[4] & 0xFF000000u) {
    fail(fout, "R4 reserved bits");
  }

  Decoded d = decode(reg);
  Adf4351Fields back;
  memset(&back, 0, sizeof(back));
  adf4351_disassemble(reg, &back);
  if (back.INT != d.INT || back.FRAC != d.FRAC || back.MOD != d.MOD ||
      back.r_counter != d.r_counter || back.prescaler != d.prescaler ||
      back.rf_div_sel != d.rf_div_sel || back.feedback_sel != d.feedback ||
      back.ldf != d.ldf || back.ldp != d.ldp || back.abp != d.abp ||
      back.chg_cancel != d.chg_cancel || back.band_sel_clk_mode != d.bs_mode ||
      back.band_select_clkdiv != d.bs_div || back.muxout != d.muxout ||
      back.phase != d.phase || back.ref_doubler != d.ref_doubler ||
      back.ref_by_2 != d.ref_div2) {
    fail(fout, "disassemble does not match independent decode");
  }

  const uint32_t D = d.ref_doubler;
  const uint32_t T = d.ref_div2;
  const uint32_t R = d.r_counter;
  const uint32_t MOD = d.MOD;
  const uint64_t rf = d.rf_div;

  if (R < 1 || R > 1023) {
    fail(fout, "R counter out of 1..1023");
  }
  if (MOD < 2 || MOD > 4095 || d.FRAC >= MOD) {
    fail(fout, "FRAC/MOD out of range");
  }
  if (d.rf_div_sel > 6 || rf == 0) {
    fail(fout, "RF divider code");
  }
  if (d.feedback != 1) {
    fail(fout, "feedback is not fundamental (equation 3)");
  }
  if (d.phase_adj != 0) {
    fail(fout, "phase adjust disables band selection");
  }
  if (d.phase == 0 || d.phase >= MOD) {
    fail(fout, "phase word must be 1..MOD-1");
  }
  if (d.clk_div_mode == 3) {
    fail(fout, "clock divider mode reserved");
  }
  if (d.clk_div_mode != 0 || d.clock_divider != 0 || d.csr != 0) {
    fail(fout, "phase resync / fast lock / CSR left enabled");
  }
  if (d.noise_mode == 1 || d.noise_mode == 2) {
    fail(fout, "noise mode reserved");
  }
  if (D != 0 || T != 0) {
    fail(fout, "reference doubler/div2 unexpected for 25 MHz");
  }
  // User fields must survive the tune.
  if (d.muxout != 3 || d.charge_pump != 7 || d.pd_polarity != 1 ||
      d.aux_ena != 1 || d.aux_pwr != 3 || d.rf_ena != 1 || d.rf_pwr != 3 ||
      d.ld_pin != 1) {
    fail(fout, "user field clobbered");
  }

  const bool integer_n = (d.FRAC == 0);
  if (integer_n) {
    if (!(d.ldf == 1 && d.ldp == 1 && d.abp == 1 && d.chg_cancel == 1)) {
      fail(fout, "integer-N LDF/LDP/ABP/charge-cancel");
    }
  } else if (!(d.ldf == 0 && d.ldp == 0 && d.abp == 0 && d.chg_cancel == 0)) {
    fail(fout, "fractional-N LDF/LDP/ABP/charge-cancel");
  }

  const uint32_t pfd_max = integer_n ? 45000000u : 32000000u;
  if ((uint64_t)ADF4351_REF_HZ * (1u + D) > (uint64_t)pfd_max * R * (1u + T)) {
    fail(fout, "PFD above datasheet maximum");
  }

  if (d.bs_div < 1 || (d.bs_mode && d.bs_div == 255)) {
    fail(fout, "band-select divider");
  }
  const uint32_t bsc_max = d.bs_mode ? 500000u : 125000u;
  // R-counter output / divider ≤ limit. Output = REFIN×(1+D)/R.
  if ((uint64_t)ADF4351_REF_HZ * (1u + D) > (uint64_t)bsc_max * R * d.bs_div) {
    fail(fout, "band-select clock too fast");
  }

  // Programmed VCO and the prescaler that is legal for it.
  const uint64_t vco_num = ((uint64_t)d.INT * MOD + d.FRAC) * ADF4351_REF_HZ * (1u + D);
  const uint64_t vco_den = (uint64_t)MOD * R * (1u + T);
  if (vco_den == 0 || vco_num < 2200000000ULL * vco_den || vco_num > 4400000000ULL * vco_den) {
    fail(fout, "programmed VCO outside 2.2–4.4 GHz");
  }
  const bool above_36 = vco_num > 3600000000ULL * vco_den;
  if (above_36) {
    if (d.prescaler != 1 || d.INT < 75) {
      fail(fout, "8/9 prescaler required above 3.6 GHz");
    }
  } else if (d.prescaler == 0) {
    if (d.INT < 23) {
      fail(fout, "INT below 23 for 4/5 prescaler");
    }
  } else if (d.INT < 75) {
    fail(fout, "INT below 75 for 8/9 prescaler");
  }
  if (d.INT < 23) {
    fail(fout, "INT out of range");
  }

  // RFOUT = [INT + FRAC/MOD] × fPFD / RF_divider
  // |num/den − fout| ≤ fPFD / (2 × 4095 × RF_divider), i.e. half a finest channel.
  const uint64_t num = vco_num;
  const uint64_t den = vco_den * rf;
  const uint64_t target = fout * den;
  const uint64_t diff = (num > target) ? (num - target) : (target - num);
  if (exact && diff != 0) {
    fail(fout, "expected an exact frequency");
  }
  if (diff > UINT64_MAX / 8190ull || diff * 8190ull > (uint64_t)ADF4351_REF_HZ * (1u + D) * MOD) {
    char buf[96];
    snprintf(buf, sizeof(buf), "frequency error too large (diff %llu)", (unsigned long long)diff);
    fail(fout, buf);
  }

  // Divider actually used must put the *requested* carrier in the VCO window.
  const uint64_t req_vco = fout * rf;
  if (req_vco < 2200000000ULL || req_vco > 4400000000ULL) {
    fail(fout, "RF divider does not cover the request");
  }

  if (golden != 0) {
    check_words(fout, reg, golden);
  }
}

static void check_reject(uint64_t fout) {
  g_checked++;
  Adf4351Fields fields = board_defaults();
  if (adf4351_calculate(fout, ADF4351_REF_HZ, &fields)) {
    fail(fout, "out-of-range frequency was accepted");
  }
  if (fields.r_counter != 10) {
    fail(fout, "failed calculate modified R");
  }
}

int main() {
  // Console capture, 50.200000 MHz, integer-N. R must be 125, not the
  // InitFields leftover of 10. Words assume the board's default user fields.
  const uint32_t golden_50200000[6] = {
      0x1F600000u, // INT 16064, FRAC 0
      0x00008011u, // prescaler 4/5, phase 1, MOD 2
      0x0C1F4FC2u, // R 125, LDF 1, LDP 1, CP 7, muxout 3
      0x00600003u, // ABP 3 ns, charge cancel, band-select low
      0x00E021FCu, // fundamental, ÷64, band-select div 2
      0x00580005u,
  };
  check(50200000ull, true, golden_50200000);

  // Console capture, 50.200010 MHz. The old uint16 remainder (3402) targeted
  // the wrong fraction; the exact tune is INT 3212, FRAC 2502/3125, R 25.
  const uint32_t golden_50200010[6] = {
      0x06464E30u,
      0x0000E1A9u, // MOD 3125, phase 1
      0x0C064E42u, // R 25, fractional lock detect
      0x00000003u,
      0x00E081FCu, // band-select div 8 → 1 MHz/8 = 125 kHz
      0x00580005u,
  };
  check(50200010ull, true, golden_50200010);

  check(35000000ull, true, 0);
  check(144200000ull, true, 0);
  check(432100000ull, true, 0);
  check(1296100000ull, true, 0);
  check(34375000ull, true, 0);    // 2200 MHz / 64
  check(68750000ull, true, 0);    // 2200 MHz / 32
  check(137500000ull, true, 0);
  check(275000000ull, true, 0);
  check(550000000ull, true, 0);
  check(1100000000ull, true, 0);
  check(2200000000ull, true, 0);
  check(3600000000ull, true, 0);  // 4/5 prescaler includes 3.6 GHz
  check(3600000001ull, false, 0); // 1 Hz over 3.6 GHz, 8/9 if the tune stays above
  check(4000000000ull, true, 0);
  check(4400000000ull, true, 0);
  check(2200000001ull, false, 0);
  check(4399999999ull, false, 0);

  for (uint64_t f = 35000000ull; f <= 4400000000ull; f += 1000000ull) {
    check(f, false, 0);
  }

  check_reject(0);
  check_reject(1000000ull);
  check_reject(34374999ull);
  check_reject(4400000001ull);

  if (ADF4351_REF_HZ != 25000000ul) {
    fprintf(stderr, "FAIL reference constant is not 25 MHz\n");
    g_failed++;
  }

  if (g_failed != 0) {
    fprintf(stderr, "%d failure(s) in %d checks\n", g_failed, g_checked);
    return 1;
  }
  printf("ok %d frequencies\n", g_checked);
  return 0;
}
