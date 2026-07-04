#ifndef _KPWN_SLAB_
#define _KPWN_SLAB_

#include <stddef.h>

// SLUB slab-cache introspection for cross-cache attack planning.
//
// Before mounting a cross-cache (see kpwn/crosscache.h) you need the geometry of
// the caches involved: how big each object is, how many objects pack into a
// slab, and what page order a slab spans. That tells you how many objects to
// spray to fill a page, and which victim/target caches share a slab order (a
// prerequisite for a freed page to be reusable across caches).
//
// Data comes from two kernel interfaces, tried in order:
//   1. /sys/kernel/slab/<name>/  — per-cache SLUB attributes (object_size,
//      objs_per_slab, order, objects, slabs, partial). Primary source, and the
//      only place partial-slab counts are exposed.
//   2. /proc/slabinfo            — the classic table. Fallback when sysfs is
//      unavailable; does not report partial slabs.
//
// Both interfaces are typically root-only, which matches the post-exploitation
// context these helpers run in.

struct kpwn_slab_info {
  char name[128];       // cache name, e.g. "kmalloc-1k"
  size_t object_size;   // usable bytes per object
  size_t objs_per_slab; // objects packed into one slab
  size_t slab_order;    // buddy order of a slab (pages = 1 << order)
  size_t active_objs;   // objects currently in use
  size_t total_slabs;   // total slabs (full + partial)
  size_t partial_slabs; // partial slabs (sysfs only; 0 if unknown)
  int is_cg;            // 1 if memcg-accounted (kmalloc-cg-*), else 0
};

// Query a single cache by name. Parses /sys/kernel/slab/<cache_name>/ first and
// falls back to /proc/slabinfo. Returns 0 and fills *out on success, -1 if the
// cache cannot be found or an argument is NULL.
int kpwn_slab_query(const char *cache_name, struct kpwn_slab_info *out);

// Query the kmalloc cache that backs an allocation of the given size. The size
// is rounded up to the next kmalloc bucket (32, 64, 96, 128, 192, 256, 512,
// 1024, 2048, 4096, 8192) and mapped to the corresponding cache name
// ("kmalloc-32" ... "kmalloc-8k"). Returns -1 if size exceeds the largest
// kmalloc slab (8192) or out is NULL.
int kpwn_slab_query_kmalloc(size_t size, struct kpwn_slab_info *out);

// Dump every cache as machine-parseable lines:
//   [kpwn:slab] name=X obj_size=Y objs_per_slab=Z
// Enumerates /proc/slabinfo, falling back to /sys/kernel/slab/. Returns 0 if at
// least one cache was printed, -1 otherwise.
int kpwn_slab_dump(void);

#endif
