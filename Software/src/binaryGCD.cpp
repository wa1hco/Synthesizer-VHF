// from perplexity search
#include <Arduino.h>

uint32_t binaryGCD(uint32_t a, uint32_t b) {
    if (a == 0) return b;
    if (b == 0) return a;
    
    int shift = 0;
    while (((a | b) & 1) == 0) {
      a >>= 1;
      b >>= 1;
      shift++;
    }
    while ((a & 1) == 0) a >>= 1;
    
    while (b != 0) {
      while ((b & 1) == 0) b >>= 1;
      if (a > b) {
        uint32_t t = b;
        b = a;
        a = t;
      }
      b -= a;
    }
    
    return a << shift;
  }