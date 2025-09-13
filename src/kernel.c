#define _GNU_SOURCE

#include <kpwn/kernel.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <linux/capability.h>
#include <linux/io_uring.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <unistd.h>

// std headers not needed here after moving helpers to utils.c

// helpers moved to utils.c

struct kchecksec_t inspect() {
  struct kchecksec_t check;
  memset(&check, 0, sizeof(check));

  // == Kernel ==
  check.uname = popen_read("uname -a", 4096);
  if (check.uname)
    trim_trailing_newlines(check.uname);

  check.cmdline = slurp_file("/proc/cmdline", 8192);
  if (check.cmdline)
    trim_trailing_newlines(check.cmdline);

  check.has_config_gz = file_is_readable("/proc/config.gz") ? 1 : 0;

  // == Leak knobs ==
  check.kptr_restrict = slurp_int_file("/proc/sys/kernel/kptr_restrict", -1);
  check.dmesg_restrict = slurp_int_file("/proc/sys/kernel/dmesg_restrict", -1);

  // == Mitigations ==
  if (check.cmdline) {
    check.kaslr_on = strstr(check.cmdline, "nokaslr") ? 0 : 1;
  } else {
    check.kaslr_on = -1;
  }
  char *cpuinfo = slurp_file("/proc/cpuinfo", 1 << 16);
  check.cpu_smep = contains_token_case_insensitive(cpuinfo, "smep") ? 1 : 0;
  check.cpu_smap = contains_token_case_insensitive(cpuinfo, "smap") ? 1 : 0;
  if (cpuinfo)
    free(cpuinfo);
  check.meltdown =
      slurp_file("/sys/devices/system/cpu/vulnerabilities/meltdown", 4096);
  if (check.meltdown)
    trim_trailing_newlines(check.meltdown);

  // == Surfaces ==
  check.unpriv_userfaultfd =
      slurp_int_file("/proc/sys/vm/unprivileged_userfaultfd", -1);
  if (file_exists("/dev/userfaultfd")) {
    char *ls = popen_read("ls -l /dev/userfaultfd", 1024);
    if (ls) {
      char *first = read_first_n_lines(ls, 1);
      free(ls);
      if (first) {
        trim_trailing_newlines(first);
        check.dev_userfaultfd_ls = first;
      } else {
        check.dev_userfaultfd_ls =
            str_dup_or_null("/dev/userfaultfd: (ls failed)");
      }
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

  // Try file first then sysctl for unprivileged_userns_clone
  int userns_clone =
      slurp_int_file("/proc/sys/kernel/unprivileged_userns_clone", -9999);
  if (userns_clone == -9999) {
    char *out = popen_read(
        "sysctl -n kernel.unprivileged_userns_clone 2>/dev/null", 256);
    if (out) {
      trim_trailing_newlines(out);
      char *end = NULL;
      long v = strtol(out, &end, 10);
      if (end != out)
        check.unpriv_userns_clone = (int)v;
      else
        check.unpriv_userns_clone = -1;
      free(out);
    } else {
      check.unpriv_userns_clone = -1;
    }
  } else {
    check.unpriv_userns_clone = userns_clone;
  }

  // == Sandbox/Privs ==
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

  log_info("== Kernel ==");
  if (check.uname)
    log_info("%s", check.uname);
  log_info("cmdline: %s", check.cmdline ? check.cmdline : "N/A");
  log_info("config.gz: %s", check.has_config_gz ? "yes" : "no");

  log_info("== Leak knobs ==");
  if (check.kptr_restrict == -1 && check.dmesg_restrict == -1) {
    log_info("kptr_restrict=N/A  dmesg_restrict=N/A");
  } else {
    const char *kptrs = (check.kptr_restrict == -1) ? "N/A" : (char[]){0};
    const char *dmesgs = (check.dmesg_restrict == -1) ? "N/A" : (char[]){0};
    // Print integers or N/A
    if (check.kptr_restrict == -1 && check.dmesg_restrict != -1) {
      log_info("kptr_restrict=N/A  dmesg_restrict=%d", check.dmesg_restrict);
    } else if (check.kptr_restrict != -1 && check.dmesg_restrict == -1) {
      log_info("kptr_restrict=%d  dmesg_restrict=N/A", check.kptr_restrict);
    } else if (check.kptr_restrict != -1 && check.dmesg_restrict != -1) {
      log_info("kptr_restrict=%d  dmesg_restrict=%d", check.kptr_restrict,
               check.dmesg_restrict);
    }
    (void)kptrs;
    (void)dmesgs; // silence unused in some branches
  }

  log_info("== Mitigations ==");
  if (check.kaslr_on == 1)
    log_info("KASLR=on");
  else if (check.kaslr_on == 0)
    log_info("KASLR=off");
  else
    log_info("KASLR=unknown");

  log_info("%s", check.cpu_smep ? "CPU:SMEP" : "CPU:SMEP-");
  log_info("%s", check.cpu_smap ? "CPU:SMAP" : "CPU:SMAP-");
  if (check.meltdown)
    log_info("Meltdown: %s", check.meltdown);

  log_info("== Surfaces ==");
  if (check.unpriv_userfaultfd == -1)
    log_info("userfaultfd sysctl: N/A");
  else
    log_info("userfaultfd sysctl: %d", check.unpriv_userfaultfd);
  if (check.dev_userfaultfd_ls)
    log_info("%s", check.dev_userfaultfd_ls);
  if (check.unpriv_bpf_disabled == -1)
    log_info("unprivileged_bpf_disabled: N/A");
  else
    log_info("unprivileged_bpf_disabled: %d", check.unpriv_bpf_disabled);
  if (check.bpf_jit_enable == -1)
    log_info("bpf_jit_enable: N/A");
  else
    log_info("bpf_jit_enable: %d", check.bpf_jit_enable);
  if (check.io_uring_disabled == -1)
    log_info("io_uring_disabled: N/A");
  else
    log_info("io_uring_disabled: %d", check.io_uring_disabled);
  if (check.unpriv_userns_clone == -1)
    log_info("unprivileged_userns_clone: N/A");
  else
    log_info("unprivileged_userns_clone: %d", check.unpriv_userns_clone);

  log_info("== Sandbox/Privs ==");
  if (check.seccomp_status)
    log_info("%s", check.seccomp_status);
  log_info("LSM: %s", check.lsm ? check.lsm : "N/A");
  if (check.capsh_first_lines)
    log_info("%s", check.capsh_first_lines);

  return check;
}

int uring_setup(unsigned entries, struct io_uring_params *p) {
  memset(p, 0, sizeof(*p));
  // ここではフラグ無しでOK。必要なら p->flags に IORING_SETUP_* を設定
  return (int)syscall(SYS_io_uring_setup, entries, p);
}

int uring_register(int ring_fd, unsigned opcode, const void *arg,
                   unsigned nr_args) {
  return (int)syscall(SYS_io_uring_register, ring_fd, opcode, arg, nr_args);
}

int alloc_n_creds(int nr_creds) {
  ASSERT_MSG(nr_creds > 0, "nr_creds must be greater than 0");

  static struct io_uring_params params;
  int ring_fd = SYSCHK_BAIL(uring_setup(1, &params));

  struct __user_cap_header_struct cap_header = {
      .version = _LINUX_CAPABILITY_VERSION_3,
      .pid = 0,
  };
  struct __user_cap_data_struct cap_data[2];

  SYSCHK_BAIL(syscall(__NR_capget, &cap_header, &cap_data));

  REP(nr_creds) {
    SYSCHK_BAIL(syscall(__NR_capset, &cap_header, &cap_data));
    SYSCHK_BAIL(uring_register(ring_fd, IORING_REGISTER_PERSONALITY, NULL, 0));
  }

  log_debug("allocated %d creds = %lx bytes", nr_creds,
            (unsigned long)nr_creds * 0xc0);

  return ring_fd;
}

void *kbase = NULL;

void set_kbase(void *addr) {
  log_success("setting kernel base address to %p", addr);
  kbase = addr;
}

size_t call_ptr(void *fptr, int argc, va_list list) {
  size_t args[argc];
  for (int i = 0; i < argc; i++)
    args[i] = va_arg(list, size_t);

  switch (argc) {
  case 0: {
    size_t (*ft)() = fptr;
    return ft();
  }
  case 1: {
    size_t (*ft)(size_t a) = fptr;
    return ft(args[0]);
  }
  case 2: {
    size_t (*ft)(size_t a, size_t b) = fptr;
    return ft(args[0], args[1]);
  }
  case 3: {
    size_t (*ft)(size_t a, size_t b, size_t c) = fptr;
    return ft(args[0], args[1], args[2]);
  }
  case 4: {
    size_t (*ft)(size_t a, size_t b, size_t c, size_t d) = fptr;
    return ft(args[0], args[1], args[2], args[3]);
  }
  case 5: {
    size_t (*ft)(size_t a, size_t b, size_t c, size_t d, size_t e) = fptr;
    return ft(args[0], args[1], args[2], args[3], args[4]);
  }
  case 6: {
    size_t (*ft)(size_t a, size_t b, size_t c, size_t d, size_t e, size_t f) =
        fptr;
    return ft(args[0], args[1], args[2], args[3], args[4], args[5]);
  }
  default: {
    log_error("call_ptr: unsupported argument count %d", argc);
    ASSERT(0);
    return 0; // unreachable
  }
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
  va_list list;
  va_start(list, argc);

  if (kbase == NULL) {
    log_error("kbase is not initialized. you probably forget "
              "initialize or use kfunc_abs instead of kfunc_off");
    ASSERT(0);
  }

  size_t ret = call_ptr(kbase + (size_t)fptr, argc, list);
  va_end(list);

  return ret;
}
