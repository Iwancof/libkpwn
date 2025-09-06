#ifndef _KPWN_KERNEL_
#define _KPWN_KERNEL_

#include <stddef.h>

struct kchecksec_t {
  // == Kernel ==
  char* uname;                 // `uname -a` の結果
  char* cmdline;               // /proc/cmdline
  int has_config_gz;           // /proc/config.gz の可読性 (1=yes,0=no)

  // == Leak knobs ==
  int kptr_restrict;           // /proc/sys/kernel/kptr_restrict (or -1 if N/A)
  int dmesg_restrict;          // /proc/sys/kernel/dmesg_restrict (or -1 if N/A)

  // == Mitigations ==
  int kaslr_on;                // 1 if KASLR=on, 0 if off, -1 unknown
  int cpu_smep;                // 1 if SMEP supported, 0 if not
  int cpu_smap;                // 1 if SMAP supported, 0 if not
  char* meltdown;              // meltdown status string or NULL

  // == Surfaces ==
  int unpriv_userfaultfd;      // /proc/sys/vm/unprivileged_userfaultfd (or -1 if N/A)
  char* dev_userfaultfd_ls;    // `ls -l /dev/userfaultfd` 結果 or "none"
  int unpriv_bpf_disabled;     // /proc/sys/kernel/unprivileged_bpf_disabled (or -1 if N/A)
  int bpf_jit_enable;          // /proc/sys/net/core/bpf_jit_enable (or -1 if N/A)
  int io_uring_disabled;       // /proc/sys/kernel/io_uring_disabled (or -1 if N/A)
  int unpriv_userns_clone;     // sysctl kernel.unprivileged_userns_clone (or -1 if N/A)

  // == Sandbox/Privs ==
  char* seccomp_status;        // from /proc/self/status line starting with "Seccomp:"
  char* lsm;                   // /sys/kernel/security/lsm (or NULL)
  char* capsh_first_lines;     // first ~10 lines of `capsh --print` or NULL
};

struct kchecksec_t inspect();
struct kchecksec_t kchecksec();

extern void* kbase;

void set_kbase(void* addr);

size_t kfunc_abs(void* fptr, int argc, ...);
size_t kfunc_off(void* fptr, int argc, ...);

#endif