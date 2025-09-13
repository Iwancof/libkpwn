#define _GNU_SOURCE

#include <kpwn/prelude.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

size_t call_me(int x, int y) {
  printf("x = %d, y = %d\n", x, y);

  return 0xdead;
}

int main(int argc, char *argv[]) {
  noaslr(argc, argv);

  log_level = LOG_DEBUG;
  hexdump_width = 16;

  kchecksec();

  log_info("pwning started");

  uint64_t kbase = kasld();
  set_kbase((void *)kbase);

  dmmap(NULL, MAPDEF);

  void *ptr = malloc(0x40);
  malloc(0x0);

  REP(i, 0x40) { ((uint8_t *)ptr)[i] = i; }

  free(ptr);
  hexdump(log_info, ptr, 0x40);

  // struct cpu_state state = cpu_now();
  // print_cpu_state(log_info, &state);

  vmmap(log_info);

  size_t x = kfunc_abs(call_me, 2, 1, 2);
  printf("ret = 0x%lx\n", x);

  char *payload = calloc(1, 0x20);
  up64(0xdeadbeefcafebabe, &payload[0]);
  up64(0x1122334444332211, &payload[8]);
  up32(0xffffffff, &payload[16]);
  up32(0xeeeeeeee, &payload[24]);

  hexdump(log_info, payload, 0x20);

  alloc_n_creds(0x100);

  interactive();
}
