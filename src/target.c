#define _GNU_SOURCE

#include <kpwn/logger.h>
#include <kpwn/target.h>
#include <kpwn/utils.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#define V64(val, s)                                                            \
  (struct kpwn_val64) { (val), (s) }
#define VINT(val, s)                                                           \
  (struct kpwn_val_int) { (val), (s) }

static void detect_identity(struct kpwn_target *t) {
  struct utsname u;
  if (uname(&u) == 0) {
    strncpy(t->release, u.release, sizeof(t->release) - 1);
    strncpy(t->arch, u.machine, sizeof(t->arch) - 1);
  }
  t->page_size = (size_t)sysconf(_SC_PAGESIZE);
}

static void set_defaults_x86_64(struct kpwn_target *t) {
  t->msg_msg_hdr_size = V64(0x30, KPWN_SRC_ASSUMED);
  t->msg_msgseg_hdr_size = V64(0x08, KPWN_SRC_ASSUMED);
  t->pipe_buffer_size = V64(0x28, KPWN_SRC_ASSUMED);
  t->skb_shared_info_size = V64(320, KPWN_SRC_ASSUMED);
  t->user_key_hdr_size = V64(0x18, KPWN_SRC_ASSUMED);
  t->cred_size = V64(0xa8, KPWN_SRC_ASSUMED);

  t->pipe_buffer_ops_off = V64(0x10, KPWN_SRC_ASSUMED);
  t->pipe_buffer_flags_off = V64(0x18, KPWN_SRC_ASSUMED);
  t->pipe_buffer_page_off = V64(0x00, KPWN_SRC_ASSUMED);
  t->msg_msg_m_ts_off = V64(0x18, KPWN_SRC_ASSUMED);
  t->msg_msg_next_off = V64(0x20, KPWN_SRC_ASSUMED);
  t->msg_msg_security_off = V64(0x28, KPWN_SRC_ASSUMED);
  t->cred_uid_off = V64(0x04, KPWN_SRC_ASSUMED);
  t->cred_euid_off = V64(0x14, KPWN_SRC_ASSUMED);
}

static void detect_surfaces(struct kpwn_target *t) {
  t->unpriv_userns =
      VINT(slurp_int_file("/proc/sys/kernel/unprivileged_userns_clone", -1),
           KPWN_SRC_AUTODETECT);
  t->io_uring_disabled =
      VINT(slurp_int_file("/proc/sys/kernel/io_uring_disabled", -1),
           KPWN_SRC_AUTODETECT);
  t->unpriv_bpf_disabled =
      VINT(slurp_int_file("/proc/sys/kernel/unprivileged_bpf_disabled", -1),
           KPWN_SRC_AUTODETECT);
  t->bpf_jit_enable =
      VINT(slurp_int_file("/proc/sys/net/core/bpf_jit_enable", -1),
           KPWN_SRC_AUTODETECT);
  t->perf_event_paranoid =
      VINT(slurp_int_file("/proc/sys/kernel/perf_event_paranoid", -1),
           KPWN_SRC_AUTODETECT);

  t->cap_net_admin = VINT(-1, KPWN_SRC_UNKNOWN);
  t->cap_net_raw = VINT(-1, KPWN_SRC_UNKNOWN);

  t->random_kmalloc_caches = VINT(-1, KPWN_SRC_UNKNOWN);
  t->slab_buckets = VINT(-1, KPWN_SRC_UNKNOWN);
  t->hardened_usercopy = VINT(-1, KPWN_SRC_UNKNOWN);

  char *config = slurp_file("/proc/config.gz", 0);
  if (config) {
    free(config);
    // /proc/config.gz exists but we'd need zlib to parse it
    // Mark as unknown for now
  }
}

static const char *src_str(enum kpwn_src s) {
  switch (s) {
  case KPWN_SRC_AUTODETECT:
    return "auto";
  case KPWN_SRC_KALLSYMS:
    return "kallsyms";
  case KPWN_SRC_ASSUMED:
    return "assumed";
  case KPWN_SRC_MANUAL:
    return "manual";
  default:
    return "unknown";
  }
}

int kpwn_target_init(struct kpwn_target *t, unsigned flags) {
  memset(t, 0, sizeof(*t));

  detect_identity(t);

  if (strcmp(t->arch, "x86_64") == 0)
    set_defaults_x86_64(t);

  if (!(flags & KPWN_TARGET_SKIP_CHECKSEC))
    t->checksec = kchecksec();

  detect_surfaces(t);

  if (!(flags & KPWN_TARGET_NO_KASLR)) {
    extern size_t kasld(void);
    uint64_t kb = kasld();
    if (kb) {
      t->kbase = kb;
      t->kbase_src = KPWN_SRC_AUTODETECT;
    }
  }

  log_info("[kpwn:target] init release=%s arch=%s page_size=%zu kbase=0x%lx",
           t->release, t->arch, t->page_size, t->kbase);
  return 0;
}

void kpwn_target_free(struct kpwn_target *t) {
  // kchecksec_t has heap-allocated strings
  free(t->checksec.uname);
  free(t->checksec.cmdline);
  free(t->checksec.meltdown);
  free(t->checksec.dev_userfaultfd_ls);
  free(t->checksec.seccomp_status);
  free(t->checksec.lsm);
  free(t->checksec.capsh_first_lines);
  memset(t, 0, sizeof(*t));
}

int kpwn_target_set_kbase(struct kpwn_target *t, uint64_t kbase,
                          enum kpwn_src src) {
  t->kbase = kbase;
  t->kbase_src = src;
  log_success("[kpwn:target] kbase=0x%lx src=%s", kbase, src_str(src));
  return 0;
}

int kpwn_target_set_symbol(struct kpwn_target *t, const char *name,
                           uint64_t offset) {
  struct {
    const char *n;
    struct kpwn_val64 *f;
  } syms[] = {
      {"anon_pipe_buf_ops", &t->sym_anon_pipe_buf_ops},
      {"modprobe_path", &t->sym_modprobe_path},
      {"core_pattern", &t->sym_core_pattern},
      {"commit_creds", &t->sym_commit_creds},
      {"prepare_kernel_cred", &t->sym_prepare_kernel_cred},
      {"init_cred", &t->sym_init_cred},
  };
  for (size_t i = 0; i < sizeof(syms) / sizeof(syms[0]); i++) {
    if (strcmp(syms[i].n, name) == 0) {
      *syms[i].f = V64(offset, KPWN_SRC_MANUAL);
      log_debug("[kpwn:target] sym %s=0x%lx", name, offset);
      return 0;
    }
  }
  log_warn("[kpwn:target] unknown symbol: %s", name);
  return -1;
}

#define REPORT_V64(label, fld)                                                 \
  if ((fld).src != KPWN_SRC_UNKNOWN)                                           \
  log_info("[kpwn:report] %s=0x%lx src=%s", label, (fld).v, src_str((fld).src))

#define REPORT_VINT(label, fld)                                                \
  if ((fld).src != KPWN_SRC_UNKNOWN)                                           \
  log_info("[kpwn:report] %s=%d src=%s", label, (fld).v, src_str((fld).src))

int kpwn_report(struct kpwn_target *t, unsigned flags) {
  (void)flags;

  log_info("[kpwn:report] release=%s arch=%s page_size=%zu", t->release,
           t->arch, t->page_size);
  log_info("[kpwn:report] kbase=0x%lx src=%s", t->kbase, src_str(t->kbase_src));
  log_info("[kpwn:report] uid=%d euid=%d", getuid(), geteuid());

  if (t->checksec.kaslr_on >= 0)
    log_info("[kpwn:report] kaslr=%s smep=%d smap=%d",
             t->checksec.kaslr_on ? "on" : "off", t->checksec.cpu_smep,
             t->checksec.cpu_smap);

  REPORT_VINT("userns", t->unpriv_userns);
  REPORT_VINT("io_uring_disabled", t->io_uring_disabled);
  REPORT_VINT("bpf_disabled", t->unpriv_bpf_disabled);
  REPORT_VINT("bpf_jit", t->bpf_jit_enable);
  REPORT_VINT("perf_paranoid", t->perf_event_paranoid);

  REPORT_V64("msg_msg_hdr", t->msg_msg_hdr_size);
  REPORT_V64("pipe_buffer_size", t->pipe_buffer_size);
  REPORT_V64("pipe_buffer.ops_off", t->pipe_buffer_ops_off);
  REPORT_V64("cred_size", t->cred_size);

  return 0;
}

size_t kpwn_msg_payload_for_cache(const struct kpwn_target *t,
                                  size_t cache_size) {
  return cache_size - t->msg_msg_hdr_size.v;
}

size_t kpwn_key_payload_for_cache(const struct kpwn_target *t,
                                  size_t cache_size) {
  return cache_size - t->user_key_hdr_size.v;
}
