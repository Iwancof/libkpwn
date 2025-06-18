#define _GNU_SOURCE

#include <kpwn/arch.h>
#include <kpwn/flow.h>
#include <kpwn/hexdump.h>
#include <kpwn/interactive.h>
#include <kpwn/kernel.h>
#include <kpwn/logger.h>
#include <kpwn/memory.h>
#include <kpwn/slog.h>
#include <kpwn/utils.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

size_t call_me(int x, int y) {
  printf("x = %d, y = %d\n", x, y);

  return 0xdead;
}

int main() {
  noaslr();

  log_level = LOG_INFO;
  hexdump_width = 16;

  log_info("pwning started");

  uint64_t kbase = kasld();
  set_kbase((void*)kbase);

  SYSCHK(dmmap(NULL, MAPDEF));

  void* ptr = malloc(0x40);
  REP2(i, 0x40) {
    ((uint8_t*)ptr)[i] = i;
  }

  free(ptr);
  hexdump(log_info, ptr, 0x40);

  // struct cpu_state state = cpu_now();
  // print_cpu_state(log_info, &state);

  vmmap(log_info);

  size_t x = kfunc_abs(call_me, 2, 1, 2);
  printf("ret = 0x%llx\n", x);

  interactive();
}