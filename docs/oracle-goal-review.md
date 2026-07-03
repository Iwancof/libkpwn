# Oracle Goal Review (GPT-5.5 Pro Extended, 2026-07-04)

## Priority Reorder (recommended)
1. kpwn_target + kpwn_report (was P3+P4) — foundation everything depends on
2. slab query + cross-cache orchestration (was P6+P7) — dominant modern primitive
3. pipe high-level API (was P2) — needs OOB/cross-cache first
4. Dirty Pagetable / PTE / physrw (was P5) — needs page-level UAF first
5. race helpers (was P1) — bug-class-specific, not universally needed

## Missing Techniques
- io_uring lifecycle (beyond cred spray): setup_many, register_pbuf_ring, mmap_rings
- tty_struct / timerfd_ctx / eventfd_ctx / seq_file sprays
- Namespace setup (userns_enter, netns_enter, ns_setup_net_admin)
- Data-only attacks (cred_patch_uid0, file_replace_f_cred, task_find_current_cred)
- Kernel stack spray/reclaim (kstack_reclaim_page, kstack_spray_sendmsg)
- BPF JIT probe (gated, not default)
- KASLR facade (kaslr_from_pipe_ops, kaslr_from_func_ptr, kaslr_try_all)
- Page-cache helpers (pagecache_prime, pipe_splice_file_page)

## Key API Fixes
- kpwn_kwrite_fn: void* → uint64_t (kernel addrs aren't valid userspace ptrs)
- Race: split pile init from arming (kpwn_epoll_pile_init + arm)
- Pipe: kpwn_pipebuf_parse_leak() instead of raw ops extraction
- Target: needs confidence/source bits, symbols, offsets, surfaces
- Report: should fill a target struct, not be separate
- Slab: object-aware query (kpwn_slab_query_object) not just name-based
- README: kpwn_spray_msg sample has stale extra argument

## Over-engineering Warnings
- Generic race worker → simple kpwn_race_run() wrapper
- kpwn_physrw_init as magic abstraction → split PTE helpers
- kpwn_report separate from target → merge into one path
- mprotect stall context → add one-shot kpwn_usercopy_stall_once()
