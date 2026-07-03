#ifndef _KPWN_SLOG_
#define _KPWN_SLOG_

// Syscall thin wrappers with logging.
//
// lg*: logged variant — outputs to a caller-provided logf_t.
// d*:  default variant — logs via log_debug and checks the return value.

#include <kpwn/logger.h>

void *lgmmap(logf_t log, void *addr, size_t len, int prot, int flags,
             int fildes, off_t off);
void *dmmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);

int lgmunmap(logf_t log, void *addr, size_t len);
int dmunmap(void *addr, size_t len);

void *lgmremap(logf_t log, void *addr, size_t old, size_t new, int flags);
void *dmremap(void *addr, size_t old, size_t new, int flags);

#endif
