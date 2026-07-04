#ifndef _KPWN_TARGET_
#define _KPWN_TARGET_

#include <kpwn/kernel.h>
#include <stddef.h>
#include <stdint.h>

enum kpwn_src {
  KPWN_SRC_UNKNOWN = 0,
  KPWN_SRC_AUTODETECT,
  KPWN_SRC_KALLSYMS,
  KPWN_SRC_ASSUMED,
  KPWN_SRC_MANUAL,
};

struct kpwn_val64 {
  uint64_t v;
  enum kpwn_src src;
};

struct kpwn_val_int {
  int v;
  enum kpwn_src src;
};

struct kpwn_target {
  // --- identity ---
  uint64_t kbase;
  enum kpwn_src kbase_src;
  char release[128];
  char arch[16];
  size_t page_size;

  // --- mitigations (from kchecksec) ---
  struct kchecksec_t checksec;

  // --- struct sizes (64-bit defaults, overridable) ---
  struct kpwn_val64 msg_msg_hdr_size;
  struct kpwn_val64 msg_msgseg_hdr_size;
  struct kpwn_val64 pipe_buffer_size;
  struct kpwn_val64 skb_shared_info_size;
  struct kpwn_val64 user_key_hdr_size;
  struct kpwn_val64 cred_size;

  // --- key struct offsets ---
  struct kpwn_val64 pipe_buffer_ops_off;
  struct kpwn_val64 pipe_buffer_flags_off;
  struct kpwn_val64 pipe_buffer_page_off;
  struct kpwn_val64 msg_msg_m_ts_off;
  struct kpwn_val64 msg_msg_next_off;
  struct kpwn_val64 msg_msg_security_off;
  struct kpwn_val64 cred_uid_off;
  struct kpwn_val64 cred_euid_off;

  // --- symbols (offsets from kbase) ---
  struct kpwn_val64 sym_anon_pipe_buf_ops;
  struct kpwn_val64 sym_modprobe_path;
  struct kpwn_val64 sym_core_pattern;
  struct kpwn_val64 sym_commit_creds;
  struct kpwn_val64 sym_prepare_kernel_cred;
  struct kpwn_val64 sym_init_cred;

  // --- attack surfaces ---
  struct kpwn_val_int unpriv_userns;
  struct kpwn_val_int cap_net_admin;
  struct kpwn_val_int cap_net_raw;
  struct kpwn_val_int io_uring_disabled;
  struct kpwn_val_int unpriv_bpf_disabled;
  struct kpwn_val_int bpf_jit_enable;
  struct kpwn_val_int perf_event_paranoid;

  // --- allocator info ---
  struct kpwn_val_int random_kmalloc_caches;
  struct kpwn_val_int slab_buckets;
  struct kpwn_val_int hardened_usercopy;
};

#define KPWN_TARGET_DEFAULT 0
#define KPWN_TARGET_NO_KASLR (1u << 0)
#define KPWN_TARGET_SKIP_CHECKSEC (1u << 1)

int kpwn_target_init(struct kpwn_target *t, unsigned flags);
void kpwn_target_free(struct kpwn_target *t);

int kpwn_target_set_kbase(struct kpwn_target *t, uint64_t kbase,
                          enum kpwn_src src);
int kpwn_target_set_symbol(struct kpwn_target *t, const char *name,
                           uint64_t offset);

int kpwn_report(struct kpwn_target *t, unsigned flags);

size_t kpwn_msg_payload_for_cache(const struct kpwn_target *t,
                                  size_t cache_size);
size_t kpwn_key_payload_for_cache(const struct kpwn_target *t,
                                  size_t cache_size);

#endif
