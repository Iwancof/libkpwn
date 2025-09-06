#ifndef _KPWN_UTILS_
#define _KPWN_UTILS_

#include <errno.h>
#include <kpwn/logger.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __KPWN_CONCAT(a, b) a##b
#define __KPWN_CONCAT2(a, b) __KPWN_CONCAT(a, b)
#define __KPWN_UNIQUE __KPWN_CONCAT2(unique_, __COUNTER__)

#define REP_FORWARD 0
#define REP_BACKWARD 1

#define __DIREC_HELPER(direc, forward, backward) (((direc) == REP_FORWARD) ? (forward) : (backward))

#define __KPWN_REP_IMPL(idx_ident, start, end, step, direc)               \
  for (ptrdiff_t idx_ident = __DIREC_HELPER((direc), (start), (end) - 1); \
       __DIREC_HELPER((direc), idx_ident < (end), (start) <= idx_ident);  \
       idx_ident += (step) * __DIREC_HELPER(direc, 1, -1))

#define __KPWN_REP_ARG_5(idx, start, end, step, direc) __KPWN_REP_IMPL(idx, start, end, step, direc)
#define __KPWN_REP_ARG_4(idx, start, end, step) __KPWN_REP_IMPL(idx, start, end, step, REP_FORWARD)
#define __KPWN_REP_ARG_3(idx, start, end) __KPWN_REP_IMPL(idx, start, end, 1, REP_FORWARD)
#define __KPWN_REP_ARG_2(idx, end) __KPWN_REP_IMPL(idx, 0, end, 1, REP_FORWARD)
#define __KPWN_REP_ARG_1(end) __KPWN_REP_IMPL(__KPWN_UNIQUE, 0, end, 1, REP_FORWARD)

#define __KPWN_REP_ARG_RESOLVER(_1, _2, _3, _4, _5, NAME, ...) NAME

// repeat macro
//
// REP(n): for (0..n)
// REP(i, n): for i in (0..n)
// REP(i, start, end): for i in (start..end)
// REP(i, start, end, step): for i in (start..end by step)
// REP(i, start, end, step, direc): direc(forward, backward)
// --> for i in (start..end by step) or for i in (end..start
// by step)

#define REP(...)                                                                                               \
  __KPWN_REP_ARG_RESOLVER(__VA_ARGS__, __KPWN_REP_ARG_5, __KPWN_REP_ARG_4, __KPWN_REP_ARG_3, __KPWN_REP_ARG_2, \
                          __KPWN_REP_ARG_1)(__VA_ARGS__)

#define WAIT                                                    \
  printf("waiting at %d. Press Enter to continue\n", __LINE__); \
  getc(stdin);

#define SYSCHK(eval)                                                                            \
  ({                                                                                            \
    typeof(eval) __ret = (eval);                                                                \
    if (__ret < 0 || __ret == 0xffffffff || __ret == 0xffffffffffffffff) {                      \
      log_error("SYSCHK error at " __FILE__ ":%d %s = %s\n", __LINE__, #eval, strerror(errno)); \
      exit(EXIT_FAILURE);                                                                       \
    }                                                                                           \
    __ret;                                                                                      \
  })

#define ASSERT(cond)                                                      \
  ({                                                                      \
    if (!(cond)) {                                                        \
      log_error("ASSERT error at " __FILE__ ":%d %s\n", __LINE__, #cond); \
      exit(EXIT_FAILURE);                                                 \
    }                                                                     \
  })

#define ASSERT_MSG(cond, msg)                                                      \
  ({                                                                               \
    if (!(cond)) {                                                                 \
      log_error("ASSERT error at " __FILE__ ":%d %s: %s\n", __LINE__, #cond, msg); \
      exit(EXIT_FAILURE);                                                          \
    }                                                                              \
  })

#define DIE(msg)                                                             \
  ({                                                                         \
    log_error("DIE error at " __FILE__ ":%d %s: %s\n", __LINE__, #msg, msg); \
    exit(EXIT_FAILURE);                                                      \
  })

#define ARRAY_SIZE(array)              \
  ({                                   \
    __typeof__(array) __arr = (array); \
    sizeof(__arr) / sizeof(__arr[0]);  \
  })

#define ARRAY_SIZE_BYTES(array)        \
  ({                                   \
    __typeof__(array) __arr = (array); \
    sizeof(__arr);                     \
  })

#define ARRAY_END(array) (&(array[ARRAY_SIZE(array)]))

#define MIN(a, b)            \
  ({                         \
    __typeof__(a) __a = (a); \
    __typeof__(b) __b = (b); \
    __a < __b ? __a : __b;   \
  })

#define MAX(a, b)            \
  ({                         \
    __typeof__(a) __a = (a); \
    __typeof__(b) __b = (b); \
    __a > __b ? __a : __b;   \
  })

void proc_info(logf_t log);

extern const char root_without_password[];

struct count_sort_data {
  uint64_t data;
  size_t counter;
};

struct count_sort_data count_sort(const uint64_t* data, size_t len);

uint64_t pc64(char* bytes);
void up64(uint64_t value, char* dst);

uint32_t pc32(char* bytes);
void up32(uint32_t value, char* dst);

uint16_t pc16(char* bytes);
void up16(uint16_t value, char* dst);

uint8_t pc8(char* bytes);
void up8(uint8_t value, char* dst);

uint64_t swab64(uint64_t value);

#endif
