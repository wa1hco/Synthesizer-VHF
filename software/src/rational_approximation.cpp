// Farey sequence-based rational approximation of numbers.
// Per Magnusson, 2024, 2025
// MIT licence, http://www.opensource.org/licenses/mit-license.php

#include <Arduino.h>
#include "rational_approximation.h"

// Find the best rational approximation to a number between 0 and 1.
//
// target - a number between 0 and 1 (inclusive)
// maxdenom - the maximum allowed denominator
//
// The algorithm is based on Farey sequences/fractions. See
// https://web.archive.org/web/20181119092100/https://nrich.maths.org/6596
// a, b, c, d notation from
// https://en.wikipedia.org/wiki/Farey_sequence is used here (not
// from the above reference). I.e. narrow the interval between a/b
// and c/d by splitting it using the mediant (a+c)/(b+d) until we are
// close enough with either endpoint, or we have a denominator that is
// bigger than what is allowed.
// Start with the interval 0 to 1 (i.e. 0/1 to 1/1).
// A simple implementation of just calculating the mediant (a+c)/(b+d) and
// iterating with the mediant replacing the worst value of a/b and c/d is very
// inefficient in cases where the target is close to a rational number
// with a small denominator, like e.g. when approximating 10^-6.
// The straightforward algorithm would need about 10^6 iterations as it
// would try all of 1/1, 1/2, 1/3, 1/4, 1/5 etc. To resolve this slow
// convergence, at each step, it is calculated how many times the
// interval will need to be narrowed from the same side and all those
// steps are taken at once.


rational_t rational_approximation(float target, uint16_t maxdenom) {
  rational_t retval;
  float mediant;  // float does not have enough resolution
                   // to deal with single-digit differences
                   // between numbers above 10^8.
  float N, Ndenom, Ndenom_min;
  uint16_t a = 0, b = 1, c = 1, d = 1, ac, bd, Nint;
  const int maxIter = 100;

  if(target > 1) {
    // Invalid
    Serial.println("rat_approx: target > 1");
    retval.numerator = 1;
    retval.denominator = 1;
    return retval;
  }
  if(target < 0) {
    // Invalid
    Serial.println("rat_approx: target < 0");
    retval.numerator = 0;
    retval.denominator = 1;
    return retval;
  }
  if(maxdenom < 1) {
    maxdenom = 1;
  }

  mediant = 0;
  Ndenom_min = 1/((float) 10*maxdenom);
  uint16_t ii = 0;
  // Farey approximation loop
  while(1) {
    ac = a+c;
    bd = b+d;
    if(bd > maxdenom || ii > maxIter) {
      // The denominator has become too big, or too many iterations.
        // Select the best of a/b and c/d.
      if(target - a/(float)b < c/(float)d - target) {
        ac = a;
        bd = b;
      } else {
        ac = c;
        bd = d;
      }
      break;
    }
    ii++;
    mediant = ac/(float)bd;
    if(target < mediant) {
      // Discard c/d since the mediant is closer to the target.
      // How many times in a row should we do that?
      // N = (c - target*d)/(target*b - a), but need to check for division by zero
      Ndenom = target * (float)b - (float)a;
      if(Ndenom < Ndenom_min) {
        // Division by zero, or close to it!
        // This means that a/b is a very good approximation
        // as we would need to update the c/d side a
        // very large number of times to get closer.
        // Use a/b and exit the loop.
        ac = a;
        bd = b;
        break;
      }
      N = (c - target * (float)d)/Ndenom;
      Nint = floor(N);
      if(Nint < 1) {
        // Nint should be at least 1, a rounding error may cause N to be just less than that
        Nint = 1;
      }
      // Check if the denominator will become too large
      if(d + Nint*b > maxdenom) {
        // Limit N, as the denominator would otherwise become too large
        N = (maxdenom - d)/(float)b;
        Nint = floor(N);
      }
      // Fast forward to a good c/d.
      c = c + Nint*a;
      d = d + Nint*b;

    } else {
      // Discard a/b since the mediant is closer to the target.
      // How many times in a row should we do that?
      // N = (target*b - a)/(c - target*d), but need to check for division by zero
      Ndenom = (float)c - target * (float)d;
      if(Ndenom < Ndenom_min) {
        // Division by zero, or close to it!
        // This means that c/d is a very good approximation
        // as we would need to update the a/b side a
        // very large number of times to get closer.
        // Use c/d and exit the loop.
        ac = c;
        bd = d;
        break;
      }
      N = (target * (float)b - a)/Ndenom;
      Nint = floor(N);
      if(Nint < 1) {
        // Nint should be at least 1, a rounding error may cause N to be just less than that
        Nint = 1;
      }
      // Check if the denominator will become too large
      if(b + Nint*d > maxdenom) {
        // Limit N, as the denominator would otherwise become too large
        N = (maxdenom - b)/(float)d;
        Nint = floor(N);
      }
      // Fast forward to a good a/b.
      a = a + Nint*c;
      b = b + Nint*d;
    }
  }

  retval.numerator = ac;
  retval.denominator = bd;
  retval.iterations = ii;
  return retval;
}
