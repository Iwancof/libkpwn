#include "ktest.h"
#include <kpwn/utils.h>

// Tests of the swab64 byte-swap utility.

KTEST(swab, known_values) {
  struct {
    uint64_t in;
    uint64_t out;
  } cases[] = {
      {0ULL, 0ULL},
      {1ULL, 0x0100000000000000ULL},
      {0x0123456789ABCDEFULL, 0xEFCDAB8967452301ULL},
      {UINT64_MAX, UINT64_MAX},
  };

  for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
    KT_ASSERT_EQ(swab64(cases[i].in), cases[i].out);
  }
}

KTEST(swab, involution) {
  uint64_t values[] = {
      0ULL, 1ULL, 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL, UINT64_MAX,
  };

  for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
    uint64_t x = values[i];
    KT_ASSERT_EQ(swab64(swab64(x)), x);
  }
}
