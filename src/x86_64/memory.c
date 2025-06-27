#include <kpwn/x86_64/memory.h>
#include <sys/mman.h>

const pgaddr pg_null = {.ptr = 0};

pgaddr from_pti(unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, unsigned long byte) {
  pgaddr ret = {
      .pgd = pgd,
      .pud = pud,
      .pmd = pmd,
      .pte = pte,
      .byte = byte,
  };

  return ret;
}

void* pti_mmap(logf_t log, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, size_t len,
               int prot, int flags, int fildes, off_t off) {
  void* dest = from_pti(pgd, pud, pmd, pte, 0).ptr;
  void* ret = mmap(dest, len, prot, flags, fildes, off);

  log("pti_mmap(%p, %#lx, %#x, %#x, %d, %#lx) = %p", dest, len, prot, flags, fildes, off, ret);

  return ret;
}

size_t phy_to_pte(size_t phys) {
  return phys | 0x67ULL | (1ULL << 63);
}
size_t pte_to_phy(size_t pte) {
  return pte & ~(1ULL << 63) & ~0xFFFULL;
}
