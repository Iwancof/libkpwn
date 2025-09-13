#define _GNU_SOURCE

#include <kpwn/prelude.h>

int main(int argc, char *argv[]) {
  noaslr(argc, argv);
  log_level = LOG_DEBUG;
  hexdump_width = 16;

  init_billy();

  log_info("pwning started");

  interactive();
}
