// Type to represent (positive) rational numbers
#ifndef RATIONAL_APPROXIMATION_H
#define RATIONAL_APPROXIMATION_H

typedef struct {
    uint16_t numerator;
    uint16_t denominator;
    uint16_t iterations;   // Just for debugging
  } rational_t;

rational_t rational_approximation(float target, uint16_t maxdenom);
void test_rational_approx();
#endif