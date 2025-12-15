#include <fcntl.h>
#include <kpwn/logger.h>
#include <kpwn/memory.h>
#include <kpwn/utils.h>
#include <stddef.h>
#include <stdint.h>
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

uint64_t virt2phys(void *addr) {
  static int fd = -1;

  if (fd == -1) {
    fd = SYSCHK(open("/proc/self/pagemap", O_RDONLY));
  }

  size_t page_offset = (uintptr_t)addr / PAGE_SIZE;
  size_t file_offset = page_offset * sizeof(uint64_t);

  SYSCHK(lseek(fd, file_offset, SEEK_SET));

  uint64_t entry;
  SYSCHK(read(fd, &entry, sizeof(entry)));

  // Check if page is present (bit 63)
  if (!(entry & (1ULL << 63))) {
    log_warn("virt2phys: page not present for address %p", addr);
    return 0;
  }

  // Extract PFN (bits 0-54)
  uint64_t pfn = entry & ((1ULL << 55) - 1);

  // Calculate physical address: PFN * PAGE_SIZE + page offset
  uint64_t phy_addr = pfn * PAGE_SIZE + ((uintptr_t)addr & PAGE_MASK);

  return phy_addr;
}