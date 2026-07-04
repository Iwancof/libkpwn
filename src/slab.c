#define _GNU_SOURCE

#include <kpwn/logger.h>
#include <kpwn/slab.h>
#include <kpwn/utils.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#define SLAB_SYS_DIR "/sys/kernel/slab"
#define SLAB_PROC_PATH "/proc/slabinfo"

// /proc/slabinfo can list several hundred caches; 256 KiB is comfortably large.
#define SLAB_PROC_MAX_BYTES (1 << 18)

// The kmalloc bucket sizes a general-purpose allocation rounds up into. Matches
// the general-purpose kmalloc-* slabs (the -cg- / dma- variants share sizes).
static const size_t kmalloc_buckets[] = {32,  64,   96,   128,  192, 256,
                                         512, 1024, 2048, 4096, 8192};

// slab_order is not stored by slabinfo directly; derive it from pages-per-slab.
static size_t order_from_pages(size_t pages) {
  size_t order = 0;
  while (pages > 1) {
    pages >>= 1;
    order++;
  }
  return order;
}

// memcg-accounted caches carry a "-cg-" token, e.g. "kmalloc-cg-1k".
static int name_is_cg(const char *name) { return strstr(name, "-cg-") != NULL; }

// Read one unsigned attribute from /sys/kernel/slab/<cache>/<attr>. sysfs count
// files may append a per-node breakdown ("7585 N0=7585"); slurp_int_file stops
// at the first non-digit, so it returns the leading total.
static int read_sysfs_attr(const char *cache, const char *attr, int na) {
  char path[320];
  snprintf(path, sizeof(path), SLAB_SYS_DIR "/%s/%s", cache, attr);
  return slurp_int_file(path, na);
}

// Primary source: /sys/kernel/slab/<cache_name>/. Returns 0 on success.
static int slab_query_sysfs(const char *cache_name,
                            struct kpwn_slab_info *out) {
  char dir[256];
  snprintf(dir, sizeof(dir), SLAB_SYS_DIR "/%s", cache_name);
  if (!file_exists(dir))
    return -1;

  int object_size = read_sysfs_attr(cache_name, "object_size", -1);
  if (object_size <= 0)
    return -1; // present but not a SLUB cache we understand

  int objs_per_slab = read_sysfs_attr(cache_name, "objs_per_slab", 0);
  int order = read_sysfs_attr(cache_name, "order", 0);
  int active = read_sysfs_attr(cache_name, "objects", 0);
  int slabs = read_sysfs_attr(cache_name, "slabs", 0);
  int partial = read_sysfs_attr(cache_name, "partial", 0);

  memset(out, 0, sizeof(*out));
  snprintf(out->name, sizeof(out->name), "%s", cache_name);
  out->object_size = (size_t)object_size;
  out->objs_per_slab = objs_per_slab > 0 ? (size_t)objs_per_slab : 0;
  out->slab_order = order > 0 ? (size_t)order : 0;
  out->active_objs = active > 0 ? (size_t)active : 0;
  out->total_slabs = slabs > 0 ? (size_t)slabs : 0;
  out->partial_slabs = partial > 0 ? (size_t)partial : 0;
  out->is_cg = name_is_cg(cache_name);
  return 0;
}

// Parse a single /proc/slabinfo data line into *out. Returns 1 on success, 0
// for banner/header/blank lines. The line must be NUL-terminated (no newline).
//
// Format (version 2.1), one cache per line:
//   name active_objs num_objs objsize objperslab pagesperslab
//   : tunables L B S : slabdata active_slabs num_slabs sharedavail
static int parse_slabinfo_line(const char *line, struct kpwn_slab_info *out) {
  if (line[0] == '\0' || line[0] == '#')
    return 0;
  if (strncmp(line, "slabinfo", 8) == 0)
    return 0;

  char name[128];
  size_t active_objs, num_objs, objsize, objperslab, pagesperslab;
  if (sscanf(line, "%127s %zu %zu %zu %zu %zu", name, &active_objs, &num_objs,
             &objsize, &objperslab, &pagesperslab) != 6)
    return 0;

  size_t active_slabs = 0, num_slabs = 0;
  const char *sd = strstr(line, "slabdata");
  if (sd)
    sscanf(sd, "slabdata %zu %zu", &active_slabs, &num_slabs);

  memset(out, 0, sizeof(*out));
  snprintf(out->name, sizeof(out->name), "%s", name);
  out->object_size = objsize;
  out->objs_per_slab = objperslab;
  out->slab_order = order_from_pages(pagesperslab);
  out->active_objs = active_objs;
  out->total_slabs = num_slabs;
  out->partial_slabs = 0; // not reported by /proc/slabinfo
  out->is_cg = name_is_cg(name);
  return 1;
}

// Fallback source: scan /proc/slabinfo for cache_name. Returns 0 on success.
static int slab_query_proc(const char *cache_name, struct kpwn_slab_info *out) {
  char *buf = slurp_file(SLAB_PROC_PATH, SLAB_PROC_MAX_BYTES);
  if (!buf)
    return -1;

  int ret = -1;
  char *save = NULL;
  for (char *line = strtok_r(buf, "\n", &save); line != NULL;
       line = strtok_r(NULL, "\n", &save)) {
    struct kpwn_slab_info tmp;
    if (parse_slabinfo_line(line, &tmp) && strcmp(tmp.name, cache_name) == 0) {
      *out = tmp;
      ret = 0;
      break;
    }
  }

  free(buf);
  return ret;
}

int kpwn_slab_query(const char *cache_name, struct kpwn_slab_info *out) {
  if (!cache_name || !out) {
    log_error("[kpwn:slab] query: NULL argument");
    return -1;
  }

  if (slab_query_sysfs(cache_name, out) == 0) {
    log_debug(
        "[kpwn:slab] %s via sysfs obj_size=%zu objs_per_slab=%zu order=%zu",
        out->name, out->object_size, out->objs_per_slab, out->slab_order);
    return 0;
  }

  if (slab_query_proc(cache_name, out) == 0) {
    log_debug(
        "[kpwn:slab] %s via /proc/slabinfo obj_size=%zu objs_per_slab=%zu",
        out->name, out->object_size, out->objs_per_slab);
    return 0;
  }

  log_warn("[kpwn:slab] cache '%s' not found", cache_name);
  return -1;
}

int kpwn_slab_query_kmalloc(size_t size, struct kpwn_slab_info *out) {
  if (!out) {
    log_error("[kpwn:slab] query_kmalloc: NULL out");
    return -1;
  }

  size_t bucket = 0;
  for (size_t i = 0; i < ARRAY_SIZE(kmalloc_buckets); i++) {
    if (size <= kmalloc_buckets[i]) {
      bucket = kmalloc_buckets[i];
      break;
    }
  }
  if (bucket == 0) {
    log_warn("[kpwn:slab] size %zu exceeds largest kmalloc slab (8192)", size);
    return -1;
  }

  // Cache names use a "k" suffix from 1 KiB up: kmalloc-1k, kmalloc-2k, ...
  char name[64];
  if (bucket >= 1024)
    snprintf(name, sizeof(name), "kmalloc-%zuk", bucket / 1024);
  else
    snprintf(name, sizeof(name), "kmalloc-%zu", bucket);

  log_debug("[kpwn:slab] size %zu -> %s", size, name);
  return kpwn_slab_query(name, out);
}

int kpwn_slab_dump(void) {
  // Prefer /proc/slabinfo: one read yields every real cache with obj_size and
  // objs_per_slab, and avoids the merged-cache alias symlinks under sysfs.
  char *buf = slurp_file(SLAB_PROC_PATH, SLAB_PROC_MAX_BYTES);
  if (buf) {
    int count = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line != NULL;
         line = strtok_r(NULL, "\n", &save)) {
      struct kpwn_slab_info info;
      if (!parse_slabinfo_line(line, &info))
        continue;
      log_info("[kpwn:slab] name=%s obj_size=%zu objs_per_slab=%zu", info.name,
               info.object_size, info.objs_per_slab);
      count++;
    }
    free(buf);
    log_debug("[kpwn:slab] dumped %d caches from %s", count, SLAB_PROC_PATH);
    return count > 0 ? 0 : -1;
  }

  // Fallback: enumerate /sys/kernel/slab/ and query each entry.
  DIR *dir = opendir(SLAB_SYS_DIR);
  if (!dir) {
    log_error("[kpwn:slab] dump: neither %s nor %s is readable", SLAB_PROC_PATH,
              SLAB_SYS_DIR);
    return -1;
  }

  int count = 0;
  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.')
      continue;
    struct kpwn_slab_info info;
    if (slab_query_sysfs(ent->d_name, &info) == 0) {
      log_info("[kpwn:slab] name=%s obj_size=%zu objs_per_slab=%zu", info.name,
               info.object_size, info.objs_per_slab);
      count++;
    }
  }
  closedir(dir);
  log_debug("[kpwn:slab] dumped %d caches from %s", count, SLAB_SYS_DIR);
  return count > 0 ? 0 : -1;
}
