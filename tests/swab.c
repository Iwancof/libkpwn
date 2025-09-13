#include <criterion/criterion.h>
#include <kpwn/utils.h>

// Tests of swab64 utility

Test(swab64_known_values, test) {
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
    cr_assert_eq(swab64(cases[i].in), cases[i].out);
  }
}

Test(swab64_involution, test) {
  uint64_t values[] = {
      0ULL, 1ULL, 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL, UINT64_MAX,
  };

  for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
    uint64_t x = values[i];
    cr_assert_eq(swab64(swab64(x)), x);
  }
}
