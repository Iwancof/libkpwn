#ifndef _KPWN_MEMORY_
#define _KPWN_MEMORY_

#define PAGE_SIZE 0x1000
#define PAGE_MASK (PAGE_SIZE - 1)

#include <kpwn/logger.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>

#define MAKE_MMAP_CONF(size, prot, flags, fildes, off) \
  (size), (prot), (flags), (fildes), (off)

#define MAPDEF                                      \
  MAKE_MMAP_CONF(PAGE_SIZE, PROT_READ | PROT_WRITE, \
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)

#define MAPDEF_SIZE(size)                        \
  MAKE_MMAP_CONF((size), PROT_READ | PROT_WRITE, \
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)

typedef union {
  struct {
    unsigned long byte : 12;
    unsigned long pte : 9;
    unsigned long pmd : 9;
    unsigned long pud : 9;
    unsigned long pgd : 9;
    unsigned long padding : 16;
  };
  void* ptr;
} pgaddr;

extern const pgaddr pg_null;

pgaddr from_pti(unsigned long pgd, unsigned long pud,
                unsigned long pmd, unsigned long pte,
                unsigned long byte);
void* pti_mmap(logf_t log, unsigned long pgd,
               unsigned long pud, unsigned long pmd,
               unsigned long pte, size_t len, int prot,
               int flags, int fildes, off_t off);

size_t phy_to_pte(size_t phys);
size_t pte_to_phy(size_t pte);

void vmmap(logf_t log);

#endif