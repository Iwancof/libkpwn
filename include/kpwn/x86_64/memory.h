#ifndef _KPWN_X86_64_MEMORY_
#define _KPWN_X86_64_MEMORY_

#include <kpwn/logger.h>
#include <stdint.h>

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

#endif
