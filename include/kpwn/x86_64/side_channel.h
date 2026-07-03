#ifndef _KPWN_x86_64_SIDE_CHANNEL_
#define _KPWN_x86_64_SIDE_CHANNEL_

#include <stddef.h>
#include <stdint.h>

enum side_channel_arch {
  unknown_arch = -1,
  x86_64_intel,
  x86_64_amd,
};

void cpuinfo_str(char *dst);
enum side_channel_arch get_arch();

uint64_t measure_prefetch(void *ptr);

// Multi-strategy KASLR bypass. Tries /proc/kallsyms first, then prefetch
// side-channel. Returns the kernel text base or 0 on failure.
size_t kasld(void);

// Individual strategies (called by kasld)
size_t kasld_proc_kallsyms(void);
size_t kasld_prefetch(void);

// Legacy API — all delegate to kasld_prefetch()
size_t kasld_amd(void);
size_t kasld_amd_with_conf(size_t start, size_t end, size_t step,
                           size_t num_confirm, size_t window_size);
size_t kasld_intel(void);
size_t kasld_intel_with_conf(size_t start, size_t end, size_t step,
                             size_t num_confirm);

#endif
