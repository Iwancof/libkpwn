#include <criterion/criterion.h>
#include <kpwn/utils.h>

// Tests of pack/unpack utils

Test(pc64_roundtrip, test) {
  uint64_t values[] = {
    0ULL,
    1ULL,
    0x0123456789ABCDEFULL,
    UINT64_MAX,
  };

  for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
    char buf[sizeof(uint64_t)] = {0};
    up64(values[i], buf);
    uint64_t out = pc64(buf);
    cr_assert(out == values[i]);
  }
}

Test(pc32_roundtrip, test) {
  uint32_t values[] = {
    0U,
    1U,
    0x89ABCDEFU,
    UINT32_MAX,
  };

  for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
    char buf[sizeof(uint32_t)] = {0};
    up32(values[i], buf);
    uint32_t out = pc32(buf);
    cr_assert(out == values[i]);
  }
}

Test(pc16_roundtrip, test) {
  uint16_t values[] = {
    0,
    1,
    0xBEEFu,
    UINT16_MAX,
  };

  for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
    char buf[sizeof(uint16_t)] = {0};
    up16(values[i], buf);
    uint16_t out = pc16(buf);
    cr_assert(out == values[i]);
  }
}

Test(pc8_roundtrip, test) {
  uint8_t values[] = {
    0,
    1,
    0xAB,
    UINT8_MAX,
  };

  for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
    char buf[sizeof(uint8_t)] = {0};
    up8(values[i], buf);
    uint8_t out = pc8(buf);
    cr_assert(out == values[i]);
  }
}

