#define _GNU_SOURCE

#include <kpwn/logger.h>
#include <kpwn/slog.h>
#include <sys/mman.h>

void *lgmmap(logf_t log, void *addr, size_t len, int prot,
             int flags, int fildes, off_t off) {
  void *ret = mmap(addr, len, prot, flags, fildes, off);
  log("mmap(%p, 0x%llx, %x, %x, %d, 0x%llx) = %p", addr,
      len, prot, flags, fildes, off, ret);

  return ret;
}

void *dmmap(void *addr, size_t len, int prot, int flags,
            int fildes, off_t off) {
  return lgmmap(log_info, addr, len, prot, flags, fildes,
                off);
}

int lgmunmap(logf_t log, void *addr, size_t len) {
  int ret = munmap(addr, len);
  log("munmap(%p, 0x%llx) = %d", addr, len, ret);
  return ret;
}

int dunmap(void *addr, size_t len) {
  return lgmunmap(log_info, addr, len);
}

void *lgmremap(logf_t log, void *addr, size_t old,
               size_t new, int flags) {
  void *ret = mremap(addr, old, new, flags);
  log("mremap(%p, 0x%llx, 0x%llx, %x) = %p", addr, old, new,
      flags, ret);
  return ret;
}

void *dremap(void *addr, size_t old, size_t new,
             int flags) {
  return lgmremap(log_info, addr, old, new, flags);
}