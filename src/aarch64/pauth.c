#include <kpwn/aarch64/pauth.h>
#include <kpwn/utils.h>
#include <stdint.h>

#define MAKE_64BIT_MASK(shift, length) \
  (((~0ULL) >> (64 - (length))) << (shift))

uint64_t extract64(uint64_t value, int start, int length) {
  ASSERT(start >= 0 && length > 0 && length <= 64 - start);
  return (value >> start) & (~0ULL >> (64 - length));
}

uint32_t extract32(uint32_t value, int start, int length) {
  ASSERT(start >= 0 && length > 0 && length <= 32 - start);
  return (value >> start) & (~0U >> (32 - length));
}

uint64_t pac_cell_shuffle(uint64_t i) {
  uint64_t o = 0;

  o |= extract64(i, 52, 4);
  o |= extract64(i, 24, 4) << 4;
  o |= extract64(i, 44, 4) << 8;
  o |= extract64(i, 0, 4) << 12;

  o |= extract64(i, 28, 4) << 16;
  o |= extract64(i, 48, 4) << 20;
  o |= extract64(i, 4, 4) << 24;
  o |= extract64(i, 40, 4) << 28;

  o |= extract64(i, 32, 4) << 32;
  o |= extract64(i, 12, 4) << 36;
  o |= extract64(i, 56, 4) << 40;
  o |= extract64(i, 20, 4) << 44;

  o |= extract64(i, 8, 4) << 48;
  o |= extract64(i, 36, 4) << 52;
  o |= extract64(i, 16, 4) << 56;
  o |= extract64(i, 60, 4) << 60;

  return o;
}

uint64_t pac_cell_inv_shuffle(uint64_t i) {
  uint64_t o = 0;

  o |= extract64(i, 12, 4);
  o |= extract64(i, 24, 4) << 4;
  o |= extract64(i, 48, 4) << 8;
  o |= extract64(i, 36, 4) << 12;

  o |= extract64(i, 56, 4) << 16;
  o |= extract64(i, 44, 4) << 20;
  o |= extract64(i, 4, 4) << 24;
  o |= extract64(i, 16, 4) << 28;

  o |= i & MAKE_64BIT_MASK(32, 4);
  o |= extract64(i, 52, 4) << 36;
  o |= extract64(i, 28, 4) << 40;
  o |= extract64(i, 8, 4) << 44;

  o |= extract64(i, 20, 4) << 48;
  o |= extract64(i, 0, 4) << 52;
  o |= extract64(i, 40, 4) << 56;
  o |= i & MAKE_64BIT_MASK(60, 4);

  return o;
}

uint64_t pac_sub(uint64_t i) {
  static const uint8_t sub[16] = {
      0xb, 0x6, 0x8, 0xf, 0xc, 0x0, 0x9, 0xe,
      0x3, 0x7, 0x4, 0x5, 0xd, 0x2, 0x1, 0xa,
  };
  uint64_t o = 0;
  int b;

  for (b = 0; b < 64; b += 4) {
    o |= (uint64_t)sub[(i >> b) & 0xf] << b;
  }
  return o;
}

uint64_t pac_sub1(uint64_t i) {
  static const uint8_t sub1[16] = {
      0xa, 0xd, 0xe, 0x6, 0xf, 0x7, 0x3, 0x5,
      0x9, 0x8, 0x0, 0xc, 0xb, 0x1, 0x2, 0x4,
  };
  uint64_t o = 0;
  int b;

  for (b = 0; b < 64; b += 4) {
    o |= (uint64_t)sub1[(i >> b) & 0xf] << b;
  }
  return o;
}

uint64_t pac_inv_sub(uint64_t i) {
  static const uint8_t inv_sub[16] = {
      0x5, 0xe, 0xd, 0x8, 0xa, 0xb, 0x1, 0x9,
      0x2, 0x6, 0xf, 0x0, 0x4, 0xc, 0x7, 0x3,
  };
  uint64_t o = 0;
  int b;

  for (b = 0; b < 64; b += 4) {
    o |= (uint64_t)inv_sub[(i >> b) & 0xf] << b;
  }
  return o;
}

int rot_cell(int cell, int n) {
  /* 4-bit rotate left by n.  */
  cell |= cell << 4;
  return extract32(cell, 4 - n, 4);
}

uint64_t pac_mult(uint64_t i) {
  uint64_t o = 0;
  int b;

  for (b = 0; b < 4 * 4; b += 4) {
    int i0, i4, i8, ic, t0, t1, t2, t3;

    i0 = extract64(i, b, 4);
    i4 = extract64(i, b + 4 * 4, 4);
    i8 = extract64(i, b + 8 * 4, 4);
    ic = extract64(i, b + 12 * 4, 4);

    t0 =
        rot_cell(i8, 1) ^ rot_cell(i4, 2) ^ rot_cell(i0, 1);
    t1 =
        rot_cell(ic, 1) ^ rot_cell(i4, 1) ^ rot_cell(i0, 2);
    t2 =
        rot_cell(ic, 2) ^ rot_cell(i8, 1) ^ rot_cell(i0, 1);
    t3 =
        rot_cell(ic, 1) ^ rot_cell(i8, 2) ^ rot_cell(i4, 1);

    o |= (uint64_t)t3 << b;
    o |= (uint64_t)t2 << (b + 4 * 4);
    o |= (uint64_t)t1 << (b + 8 * 4);
    o |= (uint64_t)t0 << (b + 12 * 4);
  }
  return o;
}

uint64_t tweak_cell_rot(uint64_t cell) {
  return (cell >> 1) | (((cell ^ (cell >> 1)) & 1) << 3);
}

uint64_t tweak_shuffle(uint64_t i) {
  uint64_t o = 0;

  o |= extract64(i, 16, 4) << 0;
  o |= extract64(i, 20, 4) << 4;
  o |= tweak_cell_rot(extract64(i, 24, 4)) << 8;
  o |= extract64(i, 28, 4) << 12;

  o |= tweak_cell_rot(extract64(i, 44, 4)) << 16;
  o |= extract64(i, 8, 4) << 20;
  o |= extract64(i, 12, 4) << 24;
  o |= tweak_cell_rot(extract64(i, 32, 4)) << 28;

  o |= extract64(i, 48, 4) << 32;
  o |= extract64(i, 52, 4) << 36;
  o |= extract64(i, 56, 4) << 40;
  o |= tweak_cell_rot(extract64(i, 60, 4)) << 44;

  o |= tweak_cell_rot(extract64(i, 0, 4)) << 48;
  o |= extract64(i, 4, 4) << 52;
  o |= tweak_cell_rot(extract64(i, 40, 4)) << 56;
  o |= tweak_cell_rot(extract64(i, 36, 4)) << 60;

  return o;
}

uint64_t tweak_cell_inv_rot(uint64_t cell) {
  return ((cell << 1) & 0xf) | ((cell & 1) ^ (cell >> 3));
}

uint64_t tweak_inv_shuffle(uint64_t i) {
  uint64_t o = 0;

  o |= tweak_cell_inv_rot(extract64(i, 48, 4));
  o |= extract64(i, 52, 4) << 4;
  o |= extract64(i, 20, 4) << 8;
  o |= extract64(i, 24, 4) << 12;

  o |= extract64(i, 0, 4) << 16;
  o |= extract64(i, 4, 4) << 20;
  o |= tweak_cell_inv_rot(extract64(i, 8, 4)) << 24;
  o |= extract64(i, 12, 4) << 28;

  o |= tweak_cell_inv_rot(extract64(i, 28, 4)) << 32;
  o |= tweak_cell_inv_rot(extract64(i, 60, 4)) << 36;
  o |= tweak_cell_inv_rot(extract64(i, 56, 4)) << 40;
  o |= tweak_cell_inv_rot(extract64(i, 16, 4)) << 44;

  o |= extract64(i, 32, 4) << 48;
  o |= extract64(i, 36, 4) << 52;
  o |= extract64(i, 40, 4) << 56;
  o |= tweak_cell_inv_rot(extract64(i, 44, 4)) << 60;

  return o;
}

uint64_t pauth_computepac_architected(uint64_t data,
                                      uint64_t modifier,
                                      pauth_key key,
                                      int isqarma3) {
  static const uint64_t RC[5] = {
      0x0000000000000000ull, 0x13198A2E03707344ull,
      0xA4093822299F31D0ull, 0x082EFA98EC4E6C89ull,
      0x452821E638D01377ull,
  };
  const uint64_t alpha = 0xC0AC29B7C97C50DDull;
  int iterations = isqarma3 ? 2 : 4;
  uint64_t key0 = key.hi, key1 = key.lo;
  uint64_t workingval, runningmod, roundkey, modk0;
  int i;

  modk0 = (key0 << 63) | ((key0 >> 1) ^ (key0 >> 63));
  runningmod = modifier;
  workingval = data ^ key0;

  for (i = 0; i <= iterations; ++i) {
    roundkey = key1 ^ runningmod;
    workingval ^= roundkey;
    workingval ^= RC[i];
    if (i > 0) {
      workingval = pac_cell_shuffle(workingval);
      workingval = pac_mult(workingval);
    }
    if (isqarma3) {
      workingval = pac_sub1(workingval);
    } else {
      workingval = pac_sub(workingval);
    }
    runningmod = tweak_shuffle(runningmod);
  }
  roundkey = modk0 ^ runningmod;
  workingval ^= roundkey;
  workingval = pac_cell_shuffle(workingval);
  workingval = pac_mult(workingval);
  if (isqarma3) {
    workingval = pac_sub1(workingval);
  } else {
    workingval = pac_sub(workingval);
  }
  workingval = pac_cell_shuffle(workingval);
  workingval = pac_mult(workingval);
  workingval ^= key1;
  workingval = pac_cell_inv_shuffle(workingval);
  if (isqarma3) {
    workingval = pac_sub1(workingval);
  } else {
    workingval = pac_inv_sub(workingval);
  }
  workingval = pac_mult(workingval);
  workingval = pac_cell_inv_shuffle(workingval);
  workingval ^= key0;
  workingval ^= runningmod;
  for (i = 0; i <= iterations; ++i) {
    if (isqarma3) {
      workingval = pac_sub1(workingval);
    } else {
      workingval = pac_inv_sub(workingval);
    }
    if (i < iterations) {
      workingval = pac_mult(workingval);
      workingval = pac_cell_inv_shuffle(workingval);
    }
    runningmod = tweak_inv_shuffle(runningmod);
    roundkey = key1 ^ runningmod;
    workingval ^= RC[iterations - i];
    workingval ^= roundkey;
    workingval ^= alpha;
  }
  workingval ^= modk0;

  return workingval;
}

int64_t sextract64(uint64_t value, int start, int length) {
  ASSERT(start >= 0 && length > 0 && length <= 64 - start);
  return ((int64_t)(value << (64 - length - start))) >>
         (64 - length);
}

uint64_t pauth_computepac(uint64_t data, uint64_t modifier,
                          pauth_key key) {
  return pauth_computepac_architected(data, modifier, key,
                                      0);
}

uint64_t deposit64(uint64_t value, int start, int length,
                   uint64_t fieldval) {
  uint64_t mask;
  ASSERT(start >= 0 && length > 0 && length <= 64 - start);
  mask = (~0ULL >> (64 - length)) << start;
  return (value & ~mask) | ((fieldval << start) & mask);
}

uint64_t pauth_addpac(uint64_t ptr, uint64_t modifier,
                      pauth_key key) {
  uint64_t pac, ext_ptr, ext;
  int64_t test;
  int bot_bit, top_bit;

  ext = sextract64(ptr, 63, 1);

  top_bit = 64;
  bot_bit = 64 - 16;
  ext_ptr = deposit64(ptr, bot_bit, top_bit - bot_bit, ext);

  pac = pauth_computepac(ext_ptr, modifier, key);

  test = sextract64(ptr, bot_bit, top_bit - bot_bit);
  if (test != 0 && test != -1) {
    pac ^= MAKE_64BIT_MASK(top_bit - 2, 1);
  }

  pac ^= ptr;
  ptr &= MAKE_64BIT_MASK(0, bot_bit);
  pac &= ~(MAKE_64BIT_MASK(55, 1) |
           MAKE_64BIT_MASK(0, bot_bit));
  ext &= MAKE_64BIT_MASK(55, 1);
  return pac | ext | ptr;
}
