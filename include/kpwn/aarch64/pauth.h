#ifndef _KPWN_AARCH64_PAUTH_
#define _KPWN_AARCH64_PAUTH_

#include <stdint.h>

typedef struct {
  uint64_t lo, hi;
} pauth_key;

uint64_t pauth_addpac(uint64_t ptr, uint64_t modifier,
                      pauth_key key);

#endif
