#include <fcntl.h>
#include <kpwn/logger.h>
#include <kpwn/memory.h>
#include <kpwn/utils.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

void vmmap(logf_t log) {
  int maps = SYSCHK(open("/proc/self/maps", O_RDONLY));

  char line[0x100];
  for (int pos = 0; read(maps, &line[pos], 1); pos++) {
    if (line[pos] == '\n') {
      log("[vmmap] %.*s", pos, line);
      pos = -1;
    }
  }

  close(maps);
}
