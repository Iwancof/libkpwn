#define _GNU_SOURCE

#include <kpwn/kpwn.h>

int main(int argc, char *argv[]) {
  noaslr(argc, argv);
  log_level = LOG_DEBUG;

  kchecksec();

  uint64_t kb = kasld();
  set_kbase((void *)kb);

  log_info("exploit starts here");

  // --- your exploit code below ---
}
