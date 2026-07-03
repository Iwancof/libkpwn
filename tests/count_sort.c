#include "ktest.h"
#include <kpwn/utils.h>

KTEST(count_sort, majority_element) {
  uint64_t data[] = {42, 42, 42, 7, 7};
  struct count_sort_data result = count_sort(data, 5);
  KT_ASSERT_EQ(result.data, 42);
  KT_ASSERT_EQ(result.counter, 3);
}

KTEST(count_sort, all_same) {
  uint64_t data[] = {0xdead, 0xdead, 0xdead};
  struct count_sort_data result = count_sort(data, 3);
  KT_ASSERT_EQ(result.data, 0xdead);
  KT_ASSERT_EQ(result.counter, 3);
}

KTEST(count_sort, single_element) {
  uint64_t data[] = {123};
  struct count_sort_data result = count_sort(data, 1);
  KT_ASSERT_EQ(result.data, 123);
  KT_ASSERT_EQ(result.counter, 1);
}

KTEST(count_sort, no_majority) {
  uint64_t data[] = {1, 2, 3, 4, 5};
  struct count_sort_data result = count_sort(data, 5);
  // Should still return the most frequent (all have count=1, first wins)
  KT_ASSERT_EQ(result.counter, 1);
}

KTEST(count_sort, returns_most_frequent_not_least) {
  // This test verifies the bug fix: count_sort used to return the LEAST
  // frequent element due to ascending sort + returning [0].
  uint64_t data[] = {10, 20, 20, 20, 30, 30, 10};
  struct count_sort_data result = count_sort(data, 7);
  KT_ASSERT_EQ(result.data, 20);
  KT_ASSERT_EQ(result.counter, 3);
}
