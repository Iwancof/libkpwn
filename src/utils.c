#define _GNU_SOURCE

#include <dirent.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

void proc_info(logf_ptr_t log) {
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
      snprintf(link_path, sizeof(link_path), "%s/%s", path, entry->d_name);
      char target[0x200];
      ssize_t len = readlink(link_path, target, sizeof(target) - 1);
      if (len != -1) {
        target[len] = '\0';  // null-terminate the string
        log("%s -> %s", entry->d_name, target);
      } else {
        log_error("readlink error for %s: %s", link_path, strerror(errno));
      }
    }
  }
  closedir(dir);
}

const char root_without_password[] = "root::0:0:root:/root:/bin/sh";

int compare_count_sort_data(const void* a, const void* b) {
  const struct count_sort_data* data_a = (const struct count_sort_data*)a;
  const struct count_sort_data* data_b = (const struct count_sort_data*)b;

  if (data_a->counter < data_b->counter) {
    return -1;
  } else if (data_a->counter > data_b->counter) {
    return 1;
  } else {
    return 0;
  }
}

struct count_sort_data count_sort(const uint64_t* data, size_t len) {
  if (len == 0) {
    log_error("[count_sort]: empty data array");
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

  qsort(count_data, num_unique, sizeof(struct count_sort_data), compare_count_sort_data);

  if (count_data[0].counter < len / 2) {
    log_warn("[count_sort]: no majority element found");
  }

  return count_data[0];
}

uint64_t pc64(char* bytes) {
  uint64_t ret;
  ASSERT_MSG(bytes != NULL, "bytes is NULL");

  memcpy(&ret, bytes, sizeof(uint64_t));

  return ret;
}

void up64(uint64_t value, char* dst) {
  ASSERT_MSG(dst != NULL, "dst is NULL");

  memcpy(dst, &value, sizeof(uint64_t));
}

uint32_t pc32(char* bytes) {
  uint32_t ret;
  ASSERT_MSG(bytes != NULL, "bytes is NULL");

  memcpy(&ret, bytes, sizeof(uint32_t));

  return ret;
}

void up32(uint32_t value, char* dst) {
  ASSERT_MSG(dst != NULL, "dst is NULL");

  memcpy(dst, &value, sizeof(uint32_t));
}

uint16_t pc16(char* bytes) {
  uint16_t ret;
  ASSERT_MSG(bytes != NULL, "bytes is NULL");

  memcpy(&ret, bytes, sizeof(uint16_t));

  return ret;
}

void up16(uint16_t value, char* dst) {
  ASSERT_MSG(dst != NULL, "dst is NULL");

  memcpy(dst, &value, sizeof(uint16_t));
}

uint8_t pc8(char* bytes) {
  uint8_t ret;
  ASSERT_MSG(bytes != NULL, "bytes is NULL");

  memcpy(&ret, bytes, sizeof(uint8_t));

  return ret;
}

void up8(uint8_t value, char* dst) {
  ASSERT_MSG(dst != NULL, "dst is NULL");

  memcpy(dst, &value, sizeof(uint8_t));
}

uint64_t swab64(uint64_t value) {
  return ((
        ((value & 0xFF00000000000000ULL) >> 56) |
        ((value & 0x00FF000000000000ULL) >> 40) |
        ((value & 0x0000FF0000000000ULL) >> 24) |
        ((value & 0x000000FF00000000ULL) >> 8) |
        ((value & 0x00000000FF000000ULL) << 8) |
        ((value & 0x0000000000FF0000ULL) << 24) |
        ((value & 0x000000000000FF00ULL) << 40) |
        ((value & 0x00000000000000FFULL) << 56)
  ));
}

char* str_dup_or_null(const char* s) {
  if (!s) return NULL;
  size_t n = strlen(s);
  char* p = (char*)malloc(n + 1);
  if (!p) return NULL;
  memcpy(p, s, n);
  p[n] = '\0';
  return p;
}

void trim_trailing_newlines(char* s) {
  if (!s) return;
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
    s[n - 1] = '\0';
    n--;
  }
}

char* slurp_file(const char* path, size_t max_bytes) {
  FILE* f = fopen(path, "re");
  if (!f) return NULL;
  char* buf = (char*)malloc(max_bytes + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t r = fread(buf, 1, max_bytes, f);
  fclose(f);
  buf[r] = '\0';
  return buf;
}

int slurp_int_file(const char* path, int na_value) {
  char* s = slurp_file(path, 256);
  if (!s) return na_value;
  trim_trailing_newlines(s);
  char* end = NULL;
  long v = strtol(s, &end, 10);
  free(s);
  if (end == s) return na_value;
  return (int)v;
}

int file_is_readable(const char* path) {
  return access(path, R_OK) == 0;
}

int file_exists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0;
}

char* popen_read(const char* cmd, size_t max_bytes) {
  FILE* p = popen(cmd, "r");
  if (!p) return NULL;
  char* buf = (char*)malloc(max_bytes + 1);
  if (!buf) {
    pclose(p);
    return NULL;
  }
  size_t off = 0;
  while (!feof(p) && off < max_bytes) {
    size_t r = fread(buf + off, 1, max_bytes - off, p);
    if (r == 0) break;
    off += r;
  }
  pclose(p);
  buf[off] = '\0';
  return buf;
}

int command_exists(const char* cmd) {
  char command[256];
  snprintf(command, sizeof(command), "command -v %s 2>/dev/null", cmd);
  char* out = popen_read(command, 256);
  if (!out) return 0;
  int ok = (out[0] != '\0');
  free(out);
  return ok;
}

char* read_first_n_lines(const char* text, size_t max_lines) {
  if (!text) return NULL;
  size_t count = 0;
  const char* p = text;
  while (*p && count < max_lines) {
    if (*p == '\n') count++;
    p++;
  }
  size_t n = (size_t)(p - text);
  char* out = (char*)malloc(n + 1);
  if (!out) return NULL;
  memcpy(out, text, n);
  out[n] = '\0';
  return out;
}

char* read_status_key_line(const char* path, const char* key_prefix) {
  FILE* f = fopen(path, "re");
  if (!f) return NULL;
  char line[1024];
  char* out = NULL;
  size_t prefix_len = strlen(key_prefix);
  while (fgets(line, sizeof(line), f)) {
    if (strncasecmp(line, key_prefix, prefix_len) == 0) {
      trim_trailing_newlines(line);
      out = str_dup_or_null(line);
      break;
    }
  }
  fclose(f);
  return out;
}

int contains_token_case_insensitive(const char* haystack, const char* needle) {
  if (!haystack || !needle) return 0;
  return strcasestr(haystack, needle) != NULL;
}