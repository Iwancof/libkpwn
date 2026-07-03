#ifndef _KPWN_CROSSCACHE_
#define _KPWN_CROSSCACHE_

#include <stddef.h>

// Cross-cache attack orchestration.
//
// The idea: free enough objects in a target slab so the entire page (order-0)
// returns to the page allocator (buddy system), then reclaim it from a
// different cache. This converts a UAF in cache A into a type-confusion /
// overlap in cache B.
//
// Typical flow (msg_msg → pipe_buffer cross-cache):
//   kpwn_defrag_msg(...)      // fill fragmented pages in the msg cache
//   kpwn_spray_msg(...)       // N contiguous allocations → ~full pages
//   ... trigger UAF in one of the sprayed msg objects ...
//   kpwn_free_msg(...)        // free ALL spray objects → pages return to buddy
//   kpwn_spray_pipe(...)      // reclaim the freed pages for pipe_buffer
//
// The helpers below wrap that sequence and add defrag (pre-fill) and drainage
// (consume free pages to force new slab pages) utilities.

#include <kpwn/spray.h>

// Drain the page allocator to force new slab allocations to come from fresh
// pages (rather than recycling existing partial slabs). Uses mmap to consume
// physical pages.
int kpwn_drain_pages(void **maps, size_t n_pages);
int kpwn_release_pages(void **maps, size_t n_pages);

// Defragment a slab by spraying a large number of objects to fill partial
// pages. Returns 0 on success. The caller is responsible for freeing defrag
// objects after the cross-cache is complete.
int kpwn_defrag_msg(int *qids, size_t n, size_t obj_size);

#endif
