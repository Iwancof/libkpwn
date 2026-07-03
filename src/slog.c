#define _GNU_SOURCE

#include <kpwn/logger.h>
#include <kpwn/slog.h>
#include <kpwn/utils.h>
#include <sys/mman.h>

void *lgmmap(logf_t log, void *addr, size_t len, int prot, int flags,
             int fildes, off_t off) {
  void *ret = mmap(addr, len, prot, flags, fildes, off);
  log("mmap(%p, %#lx, %x, %x, %d, %#lx) = %p", addr, len, prot, flags, fildes,
      off, ret);
  return ret;
}

void *dmmap(void *addr, size_t len, int prot, int flags, int fildes,
            off_t off) {
  return PTRCHK(lgmmap(log_debug, addr, len, prot, flags, fildes, off));
}

int lgmunmap(logf_t log, void *addr, size_t len) {
  int ret = munmap(addr, len);
  log("munmap(%p, %#lx) = %d", addr, len, ret);
  return ret;
}

int dmunmap(void *addr, size_t len) {
  return SYSCHK(lgmunmap(log_debug, addr, len));
}

void *lgmremap(logf_t log, void *addr, size_t old, size_t new, int flags) {
  void *ret = mremap(addr, old, new, flags);
  log("mremap(%p, %#lx, %#lx, %x) = %p", addr, old, new, flags, ret);
  return ret;
}

void *dmremap(void *addr, size_t old, size_t new, int flags) {
  return PTRCHK(lgmremap(log_debug, addr, old, new, flags));
}
