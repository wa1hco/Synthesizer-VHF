// SPDX-License-Identifier: GPL-3.0-or-later
#include "adf4351_calc.h"

// ADF4351 Rev. A. PFD_FLOOR (125 kHz) is the operating floor this loop uses;
// the register limits are 32 MHz fractional-N and 45 MHz integer-N.
static const uint64_t VCO_MIN = 2200000000ULL;
static const uint64_t VCO_MAX = 4400000000ULL;
static const uint64_t PRE_MAX = 3600000000ULL;
static const uint32_t PFD_FRAC = 32000000UL;
static const uint32_t PFD_INT = 45000000UL;
static const uint32_t PFD_FLOOR = 125000UL;

#define NOINLINE __attribute__((noinline))

static NOINLINE uint32_t gcd32(uint32_t a, uint32_t b) {
  while (b) {
    uint32_t t = a % b;
    a = b;
    b = t;
  }
  return a;
}

static NOINLINE uint64_t gcd64(uint64_t a, uint64_t b) {
  while (b) {
    uint64_t t = a % b;
    a = b;
    b = t;
  }
  return a;
}

// fvco*R = ip*fref + rem, with ip in 0..65535 and rem < fref.
static NOINLINE uint8_t quot_rem(uint64_t fvco, uint32_t R, uint32_t fref,
                                uint16_t *ip, uint32_t *rem) {
  uint64_t n = fvco * R;
  uint64_t q = n / fref;
  if (q > 65535ull) {
    return 0;
  }
  *ip = (uint16_t)q;
  *rem = (uint32_t)(n - q * fref);
  return 1;
}

// Closest p/q to rem/den, 1 ≤ q ≤ maxq. Farey neighbors bracket the target.
static NOINLINE void best_ratio(uint32_t rem, uint32_t den, uint16_t maxq,
                               uint16_t *op, uint16_t *oq) {
  uint16_t a = 0, b = 1, c = 1, d = 1;
  if (!den || !rem || rem >= den || maxq < 1) {
    *op = (rem && den) ? 1 : 0;
    *oq = 1;
    return;
  }
  while ((uint32_t)b + d <= maxq) {
    uint16_t p = (uint16_t)(a + c);
    uint16_t q = (uint16_t)(b + d);
    uint64_t left = (uint64_t)rem * q;
    uint64_t right = (uint64_t)p * den;
    if (left == right) {
      *op = p;
      *oq = q;
      return;
    }
    if (left < right) {
      c = p;
      d = q;
    } else {
      a = p;
      b = q;
    }
  }
  uint64_t ea = (uint64_t)rem * b;                 // a == 0 side may be the best
  uint64_t eb = (uint64_t)a * den;
  uint64_t e_ab = ea > eb ? ea - eb : eb - ea;
  uint64_t ec = (uint64_t)rem * d;
  uint64_t ed = (uint64_t)c * den;
  uint64_t e_cd = ec > ed ? ec - ed : ed - ec;
  // Compare e/q. e_ab/b ≤ e_cd/d  iff  e_ab * d ≤ e_cd * b.
  if (e_ab * d <= e_cd * b) {
    *op = a;
    *oq = b;
  } else {
    *op = c;
    *oq = d;
  }
}

static uint8_t rf_code(uint64_t fout, uint64_t *fvco) {
  for (uint8_t c = 0; c <= 6; c++) {
    uint64_t v = fout << c;
    if (v > VCO_MAX) {
      return 0xFF;
    }
    if (v >= VCO_MIN) {
      *fvco = v;
      return c;
    }
  }
  return 0xFF;
}

// Band-select clock = (fref/R)/divider. Low: ≤125 kHz, div 1..255.
// High: ≤500 kHz, div 1..254.
static uint8_t band_div(uint32_t fref, uint32_t R, uint8_t *mode) {
  uint32_t low = (uint32_t)((fref + 125000ull * R - 1) / (125000ull * R));
  if (low < 1) {
    low = 1;
  }
  if (low <= 255) {
    *mode = 0;
    return (uint8_t)low;
  }
  uint32_t high = (uint32_t)((fref + 500000ull * R - 1) / (500000ull * R));
  if (high >= 1 && high <= 254) {
    *mode = 1;
    return (uint8_t)high;
  }
  return 0;
}

static uint32_t bit(uint32_t v, uint8_t s) { return v << s; }

void adf4351_assemble(const Adf4351Fields *f, uint32_t r[6]) {
  r[0] = bit(f->INT, 15) | bit(f->FRAC, 3);
  r[1] = bit(f->phase_adj, 28) | bit(f->prescaler, 27) | bit(f->phase, 15) | bit(f->MOD, 3) | 1u;
  r[2] = bit(f->noise_mode, 29) | bit(f->muxout, 26) | bit(f->ref_doubler, 25) |
         bit(f->ref_by_2, 24) | bit(f->r_counter, 14) | bit(f->dbl_buf, 13) |
         bit(f->charge_pump_current, 9) | bit(f->ldf, 8) | bit(f->ldp, 7) |
         bit(f->pd_polarity, 6) | bit(f->powerdown, 5) | bit(f->cp_three_state, 4) |
         bit(f->counter_reset, 3) | 2u;
  r[3] = bit(f->band_sel_clk_mode, 23) | bit(f->abp, 22) | bit(f->chg_cancel, 21) |
         bit(f->cycle_slip_reduce, 18) | bit(f->clk_div_mode, 15) | bit(f->clock_divider, 3) | 3u;
  r[4] = bit(f->feedback_sel, 23) | bit(f->rf_div_sel, 20) | bit(f->band_select_clkdiv, 12) |
         bit(f->vco_pwrdown, 11) | bit(f->mute_till_ld, 10) | bit(f->aux_out_sel, 9) |
         bit(f->aux_out_ena, 8) | bit(f->aux_out_pwr, 6) | bit(f->rf_out_ena, 5) |
         bit(f->rf_out_pwr, 3) | 4u;
  r[5] = bit(f->ld_pinmode & 3u, 22) | bit(3u, 19) | 5u;
}

void adf4351_disassemble(const uint32_t r[6], Adf4351Fields *f) {
  f->INT = (r[0] >> 15) & 0xFFFF;
  f->FRAC = (r[0] >> 3) & 0xFFF;
  f->phase_adj = (r[1] >> 28) & 1;
  f->prescaler = (r[1] >> 27) & 1;
  f->phase = (r[1] >> 15) & 0xFFF;
  f->MOD = (r[1] >> 3) & 0xFFF;
  f->noise_mode = (r[2] >> 29) & 3;
  f->muxout = (r[2] >> 26) & 7;
  f->ref_doubler = (r[2] >> 25) & 1;
  f->ref_by_2 = (r[2] >> 24) & 1;
  f->r_counter = (r[2] >> 14) & 0x3FF;
  f->dbl_buf = (r[2] >> 13) & 1;
  f->charge_pump_current = (r[2] >> 9) & 0xF;
  f->ldf = (r[2] >> 8) & 1;
  f->ldp = (r[2] >> 7) & 1;
  f->pd_polarity = (r[2] >> 6) & 1;
  f->powerdown = (r[2] >> 5) & 1;
  f->cp_three_state = (r[2] >> 4) & 1;
  f->counter_reset = (r[2] >> 3) & 1;
  f->band_sel_clk_mode = (r[3] >> 23) & 1;
  f->abp = (r[3] >> 22) & 1;
  f->chg_cancel = (r[3] >> 21) & 1;
  f->cycle_slip_reduce = (r[3] >> 18) & 1;
  f->clk_div_mode = (r[3] >> 15) & 3;
  f->clock_divider = (r[3] >> 3) & 0xFFF;
  f->feedback_sel = (r[4] >> 23) & 1;
  f->rf_div_sel = (r[4] >> 20) & 7;
  f->band_select_clkdiv = (r[4] >> 12) & 0xFF;
  f->vco_pwrdown = (r[4] >> 11) & 1;
  f->mute_till_ld = (r[4] >> 10) & 1;
  f->aux_out_sel = (r[4] >> 9) & 1;
  f->aux_out_ena = (r[4] >> 8) & 1;
  f->aux_out_pwr = (r[4] >> 6) & 3;
  f->rf_out_ena = (r[4] >> 5) & 1;
  f->rf_out_pwr = (r[4] >> 3) & 3;
  f->ld_pinmode = (r[5] >> 22) & 3;
}

bool adf4351_calculate(uint64_t fout, uint32_t fref, Adf4351Fields *f) {
  if (!f || !fref) {
    return false;
  }
  uint64_t fvco = 0;
  uint8_t code = rf_code(fout, &fvco);
  if (code == 0xFF) {
    return false;
  }
  uint16_t nmin = (fvco > PRE_MAX || fvco + fref > PRE_MAX) ? 75 : 23;
  uint32_t r_hi = fref / PFD_FLOOR;
  if (r_hi > 1023) {
    r_hi = 1023;
  }
  uint32_t r_cap = (uint32_t)((65535ull * fref) / fvco);
  if (r_cap < r_hi) {
    r_hi = r_cap;
  }
  uint32_t r_lo = (fref + PFD_FRAC - 1) / PFD_FRAC;
  if (r_lo < 1) {
    r_lo = 1;
  }

  uint16_t R = 0, INT = 0, FRAC = 0, MOD = 2;
  uint8_t integer_n = 0;

  uint64_t g = gcd64(fvco, fref);
  uint64_t a = fvco / g;
  uint64_t b = fref / g;
  if (b >= 1 && b <= r_hi && a >= 1 && a <= 65535ull) {
    uint32_t bb = (uint32_t)b;
    uint32_t aa = (uint32_t)a;
    uint32_t k = (aa < nmin) ? (nmin + aa - 1) / aa : 1;
    uint32_t rmin = (fref + PFD_INT - 1) / PFD_INT;
    uint32_t kp = (rmin + bb - 1) / bb;
    if (kp > k) {
      k = kp;
    }
    if (bb && k >= 1 && k <= r_hi / bb && k <= 65535u / aa) {
      uint32_t rr = bb * k;
      uint32_t nn = aa * k;
      uint8_t mode;
      if (nn >= nmin && nn <= 65535u && fref <= (uint64_t)PFD_INT * rr && band_div(fref, rr, &mode)) {
        R = (uint16_t)rr;
        INT = (uint16_t)nn;
        integer_n = 1;
      }
    }
  }

  if (!R && r_lo <= r_hi) {
    for (uint32_t rr = r_lo; rr <= r_hi; rr++) {
      uint16_t ip;
      uint32_t rem;
      if (!quot_rem(fvco, rr, fref, &ip, &rem) || ip < nmin || !rem) {
        continue;
      }
      uint32_t mod = fref / gcd32(rem, fref);
      if (mod < 2 || mod > 4095) {
        continue;
      }
      uint8_t mode;
      if (fref > (uint64_t)PFD_FRAC * rr || !band_div(fref, rr, &mode)) {
        continue;
      }
      R = (uint16_t)rr;
      INT = ip;
      FRAC = (uint16_t)(rem / (fref / mod));
      MOD = (uint16_t)mod;
      break;
    }
  }

  if (!R && r_lo <= r_hi) {
    uint16_t ip;
    uint32_t rem;
    if (quot_rem(fvco, r_lo, fref, &ip, &rem) && ip >= nmin) {
      uint16_t p, q;
      best_ratio(rem, fref, 4095, &p, &q);
      if (q <= 1) {
        if (p >= 1 && ip < 65535) {
          ip++;
        }
        p = 0;
        q = 2;
        integer_n = 1;
      }
      uint8_t mode;
      uint32_t lim = integer_n ? PFD_INT : PFD_FRAC;
      if (p < q && fref <= (uint64_t)lim * r_lo && band_div(fref, r_lo, &mode)) {
        R = (uint16_t)r_lo;
        INT = ip;
        FRAC = p;
        MOD = q;
      }
    }
  }
  if (!R) {
    return false;
  }

  uint64_t den = (uint64_t)MOD * R;
  uint64_t num = ((uint64_t)INT * MOD + FRAC) * fref;
  if (!den || num < VCO_MIN * den || num > VCO_MAX * den) {
    return false;
  }
  uint8_t prescaler = num > PRE_MAX * den;
  if (INT < (prescaler ? 75 : 23)) {
    return false;
  }
  uint8_t bs_mode = 0;
  uint8_t bs = band_div(fref, R, &bs_mode);
  if (!bs) {
    return false;
  }

  f->INT = INT;
  f->FRAC = integer_n ? 0 : FRAC;
  f->MOD = MOD;
  f->prescaler = prescaler;
  f->phase_adj = 0;
  f->phase = 1;
  f->r_counter = R;
  f->ref_doubler = 0;
  f->ref_by_2 = 0;
  f->feedback_sel = 1;
  f->rf_div_sel = code;
  f->band_sel_clk_mode = bs_mode;
  f->band_select_clkdiv = bs;
  f->clk_div_mode = 0;
  f->clock_divider = 0;
  f->cycle_slip_reduce = 0;
  if (integer_n || f->FRAC == 0) {
    f->FRAC = 0;
    f->ldf = 1;
    f->ldp = 1;
    f->abp = 1;
    f->chg_cancel = 1;
  } else {
    f->ldf = 0;
    f->ldp = 0;
    f->abp = 0;
    f->chg_cancel = 0;
  }
  return true;
}
