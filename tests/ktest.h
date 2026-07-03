#ifndef _KTEST_H_
#define _KTEST_H_

// ktest — a tiny, zero-dependency unit-test framework for libkpwn.
//
// Rationale: libkpwn gets vendored into CTF challenge directories and built in
// minimal environments; depending on a system test framework (Criterion) makes
// `make test` and CI fragile. ktest is two files (ktest.h + ktest.c) with no
// external deps, so tests build with nothing but a C compiler.
//
// Output is intentionally greppable:
//   [ktest] PASS <suite>.<name>
//   [ktest] FAIL <suite>.<name>
//   [ktest] at <file>:<line>: <message>
//   [ktest] summary total=N passed=N failed=N
//
// Usage:
//   #include "ktest.h"
//   KTEST(mysuite, mytest) {
//     KT_ASSERT(1 + 1 == 2);
//     KT_ASSERT_EQ(swab64(0), 0);
//   }
// main() is provided by ktest.c — link it into the test binary.

#include <setjmp.h>
#include <stdarg.h>

struct ktest {
  const char *suite;
  const char *name;
  const char *file;
  void (*fn)(void);
  struct ktest *next;
};

extern struct ktest *ktest_head; // intrusive list of registered tests
extern jmp_buf ktest_env;        // per-test abort target
extern int ktest_current_failed; // set when the running test fails an assert

void ktest_register(struct ktest *t);
void ktest_fail(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define KTEST(suite_, name_)                                                   \
  static void ktest_fn_##suite_##_##name_(void);                               \
  static struct ktest ktest_node_##suite_##_##name_ = {                        \
      #suite_, #name_, __FILE__, ktest_fn_##suite_##_##name_, 0};              \
  __attribute__((constructor)) static void ktest_ctor_##suite_##_##name_(      \
      void) {                                                                  \
    ktest_register(&ktest_node_##suite_##_##name_);                            \
  }                                                                            \
  static void ktest_fn_##suite_##_##name_(void)

// Fail the current test and abort it (skip the rest of the test body).
#define KT_ABORT(...)                                                          \
  do {                                                                         \
    ktest_fail(__FILE__, __LINE__, __VA_ARGS__);                               \
    longjmp(ktest_env, 1);                                                     \
  } while (0)

#define KT_ASSERT(cond)                                                        \
  do {                                                                         \
    if (!(cond))                                                               \
      KT_ABORT("assertion failed: %s", #cond);                                 \
  } while (0)

#define KT_ASSERT_EQ(a, b)                                                     \
  do {                                                                         \
    unsigned long long __a = (unsigned long long)(a);                          \
    unsigned long long __b = (unsigned long long)(b);                          \
    if (__a != __b)                                                            \
      KT_ABORT("expected (%s) == (%s), got 0x%llx vs 0x%llx", #a, #b, __a,     \
               __b);                                                           \
  } while (0)

#define KT_ASSERT_STR_EQ(a, b)                                                 \
  do {                                                                         \
    const char *__a = (a);                                                     \
    const char *__b = (b);                                                     \
    if (__builtin_strcmp(__a, __b) != 0)                                       \
      KT_ABORT("expected \"%s\" == \"%s\"", __a, __b);                         \
  } while (0)

#endif
