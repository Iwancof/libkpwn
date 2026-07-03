#include "ktest.h"
#include <kpwn/utils.h>

KTEST(rep, simple) {
  int counter = 0;
  REP(10) { counter += 1; }

  KT_ASSERT_EQ(counter, 10);
}

KTEST(rep, index) {
  int array[10] = {0};
  REP(i, 10) { array[i] = i * 2; }

  for (int i = 0; i < 10; i++) {
    KT_ASSERT_EQ(array[i], i * 2);
  }
}

KTEST(rep, range) {
  int array[5] = {0};
  REP(i, 1, 3) { array[i] = i * 2; }

  KT_ASSERT_EQ(array[0], 0);
  KT_ASSERT_EQ(array[1], 2);
  KT_ASSERT_EQ(array[2], 4);
  KT_ASSERT_EQ(array[3], 0);
  KT_ASSERT_EQ(array[4], 0);
}

KTEST(rep, skip) {
  int array[10] = {0};
  REP(i, 0, 10, 2) { array[i] = i * 2; }

  for (int i = 0; i < 10; i++) {
    if (i % 2 == 0) {
      KT_ASSERT_EQ(array[i], i * 2);
    } else {
      KT_ASSERT_EQ(array[i], 0);
    }
  }
}

KTEST(rep, backward) {
  int counter = 0;
  int array[10] = {0};

  REP(i, 0, 10, 1, REP_BACKWARD) { array[counter++] = i * 2; }

  KT_ASSERT_EQ(counter, 10);
  for (int i = 0; i < 10; i++) {
    KT_ASSERT_EQ(array[i], (10 - i - 1) * 2);
  }
}
