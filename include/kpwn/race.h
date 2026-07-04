#ifndef _KPWN_RACE_
#define _KPWN_RACE_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

// --- Epoll watch pile (timerfd + epoll)
// ---------------------------------------

struct kpwn_epoll_pile {
  int timerfd;
  pid_t *children;
  size_t n_children;
  size_t total_watches;
};

int kpwn_epoll_pile_init(struct kpwn_epoll_pile *p, size_t n_procs,
                         size_t n_epolls, size_t n_dups);
int kpwn_epoll_pile_destroy(struct kpwn_epoll_pile *p);

// --- Usercopy stall (mprotect over zeropage VMA) -----------------------------

struct kpwn_usercopy_stall {
  void *base;
  size_t len;
};

int kpwn_usercopy_stall_init(struct kpwn_usercopy_stall *s, size_t len_bytes);
int kpwn_usercopy_stall_trigger(struct kpwn_usercopy_stall *s);
int kpwn_usercopy_stall_free(struct kpwn_usercopy_stall *s);

// --- Simple race runner ------------------------------------------------------

typedef int (*kpwn_race_fn)(void *arg, uint64_t iteration);

struct kpwn_race_opts {
  int cpu;
  uint64_t max_iter;
  volatile int *stop;
};

int kpwn_race_run(size_t n_threads, kpwn_race_fn fn, void *arg,
                  const struct kpwn_race_opts *opts);

#endif
