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

  char buf[0x4000];
  ssize_t n = read(maps, buf, sizeof(buf) - 1);
  close(maps);

  if (n <= 0)
    return;
  buf[n] = '\0';

  char *p = buf;
  while (*p) {
    char *nl = strchr(p, '\n');
    if (nl)
      *nl = '\0';
    log("[vmmap] %s", p);
    if (!nl)
      break;
    p = nl + 1;
  }
}

uint64_t virt2phys(void *addr) {
  static int fd = -1;

  if (fd < 0) {
    fd = SYSCHK(open("/proc/self/pagemap", O_RDONLY));
  }

  size_t page_offset = (uintptr_t)addr / PAGE_SIZE;
  size_t file_offset = page_offset * sizeof(uint64_t);

  SYSCHK(lseek(fd, file_offset, SEEK_SET));

  uint64_t entry;
  SYSCHK(read(fd, &entry, sizeof(entry)));

  if (!(entry & (1ULL << 63))) {
    log_warn("virt2phys: page not present for address %p", addr);
    return (uint64_t)-1;
  }

  uint64_t pfn = entry & ((1ULL << 55) - 1);
  uint64_t phy_addr = pfn * PAGE_SIZE + ((uintptr_t)addr & PAGE_MASK);

  return phy_addr;
}
