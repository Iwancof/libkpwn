#include <criterion/criterion.h>
#include <kpwn/utils.h>

Test(rep_simple, test) {
  int counter = 0;
  REP(10) { counter += 1; }

  cr_assert(counter == 10);
}

Test(rep_index, test) {
  int array[10] = {0};
  REP(i, 10) { array[i] = i * 2; }

  for (int i = 0; i < 10; i++) {
    cr_assert(array[i] == i * 2);
  }
}

Test(rep_range, test) {
  int array[5] = {0};
  REP(i, 1, 3) { array[i] = i * 2; }

  cr_assert(array[0] == 0);
  cr_assert(array[1] == 2);
  cr_assert(array[2] == 4);
  cr_assert(array[3] == 0);
  cr_assert(array[4] == 0);
}

Test(rep_skip, test) {
  int array[10] = {0};
  REP(i, 0, 10, 2) { array[i] = i * 2; }

  for (int i = 0; i < 10; i++) {
    if (i % 2 == 0) {
      cr_assert(array[i] == i * 2);
    } else {
      cr_assert(array[i] == 0);
    }
  }
}

Test(rep_backward, test) {
  int counter = 0;
  int array[10] = {0};

  REP(i, 0, 10, 1, REP_BACKWARD) { array[counter++] = i * 2; }

  cr_assert(counter == 10);
  for (int i = 0; i < 10; i++) {
    cr_assert(array[i] == (10 - i - 1) * 2);
  }
}
