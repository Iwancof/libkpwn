#include "../src/x86_64/prefetch_scan.h"
#include "ktest.h"

static void profile_fill(uint64_t *times, uint64_t baseline) {
  for (size_t i = 0; i < PREFETCH_SLOTS; i++)
    times[i] = baseline;
}

static void profile_band(uint64_t *times, size_t start, size_t width,
                         uint64_t value) {
  for (size_t i = start; i < start + width; i++)
    times[i] = value;
}

static size_t slot_address(size_t slot) {
  return PREFETCH_TEXT_MIN + slot * PREFETCH_ALIGN;
}

KTEST(prefetch, amd_strong_and_weak_leading_slots) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 20, 20, 1800);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(20));
  times[20] = 1300;
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(20));
  times[21] = 1300;
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(20));
}

KTEST(prefetch, intel_strong_and_weak_leading_slots) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 20, 20, 900);
  KT_ASSERT_EQ(prefetch_find_base_intel(times), slot_address(20));
  times[20] = 970;
  KT_ASSERT_EQ(prefetch_find_base_intel(times), slot_address(20));
  times[21] = 970;
  KT_ASSERT_EQ(prefetch_find_base_intel(times), slot_address(20));
}

KTEST(prefetch, intel_ambiguous_leading_slot_reports_failure) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 20, 20, 900);
  times[20] = 980;
  KT_ASSERT_EQ(prefetch_find_base_intel(times), 0);
  times[21] = 980;
  KT_ASSERT_EQ(prefetch_find_base_intel(times), 0);
}

KTEST(prefetch, amd_ambiguous_leading_slot_reports_failure) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 20, 20, 1800);
  times[20] = 1100;
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), 0);
  times[21] = 1100;
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), 0);
}

KTEST(prefetch, first_slot_is_not_skipped) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 0, 20, 1800);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(0));
  profile_band(times, 0, 20, 900);
  KT_ASSERT_EQ(prefetch_find_base_intel(times), slot_address(0));
  profile_band(times, 0, 20, 12000);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 1), slot_address(0));
}

KTEST(prefetch, final_complete_window) {
  uint64_t times[PREFETCH_SLOTS];
  size_t start = PREFETCH_SLOTS - PREFETCH_CONFIRM_M;
  profile_fill(times, 1000);
  profile_band(times, start, PREFETCH_CONFIRM_M, 1800);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(start));
  profile_band(times, start, PREFETCH_CONFIRM_M, 900);
  KT_ASSERT_EQ(prefetch_find_base_intel(times), slot_address(start));
  profile_band(times, start, PREFETCH_CONFIRM_M, 12000);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 1), slot_address(start));
}

KTEST(prefetch, incomplete_tail_is_not_confirmed) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, PREFETCH_SLOTS - 4, 4, 1800);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), 0);
  profile_band(times, PREFETCH_SLOTS - 4, 4, 900);
  KT_ASSERT_EQ(prefetch_find_base_intel(times), 0);
  profile_band(times, PREFETCH_SLOTS - 4, 4, 12000);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 1), 0);
}

KTEST(prefetch, flat_profiles_report_failure) {
  uint64_t times[PREFETCH_SLOTS];
  const uint64_t levels[] = {0, 1, 19, 1000, UINT64_MAX};
  for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
    profile_fill(times, levels[i]);
    KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), 0);
    KT_ASSERT_EQ(prefetch_find_base_amd(times, 1), 0);
    KT_ASSERT_EQ(prefetch_find_base_intel(times), 0);
  }
}

KTEST(prefetch, isolated_outliers_report_failure) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  times[60] = 800;
  KT_ASSERT_EQ(prefetch_find_base_intel(times), 0);
  times[60] = 12000;
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), 0);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 1), 0);
}

KTEST(prefetch, isolated_outliers_do_not_hide_a_later_image) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  times[2] = 12000;
  profile_band(times, 20, 20, 1800);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(20));
  times[2] = 500;
  profile_band(times, 20, 20, 900);
  KT_ASSERT_EQ(prefetch_find_base_intel(times), slot_address(20));
}

KTEST(prefetch, amd_low_amplitude_plateau) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 20, 20, 1030);
  times[20] = times[21] = 1010;
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(20));
}

KTEST(prefetch, amd_narrow_boundary_bands_report_failure) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  for (size_t start = 1; start + 7 < PREFETCH_SLOTS; start += 128)
    profile_band(times, start, 7, 1200);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), 0);
}

KTEST(prefetch, amd_low_amplitude_plateau_after_boundary_band) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 1, 7, 1200);
  profile_band(times, 20, 20, 1030);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(20));
}

KTEST(prefetch, amd_noise_floor_rejects_weak_plateau) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 20, 20, 1010);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), 0);
}

KTEST(prefetch, amd_dispersion_adjusts_cutoff) {
  uint64_t times[PREFETCH_SLOTS];
  for (size_t i = 0; i < PREFETCH_SLOTS; i++)
    times[i] = 990 + i % 21;
  profile_band(times, 20, 20, 1030);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(20));
  profile_band(times, 20, 20, 1015);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), 0);
}

KTEST(prefetch, amd_batched_uses_stronger_cutoff) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 20, 20, 1800);
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 1), 0);
  profile_band(times, 20, 20, 12000);
  times[19] = 1800;
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 1), slot_address(20));
}

KTEST(prefetch, confirmation_tolerates_holes) {
  uint64_t times[PREFETCH_SLOTS];
  profile_fill(times, 1000);
  profile_band(times, 20, 8, 1800);
  times[22] = times[24] = times[26] = 1000;
  KT_ASSERT_EQ(prefetch_find_base_amd(times, 0), slot_address(20));
  profile_band(times, 20, 8, 900);
  times[22] = times[24] = times[26] = 1000;
  KT_ASSERT_EQ(prefetch_find_base_intel(times), slot_address(20));
}

KTEST(prefetch, kpti_exact_token) {
  char cpuinfo[] = "processor : 0\nflags\t: fpu pti rdtscp\n";
  KT_ASSERT_EQ(prefetch_kpti_flag(cpuinfo), 1);
}

KTEST(prefetch, kpti_does_not_match_substrings) {
  char cpuinfo[] = "model name : pti\n"
                   "flags : fpu_exception nopti pti_extra xpti rdtscp\n";
  KT_ASSERT_EQ(prefetch_kpti_flag(cpuinfo), 0);
}

KTEST(prefetch, kpti_whitespace_and_final_line) {
  char cpuinfo[] = " \tflags \t: fpu\tpti\r";
  KT_ASSERT_EQ(prefetch_kpti_flag(cpuinfo), 1);
}

KTEST(prefetch, kpti_later_processor) {
  char cpuinfo[] = "processor : 0\nflags : fpu rdtscp\n\n"
                   "processor : 1\nflags : fpu pti rdtscp\n";
  KT_ASSERT_EQ(prefetch_kpti_flag(cpuinfo), 1);
}

KTEST(prefetch, kpti_missing_flags_is_unknown) {
  char empty[] = "";
  char malformed[] = "model name : pti\nflags_extra : pti\nflags pti\n";
  KT_ASSERT_EQ(prefetch_kpti_flag(NULL), -1);
  KT_ASSERT_EQ(prefetch_kpti_flag(empty), -1);
  KT_ASSERT_EQ(prefetch_kpti_flag(malformed), -1);
}
