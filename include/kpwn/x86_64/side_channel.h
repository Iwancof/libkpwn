#ifndef _KPWN_x86_64_SIDE_CHANNEL_
#define _KPWN_x86_64_SIDE_CHANNEL_

#include <stddef.h>
#include <stdint.h>

enum side_channel_arch {
  unknown_arch = -1,
  x86_64_intel,
  x86_64_amd,
};

void cpuinfo_str(char* dst);
enum side_channel_arch get_arch();

uint64_t measure_prefetch(void* ptr);

size_t kasld_amd();
size_t kasld_amd_with_conf(size_t start, size_t end, size_t step, size_t num_get_kbase, size_t window_size);

size_t kasld_intel();
size_t kasld_intel_with_conf(size_t start, size_t end, size_t step, size_t num_confirm);

size_t kasld();

#endif  // _KPWN_ARCH_
