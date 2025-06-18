#define _GNU_SOURCE

#include <kpwn/flow.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/personality.h>
#include <unistd.h>

struct cpu_state cpu_now() {
  struct cpu_state ret;
  asm volatile(
      "movq %%cs, %0\n"
      "movq %%ss, %1\n"
      "movq %%rsp, %2\n"
      "pushfq\n"
      "popq  %3\n"
      : "=r"(ret.cs), "=r"(ret.ss), "=r"(ret.rsp),
        "=r"(ret.rflags)
      :
      : "memory");

  return ret;
}

void print_cpu_state(logf_t log,
                     const struct cpu_state* state) {
  log("[cpu state] cs = %lx", state->cs);
  log("[cpu state] ss = %lx", state->ss);
  log("[cpu state] rsp = %lx", state->rsp);
  log("[cpu state] rflags = %lx", state->rflags);
}

void noaslr() {
  if (personality(ADDR_NO_RANDOMIZE) & ADDR_NO_RANDOMIZE) {
    log_succ("killed aslr");
    return;
  }

  log_warn("cannot use argv, envp with noaslr");
  SYSCHK(execl("/proc/self/exe", "/proc/self/exe", NULL));
}

void process_assign_to_core(int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  SYSCHK(
      sched_setaffinity(getpid(), sizeof(cpuset), &cpuset));

  log_succ("assigned to core %d", core_id);
}

void thread_assign_to_core(int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  SYSCHK(pthread_setaffinity_np(pthread_self(),
                                sizeof(cpuset), &cpuset));

  log_succ("Thread assigned to core %d", core_id);
}