#include <kpwn/x86_64/cpu.h>

struct cpu_state cpu_now() {
  struct cpu_state ret;
  asm volatile(
      "movq %%cs, %0\n"
      "movq %%ss, %1\n"
      "movq %%rsp, %2\n"
      "pushfq\n"
      "popq  %3\n"
      : "=r"(ret.cs), "=r"(ret.ss), "=r"(ret.rsp), "=r"(ret.rflags)
      :
      : "memory");

  return ret;
}

void print_cpu_state(logf_t log, const struct cpu_state* state) {
  log("[cpu state] cs = %#lx", state->cs);
  log("[cpu state] ss = %#lx", state->ss);
  log("[cpu state] rsp = %#lx", state->rsp);
  log("[cpu state] rflags = %#lx", state->rflags);
}
