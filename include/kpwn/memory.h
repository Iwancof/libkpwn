#ifndef _KPWN_MEMORY_
#define _KPWN_MEMORY_

#include <kpwn/logger.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>

#define PAGE_SIZE 0x1000
#define PAGE_MASK (PAGE_SIZE - 1)

#define MAKE_MMAP_CONF(size, prot, flags, fildes, off)                         \
  (size), (prot), (flags), (fildes), (off)

#define MAPDEF                                                                 \
  MAKE_MMAP_CONF(PAGE_SIZE, PROT_READ | PROT_WRITE,                            \
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)

#define MAPDEF_SIZE(size)                                                      \
  MAKE_MMAP_CONF((size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,  \
                 -1, 0)

void vmmap(logf_t log);

#endif
