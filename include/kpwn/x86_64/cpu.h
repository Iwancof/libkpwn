#ifndef _KPWN_X86_64_CPU_
#define _KPWN_X86_64_CPU_

#include <kpwn/logger.h>
#include <stddef.h>

struct cpu_state {
  size_t cs;
  size_t ss;
  size_t rsp;
  size_t rflags;
};

struct cpu_state cpu_now();
void print_cpu_state(logf_t log, const struct cpu_state *state);

#endif
