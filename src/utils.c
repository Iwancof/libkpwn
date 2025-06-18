#define _GNU_SOURCE

#include <dirent.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

void proc_info(logf_t log) {
  pid_t pid = getpid();

  // get open fds
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/fd", pid);

  DIR* dir = opendir(path);
  ASSERT(dir != NULL);

  log("-- proc info %d --", pid);
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_LNK) {
      char link_path[0x200];
      snprintf(link_path, sizeof(link_path), "%s/%s", path,
               entry->d_name);
      char target[0x200];
      ssize_t len =
          readlink(link_path, target, sizeof(target) - 1);
      if (len != -1) {
        target[len] = '\0';  // null-terminate the string
        log("%s -> %s", entry->d_name, target);
      } else {
        log_erro("readlink error for %s: %s", link_path,
                 strerror(errno));
      }
    }
  }
  closedir(dir);
}

const char root_without_password[] =
    "root::0:0:root:/root:/bin/sh";

int compare_count_sort_data(const void* a, const void* b) {
  const struct count_sort_data* data_a =
      (const struct count_sort_data*)a;
  const struct count_sort_data* data_b =
      (const struct count_sort_data*)b;

  if (data_a->counter < data_b->counter) {
    return -1;
  } else if (data_a->counter > data_b->counter) {
    return 1;
  } else {
    return 0;
  }
}

struct count_sort_data count_sort(const uint64_t* data,
                                  size_t len) {
  if (len == 0) {
    log_erro("[count_sort]: empty data array");
    return (struct count_sort_data){0, 0};
  }
  struct count_sort_data count_data[len];
  memset(count_data, 0, sizeof(count_data));

  size_t num_unique = 0;
  for (size_t i = 0; i < len; i++) {
    for (size_t j = 0; j < num_unique; j++) {
      if (data[i] == count_data[j].data) {
        count_data[j].counter++;
        goto next;
      }
    }
    count_data[num_unique].data = data[i];
    count_data[num_unique].counter = 1;
    num_unique++;
  next:
  }

  qsort(count_data, num_unique,
        sizeof(struct count_sort_data),
        compare_count_sort_data);

  if (count_data[0].counter < len / 2) {
    log_warn("[count_sort]: no majority element found");
  }

  return count_data[0];
}