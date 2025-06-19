#ifndef _KPWN_SLOG_
#define _KPWN_SLOG_

// system call thin wrapper with logs

#include <kpwn/logger.h>

// d.*: 'd'efault system call wrapper. output logs and check
// result.

// lg.* `log` system call wrapper. output specified log.
// without result.

void* lgmmap(logf_t log, void* addr, size_t len, int prot,
             int flags, int fildes, off_t off);
void* dmmap(void* addr, size_t len, int prot, int flags,
            int fildes, off_t off);

int lgmunmap(logf_t log, void* addr, size_t len);
int dunmap(void* addr, size_t len);

void* lgmremap(logf_t log, void* addr, size_t old,
               size_t new, int flags);
void* dremap(void* addr, size_t old, size_t new, int flags);

#endif
