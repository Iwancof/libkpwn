#ifndef _KPWN_SPRAY_EXTRA_
#define _KPWN_SPRAY_EXTRA_

#include <stddef.h>
#include <stdint.h>

// tty_struct via /dev/ptmx (~0x2c0 bytes, dedicated cache)
int kpwn_spray_tty(int *fds, size_t n);
int kpwn_free_tty(int *fds, size_t n);

// timerfd_ctx (~0x1e8 bytes, kmalloc-512)
int kpwn_spray_timerfd(int *fds, size_t n);
int kpwn_free_timerfd(int *fds, size_t n);

// eventfd_ctx (~0x40 bytes, kmalloc-cg-64)
int kpwn_spray_eventfd(int *fds, size_t n, unsigned int initval);
int kpwn_free_eventfd(int *fds, size_t n);

// seq_file via /proc/ (~0x58 bytes, kmalloc-cg-128)
int kpwn_spray_seqfile(int *fds, size_t n, const char *proc_path);
int kpwn_free_seqfile(int *fds, size_t n);

// Namespace helpers
int kpwn_userns_enter(void);
int kpwn_netns_enter(void);
int kpwn_ns_setup_net_admin(void);

#endif
