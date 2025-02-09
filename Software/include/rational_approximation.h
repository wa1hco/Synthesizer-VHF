// Type to represent (positive) rational numbers
#ifndef RATIONAL_APPROXIMATION_H
#define RATIONAL_APPROXIMATION_H

typedef struct {
    uint32_t numerator;
    uint32_t denominator;
    uint32_t iterations;   // Just for debugging
  } rational_t;

rational_t rational_approximation(double target, uint32_t maxdenom);
void test_rational_approx();
#endif