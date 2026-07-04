#define _GNU_SOURCE

#include <kpwn/logger.h>
#include <kpwn/slab.h>
#include <kpwn/utils.h>

#include <stdio.h>
#include <string.h>

static int query_sysfs(const char *cache_name, struct kpwn_slab_info *out) {
  char path[256];
  memset(out, 0, sizeof(*out));
  strncpy(out->name, cache_name, sizeof(out->name) - 1);

  snprintf(path, sizeof(path), "/sys/kernel/slab/%s/object_size", cache_name);
  int v = slurp_int_file(path, -1);
  if (v < 0)
    return -1;
  out->object_size = (size_t)v;

  snprintf(path, sizeof(path), "/sys/kernel/slab/%s/objs_per_slab", cache_name);
  v = slurp_int_file(path, 0);
  out->objs_per_slab = (size_t)v;

  snprintf(path, sizeof(path), "/sys/kernel/slab/%s/order", cache_name);
  v = slurp_int_file(path, 0);
  out->slab_order = (size_t)v;

  snprintf(path, sizeof(path), "/sys/kernel/slab/%s/partial", cache_name);
  v = slurp_int_file(path, 0);
  out->partial_slabs = (size_t)v;

  out->is_cg = (strstr(cache_name, "-cg-") != NULL);
  return 0;
}

static int query_slabinfo(const char *cache_name, struct kpwn_slab_info *out) {
  char *content = slurp_file("/proc/slabinfo", 1 << 20);
  if (!content)
    return -1;

  memset(out, 0, sizeof(*out));
  strncpy(out->name, cache_name, sizeof(out->name) - 1);

  char *line = content;
  while (*line) {
    char *nl = strchr(line, '\n');
    if (nl)
      *nl = '\0';

    char name[128];
    size_t active, num, objsz, objs_per;
    if (sscanf(line, "%127s %zu %zu %zu %zu", name, &active, &num, &objsz,
               &objs_per) >= 4) {
      if (strcmp(name, cache_name) == 0) {
        out->object_size = objsz;
        out->active_objs = active;
        out->objs_per_slab = objs_per;
        free(content);
        return 0;
      }
    }

    if (!nl)
      break;
    line = nl + 1;
  }

  free(content);
  return -1;
}

int kpwn_slab_query(const char *cache_name, struct kpwn_slab_info *out) {
  if (query_sysfs(cache_name, out) == 0) {
    log_debug("[kpwn:slab] %s obj_size=%zu objs_per_slab=%zu order=%zu (sysfs)",
              cache_name, out->object_size, out->objs_per_slab,
              out->slab_order);
    return 0;
  }
  if (query_slabinfo(cache_name, out) == 0) {
    log_debug("[kpwn:slab] %s obj_size=%zu objs_per_slab=%zu (slabinfo)",
              cache_name, out->object_size, out->objs_per_slab);
    return 0;
  }
  log_warn("[kpwn:slab] cache %s not found", cache_name);
  return -1;
}

int kpwn_slab_query_kmalloc(size_t size, struct kpwn_slab_info *out) {
  static const size_t buckets[] = {32,  64,   96,   128,  192, 256,
                                   512, 1024, 2048, 4096, 8192};
  size_t cache_size = 0;
  for (size_t i = 0; i < sizeof(buckets) / sizeof(buckets[0]); i++) {
    if (size <= buckets[i]) {
      cache_size = buckets[i];
      break;
    }
  }
  if (!cache_size) {
    log_error("[kpwn:slab] size %zu exceeds max kmalloc bucket", size);
    return -1;
  }

  char name[64];
  snprintf(name, sizeof(name), "kmalloc-%zu", cache_size);
  if (kpwn_slab_query(name, out) == 0)
    return 0;

  snprintf(name, sizeof(name), "kmalloc-cg-%zu", cache_size);
  return kpwn_slab_query(name, out);
}

int kpwn_slab_dump(void) {
  char *content = slurp_file("/proc/slabinfo", 1 << 20);
  if (!content) {
    log_error("[kpwn:slab] cannot read /proc/slabinfo");
    return -1;
  }

  char *line = content;
  int count = 0;
  while (*line) {
    char *nl = strchr(line, '\n');
    if (nl)
      *nl = '\0';

    char name[128];
    size_t active, num, objsz;
    if (sscanf(line, "%127s %zu %zu %zu", name, &active, &num, &objsz) >= 4) {
      log_info("[kpwn:slab] name=%s active=%zu total=%zu obj_size=%zu", name,
               active, num, objsz);
      count++;
    }

    if (!nl)
      break;
    line = nl + 1;
  }

  free(content);
  log_debug("[kpwn:slab] dumped %d caches", count);
  return 0;
}
