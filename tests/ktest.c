#include "ktest.h"

#include <stdio.h>

struct ktest *ktest_head = 0;
jmp_buf ktest_env;
int ktest_current_failed = 0;

void ktest_register(struct ktest *t) {
  // Push front; execution order across translation units is unspecified anyway.
  t->next = ktest_head;
  ktest_head = t;
}

void ktest_fail(const char *file, int line, const char *fmt, ...) {
  ktest_current_failed = 1;
  fprintf(stderr, "[ktest] at %s:%d: ", file, line);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

int main(void) {
  int total = 0, failed = 0;
  for (struct ktest *t = ktest_head; t; t = t->next) {
    total++;
    ktest_current_failed = 0;
    if (setjmp(ktest_env) == 0)
      t->fn();
    if (ktest_current_failed) {
      failed++;
      printf("[ktest] FAIL %s.%s\n", t->suite, t->name);
    } else {
      printf("[ktest] PASS %s.%s\n", t->suite, t->name);
    }
  }
  printf("[ktest] summary total=%d passed=%d failed=%d\n", total,
         total - failed, failed);
  return failed ? 1 : 0;
}
