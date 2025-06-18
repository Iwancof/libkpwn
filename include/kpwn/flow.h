#ifndef _KPWN_FLOW_
#define _KPWN_FLOW_

#include <kpwn/logger.h>
#include <stddef.h>
#include <stdio.h>

struct cpu_state {
  size_t cs;
  size_t ss;
  size_t rsp;
  size_t rflags;
};

struct cpu_state cpu_now();

void noaslr();

void print_cpu_state(logf_t log,
                     const struct cpu_state* state);

__attribute__((naked, noreturn)) void win();

void process_assign_to_core(int core_id);
void thread_assign_to_core(int core_id);

#endif