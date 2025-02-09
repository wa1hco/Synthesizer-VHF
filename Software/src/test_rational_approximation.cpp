#include <Arduino.h>
#include "rational_approximation.h"
 
typedef struct {
  double target;
  uint32_t maxdenom;
  uint32_t expected_numerator;
  uint32_t expected_denominator;
  uint32_t maxiter;
} rational_test_case_t;

void test_rational_approx()
{
  char Msg[120];
  rational_t result;
 
  rational_test_case_t test[] = { 
    {0, 3000, 0, 1, 2},
    {1, 3000, 1, 1, 2},
    {0.5, 3000, 1, 2, 2},
    {0.5+1/3001.0, 3000, 751, 1501, 5},
    {1/3001.0, 2500, 1, 2500, 2},
    {1/3001.0, 1500, 0, 1, 2},
    {1/3001.0, 3001, 1, 3001, 2},
    {0.472757439, 1816, 564, 1193, 10},
    {0.472757439, 1817, 859, 1817, 10},
    {0.288, 100000000, 36, 125, 10},
    {0.47195, 1048575, 9439, 20000, 12},
    {0.471951, 1048575, 471951, 1000000, 15},
    {1/128.0, 1048575, 1, 128, 2},
    {17/65536.0, 1048575, 17, 65536, 3},
    {0.618033988749895, 1000000, 514229, 832040, 30}, // Golden ratio - 1, worst case in terms of number of iterations
    {0.141592653589793, 56, 1, 7, 2},
    {0.141592653589793, 57, 8, 57, 2},
    {0.141592653589793, 106, 15, 106, 3},
    {0.141592653589793, 113, 16, 113, 4},
    {0.141592653589793, 32000, 4527, 31972, 5},
    {0.141592653589793, 32989, 4671, 32989, 5},
  };
  uint32_t n_tests = sizeof(test)/sizeof(test[0]);
  char target_str[20];
 
  for(uint32_t ii = 0; ii < n_tests; ii++) {
    result = rational_approximation(test[ii].target, test[ii].maxdenom);
    dtostrf(test[ii].target, 8, 8, target_str);
    snprintf(Msg, 80, "target = %s, maxdenom = %lu, ", target_str, test[ii].maxdenom);
    Serial.print(Msg);
    snprintf(Msg, 80, "approx = %lu/%lu, iter = %lu ", result.numerator, result.denominator, result.iterations);
    Serial.print(Msg);
    if(result.numerator == test[ii].expected_numerator && 
       result.denominator == test[ii].expected_denominator && 
       result.iterations <= test[ii].maxiter) {
      Serial.println(" OK");
    } else {
      if(result.iterations > test[ii].maxiter) {
        Serial.printf("Too many iterations (max %lu) ", test[ii].maxiter);
      }
      Serial.printf("Expected %lu/%lu\n", test[ii].expected_numerator, test[ii].expected_denominator);
    }
  } // for each test
} // test_rational_approx()