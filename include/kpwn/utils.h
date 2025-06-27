#ifndef _KPWN_UTILS_
#define _KPWN_UTILS_

#include <errno.h>
#include <kpwn/logger.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONCAT(a, b) a##b
#define CONCAT2(a, b) CONCAT(a, b)
#define UNIQUE CONCAT2(unique_, __COUNTER__)

#define REP3(s, i, n) for (size_t i = (s); i < (n); i++)
#define REP2(i, n) REP3(0, i, n)
#define REP1(n) REP2(UNIQUE, n)

#undef CONCAT
#undef CONCAT2
#undef UNIQUE

#define WAIT                                         \
  printf("waiting at %d. Press Enter to continue\n", \
         __LINE__);                                  \
  getc(stdin);

#define SYSCHK(eval)                               \
  ({                                               \
    typeof(eval) __ret = (eval);                   \
    if (__ret < 0 || __ret == 0xffffffff ||        \
        __ret == 0xffffffffffffffff) {             \
      log_error("SYSCHK error at " __FILE__        \
                ":%d %s = %s\n",                   \
                __LINE__, #eval, strerror(errno)); \
      exit(EXIT_FAILURE);                          \
    }                                              \
    __ret;                                         \
  })

#define ASSERT(cond)                                    \
  ({                                                    \
    if (!(cond)) {                                      \
      log_error("ASSERT error at " __FILE__ ":%d %s\n", \
                __LINE__, #cond);                       \
      exit(EXIT_FAILURE);                               \
    }                                                   \
  })

#define ASSERT_MSG(cond, msg)               \
  ({                                        \
    if (!(cond)) {                          \
      log_error("ASSERT error at " __FILE__ \
                ":%d %s: %s\n",             \
                __LINE__, #cond, msg);      \
      exit(EXIT_FAILURE);                   \
    }                                       \
  })

#define DIE(msg)                                       \
  ({                                                   \
    log_error("DIE error at " __FILE__ ":%d %s: %s\n", \
              __LINE__, #msg, msg);                    \
    exit(EXIT_FAILURE);                                \
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

struct count_sort_data count_sort(const uint64_t* data,
                                  size_t len);

uint64_t pc64(char* bytes);
void up64(uint64_t value, char* dst);

uint32_t pc32(char* bytes);
void up32(uint32_t value, char* dst);

uint16_t pc16(char* bytes);
void up16(uint16_t value, char* dst);

uint8_t pc8(char* bytes);
void up8(uint8_t value, char* dst);

#endif
