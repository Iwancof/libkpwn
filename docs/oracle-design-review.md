BEGIN_ORACLE_REVIEW libkpwn-design-review

## Top-line verdict

The library is pointed in the right direction for LLM consumption, but the current API is not yet “stable-contract” quality. The biggest issue is not missing primitives; it is that the APIs imply more determinism than they provide. In particular, cache placement flags are ignored or overspecified, size constants are treated as universal, return values do not consistently mean success, and several logs are not actually grep-compatible with the stated `[kpwn:*]` contract.

For LLMs, favor explicit state, explicit assumptions, and boring names over clever wrappers. The ideal API should make a model produce code that says: “allocate this spray kind, for this desired cache size, using this kernel-profile assumption, with this many successful objects,” then logs exactly that.

## Must-fix issues before adding more primitives

1. **Fix `kpwn_free_key()` immediately.**
   `src/spray.c:129-132` uses `syscall(__NR_keyctl, 0 /* KEYCTL_REVOKE */, keyids[i]);`. In the UAPI, keyctl command `0` is `KEYCTL_GET_KEYRING_ID`; `KEYCTL_REVOKE` is `3`. This means the current free path likely does not revoke the sprayed keys. Use the real `KEYCTL_REVOKE` constant, and consider supporting `KEYCTL_UNLINK` / `KEYCTL_INVALIDATE` as explicit modes. ([GitHub][1])

2. **Do not expose cache-selection flags that do nothing.**
   `KPWN_MSG_KMALLOC_ANY` and `KPWN_MSG_KMALLOC_CG` currently have no effect. Worse, current upstream `msg_msg` allocation is not a simple “pick kmalloc vs kmalloc-cg” decision: primary `msg_msg` allocations use dedicated `kmem_buckets` with `SLAB_ACCOUNT`, while `msg_msgseg` allocations use `GFP_KERNEL_ACCOUNT`. Treat cache class as an observed/profiled property, not as a caller-selectable flag. ([GitHub][2])

3. **Standardize error handling.**
   Right now:

   * `msg`, `pipe`, `skb`, and `pgv` abort via `SYSCHK`.
   * `key` returns `-1`.
   * `xattr` ignores failure and always returns `0`.
   * `read_msg` returns syscall result but does not log failure.

   For an LLM-facing library, this is dangerous. Make library functions return `0` or `-errno`, store partial progress in a context, and reserve `*_or_die()` wrappers for quick CTF throwaway code.

4. **Make every log line parseable by `grep '\[kpwn:'`.**
   README promises `[kpwn:*]`, but current code emits `[kchecksec]`, `[alloc_n_creds]`, `SYSCHK error ...`, and multi-line `capsh` output. Rename to `[kpwn:kchecksec]`, `[kpwn:credspray]`, `[kpwn:error]`, and escape embedded newlines.

5. **Remove color by default from machine logs.**
   ANSI color sequences are hostile to grep/LLM parsing. Default to no color unless `isatty()` and `KPWN_COLOR=1`, and honor `NO_COLOR`.

---

## 1. `spray.h` API design

### Consistency of the six spray APIs

They are not currently consistent enough.

The six APIs should share the same lifecycle shape:

```c
init / alloc
prime / write / fill
read / drain / inspect, if meaningful
free / revoke / close / remove
```

Current behavior by object:

| Spray         | Current API issue                                                                        | Recommended fix                                                             |
| ------------- | ---------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| `msg_msg`     | Has alloc/free/read, but `flags` ignored; aborts on failure.                             | Keep typed API, add cache-size helper and context with `done`.              |
| `pipe_buffer` | `kpwn_spray_pipe()` only creates pipes; actual useful pipe-buffer state requires writes. | Rename or split as `kpwn_pipe_open_many()` and `kpwn_pipe_prime_buffers()`. |
| `setxattr`    | No free/remove helper; ignores `setxattr()` errors; fixed names can collide.             | Return `-errno`, add `kpwn_free_xattr()`, caller-controlled prefix.         |
| `add_key`     | Free path is wrong; no read/update helper; inconsistent error style.                     | Fix keyctl op, add revoke/unlink mode, optional `read_key`.                 |
| `sk_buff`     | No drain/read helper; can block/fail when socket queue fills.                            | Add socket buffer sizing, `MSG_DONTWAIT` option, `kpwn_drain_skb()`.        |
| `pgv`         | Only sets TX ring; no mmap pointer; no RX/TX choice; no validation.                      | Add packet-ring context with validation and optional mmap.                  |

A better typed API pattern:

```c
struct kpwn_msg_spray {
    int *qids;
    size_t n;
    size_t done;
    size_t payload_len;
    size_t requested_cache;
    int flags;
};

int kpwn_msg_spray_init(struct kpwn_msg_spray *s,
                        int *qids,
                        size_t n,
                        size_t payload_len,
                        unsigned flags);

int kpwn_msg_spray_fill(struct kpwn_msg_spray *s,
                        const void *data,
                        size_t len);

int kpwn_msg_spray_free(struct kpwn_msg_spray *s);
```

This makes partial failure machine-visible: `done=137 rc=-ENOSPC`.

### Common `spray_context` / `spray_ops` vtable

Add one, but do **not** make it the primary API.

Typed functions are better for LLMs because they are easy to grep, easy to autocomplete, and harder to misuse. A vtable is useful for cross-cache recipes and reclaim helpers.

Recommended layering:

```c
enum kpwn_spray_kind {
    KPWN_SPRAY_MSG,
    KPWN_SPRAY_PIPE,
    KPWN_SPRAY_XATTR,
    KPWN_SPRAY_KEY,
    KPWN_SPRAY_SKB,
    KPWN_SPRAY_PGV,
};

struct kpwn_spray_ctx {
    enum kpwn_spray_kind kind;
    const char *name;
    size_t n;
    size_t done;
    size_t payload_len;
    size_t requested_cache;
    unsigned flags;
    void *state;
};

struct kpwn_spray_ops {
    const char *name;
    int (*alloc)(struct kpwn_spray_ctx *);
    int (*prime)(struct kpwn_spray_ctx *, const void *data, size_t len);
    int (*read_one)(struct kpwn_spray_ctx *, size_t idx, void *buf, size_t len);
    int (*free)(struct kpwn_spray_ctx *);
};
```

Use this internally for recipe/cross-cache code:

```c
int kpwn_reclaim_as(struct kpwn_spray_ctx *ctx,
                    const struct kpwn_spray_ops *ops);
```

Keep direct APIs like `kpwn_spray_msg()` as convenience wrappers.

### Size constants

The constants are reasonable for common 64-bit kernels, but the header should stop presenting them as universal “kernel 6.x” truths.

`KPWN_MSG_HDR_SIZE = 48` is correct for the current 64-bit `struct msg_msg` layout: `list_head`, `long m_type`, `size_t m_ts`, `struct msg_msgseg *next`, and `void *security`. The `msg_msgseg` header is one pointer, so `8` is right on 64-bit. These are not portable to 32-bit, and current upstream allocation behavior uses `kmem_buckets` for primary messages rather than a plain kmalloc cache. ([GitHub][3])

`KPWN_KEY_HDR_SIZE = 24` is correct as the common 64-bit offset of `user_key_payload.data`: `struct rcu_head`, `unsigned short datalen`, then alignment padding before the flexible data array. It should be documented as a 64-bit ABI/profile constant, not a universal kernel invariant. ([GitHub][4])

`KPWN_SKB_SHARED_INFO_SIZE = 320` is correct for a common 64-bit configuration with default `MAX_SKB_FRAGS = 17` and 16-byte `skb_frag_t`, and it matches 64-byte alignment behavior. But `skb_shared_info` size is configuration- and architecture-sensitive, especially through `MAX_SKB_FRAGS` and cacheline alignment. Rename it to something like `KPWN_SKB_SHINFO_SIZE_X86_64_17FRAGS` or move it into a kernel-profile table. ([GitHub][5])

Also add:

```c
#define KPWN_PIPE_BUFFER_SIZE_X86_64 40
#define KPWN_PIPE_DEF_BUFFERS 16
```

Current upstream `struct pipe_buffer` is 40 bytes on 64-bit, and default pipes use 16 buffers. The pipe buffer array is allocated with `GFP_KERNEL_ACCOUNT`, so it often lands in accounted caches; again, expose this as profile/observed behavior rather than a hard promise. ([GitHub][6])

Recommended helpers:

```c
size_t kpwn_msg_payload_for_cache(size_t cache_size);
size_t kpwn_msgseg_payload_for_cache(size_t cache_size);
size_t kpwn_key_payload_for_cache(size_t cache_size);
size_t kpwn_pipe_ring_bytes(size_t nr_pipe_buffers);
size_t kpwn_skb_head_payload_for_cache(size_t cache_size,
                                       size_t shinfo_size);
```

Important bug: `kpwn_defrag_msg(int *qids, size_t n, size_t obj_size)` passes `obj_size` as the **payload length**, so the actual primary allocation is `obj_size + sizeof(struct msg_msg)`. Rename the argument to `payload_len` or change the function to accept `cache_size` and internally subtract `KPWN_MSG_HDR_SIZE`.

### Specifying target kmalloc cache

Yes, support “desired cache size,” but avoid promising exact cache placement.

Good API:

```c
enum kpwn_cache_account {
    KPWN_CACHE_UNKNOWN = 0,
    KPWN_CACHE_KMALLOC,
    KPWN_CACHE_KMALLOC_CG,
    KPWN_CACHE_KMEM_BUCKET,
    KPWN_CACHE_PAGE_ALLOC,
};

struct kpwn_cache_goal {
    size_t object_size;
    size_t payload_len;
    size_t cache_size;
    enum kpwn_cache_account expected_account;
    const char *expected_name;
};
```

Then:

```c
int kpwn_msg_goal_for_cache(struct kpwn_cache_goal *goal,
                            size_t cache_size,
                            unsigned flags);
```

Logs should say:

```text
[kpwn:spray] kind=msg action=alloc n=256 done=256 payload_len=208 cache_goal=256 account=kmem_bucket rc=0
```

Do not keep `KPWN_MSG_KMALLOC_CG` unless it is either enforced or logged as advisory.

### Thread safety

Default full thread safety is not necessary for CTF one-shot code, but **context-local thread compatibility** is needed.

Actionable policy:

* Spray contexts are caller-owned and can be used from one thread at a time.
* Logger is process-global and not guaranteed ordered unless `KPWN_LOG_LOCK=1`.
* `kbase` should not be a naked global for new APIs. Prefer `struct kpwn_target`.
* Race helpers must own their own affinity, barriers, stop flags, and statistics.

---

## 2. `crosscache.h` completeness

The current `crosscache.h` is only a small utility layer, not cross-cache orchestration.

The drain/defrag/reclaim pattern is conceptually right, but incomplete. Real cross-cache work needs:

1. CPU affinity control.
2. Slab/cache profile.
3. Object-count planning.
4. A victim/free phase.
5. A reclaim-as-type phase.
6. Partial-failure visibility.

### Add a cross-cache context

```c
struct kpwn_slab_info {
    char name[64];
    size_t object_size;
    size_t objs_per_slab;
    size_t slab_order;
    size_t active_objs;
    size_t total_objs;
    size_t slabs;
    size_t partial;
};

struct kpwn_xcache_plan {
    struct kpwn_slab_info src;
    struct kpwn_slab_info dst;
    size_t pages_to_drain;
    size_t defrag_objects;
    size_t victim_objects;
    size_t reclaim_objects;
    int cpu;
    unsigned flags;
};

struct kpwn_xcache_ctx {
    struct kpwn_xcache_plan plan;
    int old_cpu;
    size_t done_defrag;
    size_t done_victim;
    size_t done_reclaim;
};
```

Add:

```c
int kpwn_slab_query(const char *cache_name, struct kpwn_slab_info *out);
int kpwn_xcache_plan(struct kpwn_xcache_plan *p,
                     const char *src_cache,
                     const char *dst_cache,
                     size_t target_pages);

int kpwn_xcache_begin(struct kpwn_xcache_ctx *ctx);
int kpwn_xcache_end(struct kpwn_xcache_ctx *ctx);
```

`kpwn_slab_query()` should read `/sys/kernel/slab/<cache>/` when available and fall back to `/proc/slabinfo`.

### Add explicit “reclaim as type X” helpers

Yes. This is one of the highest-value additions for LLM consumption.

```c
int kpwn_reclaim_as_msg(struct kpwn_xcache_ctx *x,
                        struct kpwn_msg_spray *msg,
                        size_t n,
                        size_t cache_size,
                        const void *data);

int kpwn_reclaim_as_pipe(struct kpwn_xcache_ctx *x,
                         struct kpwn_pipe_spray *pipe,
                         size_t n,
                         size_t pipe_buffers);

int kpwn_reclaim_as_key(...);
int kpwn_reclaim_as_skb(...);
int kpwn_reclaim_as_pgv(...);
```

This lets an LLM write readable code:

```c
kpwn_xcache_begin(&xc);
kpwn_xcache_defrag_msg(&xc, ...);
trigger_uaf();
kpwn_xcache_free_victims(&xc, ...);
kpwn_reclaim_as_pipe(&xc, ...);
kpwn_xcache_end(&xc);
```

### CPU pinning

Add it.

SLUB behavior is heavily per-CPU, so cross-cache helpers should pin by default and restore the original affinity afterward. You already have `process_assign_to_core()` and `thread_assign_to_core()` in `flow.h`; wrap them in cross-cache context management rather than forcing the caller to remember.

Recommended flags:

```c
#define KPWN_XCACHE_PIN_CPU       (1u << 0)
#define KPWN_XCACHE_RESTORE_CPU   (1u << 1)
#define KPWN_XCACHE_VERBOSE_SLAB  (1u << 2)
```

### Page draining

`kpwn_drain_pages()` is too weak as a public contract.

Current problems:

* It creates one VMA per page.
* It assumes 4 KiB pages.
* It does not track partial success.
* It does not store mapping lengths.
* `MAP_POPULATE` is useful but not a complete “page drain succeeded” guarantee.

Prefer:

```c
struct kpwn_page_drain {
    void *addr;
    size_t len;
    size_t page_size;
    size_t pages_requested;
    size_t pages_touched;
};

int kpwn_page_drain_alloc(struct kpwn_page_drain *d, size_t pages);
int kpwn_page_drain_free(struct kpwn_page_drain *d);
```

Touch every page explicitly and log:

```text
[kpwn:crosscache] action=drain pages_req=4096 pages_touched=4096 page_size=4096 rc=0
```

---

## 3. `overwrite.h` design

### `kpwn_kwrite_fn` is the right abstraction

Yes. Keep it. It is the right low-level boundary because overwrite targets should not care whether the write primitive came from pipe buffers, dirty pagetable tricks, BPF, a device bug, or a CTF backdoor.

But change the type to use integer kernel addresses:

```c
typedef int (*kpwn_kwrite_fn)(uint64_t kaddr,
                              const void *data,
                              size_t len,
                              void *ctx);

typedef int (*kpwn_kread_fn)(uint64_t kaddr,
                             void *buf,
                             size_t len,
                             void *ctx);
```

Using `void *` for kernel addresses encourages accidental dereference and bad formatting. Also add optional verify helpers:

```c
int kpwn_kwrite_str(uint64_t kaddr,
                    size_t max_len,
                    const char *s,
                    kpwn_kwrite_fn write_fn,
                    void *ctx);

int kpwn_kwrite_str_verify(uint64_t kaddr,
                           size_t max_len,
                           const char *s,
                           kpwn_kwrite_fn write_fn,
                           kpwn_kread_fn read_fn,
                           void *ctx);
```

### `commit_creds(prepare_kernel_cred(0))`

Do **not** add this as an `overwrite.h` target.

It is not an overwrite target; it is a kernel-call/control-flow target. Put it in a separate `lpe.h`, `kcall.h`, or `creds.h` layer that requires the caller to provide an arbitrary-call primitive.

Recommended separation:

```c
typedef uint64_t (*kpwn_kcall_fn)(uint64_t fn,
                                  const uint64_t args[6],
                                  void *ctx);

int kpwn_lpe_commit_creds(struct kpwn_target *t,
                          kpwn_kcall_fn call_fn,
                          void *ctx);
```

For direct credential mutation, create `dirtycred.h` and require a target profile with explicit offsets. Do not provide a magical “overwrite current cred to root” helper without offsets, because `struct cred` layout, refcounts, namespaces, capabilities, and LSM fields are target-sensitive.

### `modprobe_path`

The existing helper is useful, but fix the trigger:

Current `kpwn_trigger_modprobe()` writes `sizeof(header)`, which writes five bytes because the array includes the trailing NUL. Write exactly four invalid magic bytes.

Also avoid `system(cmd)`. Use `fork()` + `execve(dummy_path, ...)` or direct `execve()` and ignore the expected failure. `system()` introduces shell quoting issues and makes logs less deterministic.

Recommended trigger:

```c
int kpwn_trigger_modprobe(const char *dummy_path,
                          unsigned flags);
```

With flags:

```c
#define KPWN_TRIGGER_CLEANUP_FILE  (1u << 0)
#define KPWN_TRIGGER_FCHMOD        (1u << 1)
```

### `core_pattern`

The overwrite helper is fine, but add a trigger helper if you keep this target:

```c
int kpwn_trigger_core_pattern(void);
```

It should fork a child and crash the child, then log whether the crash path was attempted. Do not hide system-specific core-dump restrictions; log them.

### `poweroff_cmd` and `uevent_helper`

Add these as **generic string overwrite targets**, but make availability explicit.

`poweroff_cmd` exists as a static 256-byte command buffer in current upstream reboot code, but because it is static, symbol discovery is target-dependent. `uevent_helper` is compiled only when `CONFIG_UEVENT_HELPER` is enabled. ([GitHub][7])

Recommended API:

```c
struct kpwn_overwrite_target {
    const char *name;
    uint64_t addr;
    size_t max_len;
    unsigned flags;
};

int kpwn_overwrite_string(const struct kpwn_overwrite_target *target,
                          const char *value,
                          kpwn_kwrite_fn write_fn,
                          kpwn_kread_fn read_fn_or_null,
                          void *ctx);
```

Then wrappers:

```c
int kpwn_overwrite_modprobe_path(...);
int kpwn_overwrite_core_pattern(...);
int kpwn_overwrite_poweroff_cmd(...);
int kpwn_overwrite_uevent_helper(...);
```

Each wrapper should log `available=0` or `addr=0` rather than pretending the target exists.

---

## 4. Overall architecture for LLM consumption

### Module split

The split is mostly good, but `kernel.h` is overloaded.

Current `kernel.h` includes:

* `kchecksec()`
* io_uring cred spray
* global `kbase`
* unsafe kernel function call helpers

Split it into:

| New header         | Contents                                                  |
| ------------------ | --------------------------------------------------------- |
| `inspect.h`        | `kpwn_inspect()`, `kpwn_kchecksec()`, target environment  |
| `target.h`         | `struct kpwn_target`, `kbase`, symbol/profile assumptions |
| `kcall.h`          | arbitrary kernel call abstractions                        |
| `credspray.h`      | io_uring personality / cred allocation helpers            |
| `spray.h`          | heap/page sprays                                          |
| `crosscache.h`     | orchestration                                             |
| `overwrite.h`      | string/data overwrite targets                             |
| `recipes.h`        | optional high-level recipes                               |
| `experimental/*.h` | physrw, usma, dirtycred, race helpers                     |

Keep `kpwn/kpwn.h` as the umbrella include.

### Add a target/profile object

This is probably the most important architecture improvement.

```c
struct kpwn_target {
    uint64_t kbase;
    const char *release;
    const char *arch;
    size_t page_size;

    size_t msg_msg_size;
    size_t msg_msgseg_size;
    size_t user_key_payload_hdr_size;
    size_t skb_shared_info_size;
    size_t pipe_buffer_size;

    unsigned flags;
};
```

Make helpers accept `struct kpwn_target *` where assumptions matter. This reduces LLM hallucinations because the model has a place to put “this exploit assumes kernel X with size Y.”

### Error handling

Use this policy:

* Library functions return `0` on success.
* Library functions return `-errno` on failure.
* Context structs expose `done` / `failed_at`.
* Fatal wrappers are named `*_or_die`.
* No `exit(1)` inside stable library functions.

Example:

```c
int kpwn_spray_msg_ex(struct kpwn_msg_spray *s);
void kpwn_spray_msg_or_die(struct kpwn_msg_spray *s);
```

`SYSCHK` is fine for exploit `main.c`, but not for reusable helpers that advertise return values.

### Recipe layer

Yes, add a recipe layer, but keep it explicit and auditable.

Good:

```c
int kpwn_recipe_modprobe_overwrite(struct kpwn_target *t,
                                   uint64_t modprobe_path_addr,
                                   const char *payload_path,
                                   kpwn_kwrite_fn write_fn,
                                   kpwn_kread_fn read_fn,
                                   void *ctx);
```

Bad:

```c
kpwn_root_me();
```

Recipes should not silently find symbols, choose sprays, overwrite globals, and trigger payloads in one opaque function. For LLM consumption, recipes should emit a stable plan:

```text
[kpwn:recipe] name=modprobe stage=overwrite target=modprobe_path addr=0xffffffff... rc=0
[kpwn:recipe] name=modprobe stage=trigger path=/tmp/x rc=0
```

### Naming conventions

New public APIs should all use `kpwn_*`.

Legacy names such as `kasld`, `hexdump`, `noaslr`, `win`, `inspect`, `kchecksec`, and `alloc_n_creds` should either become wrappers or be gated behind:

```c
#define KPWN_ENABLE_LEGACY_NAMES
```

Recommended renames:

| Current           | Recommended                  |
| ----------------- | ---------------------------- |
| `kasld()`         | `kpwn_kaslr_leak_prefetch()` |
| `kchecksec()`     | `kpwn_kchecksec()`           |
| `inspect()`       | `kpwn_inspect()`             |
| `alloc_n_creds()` | `kpwn_spray_uring_creds()`   |
| `hexdump()`       | `kpwn_hexdump()`             |
| `noaslr()`        | `kpwn_disable_user_aslr()`   |
| `kfunc_abs()`     | `kpwn_kcall_abs_unsafe()`    |
| `kfunc_off()`     | `kpwn_kcall_off_unsafe()`    |

The word `unsafe` is useful for LLMs. It discourages accidental use in normal code.

### Logging format

Adopt a strict key-value grammar:

```text
[kpwn:spray] kind=msg action=alloc n=256 done=256 payload_len=208 cache_goal=256 rc=0
[kpwn:crosscache] action=pin cpu=0 rc=0
[kpwn:overwrite] target=modprobe_path addr=0xffffffff82a3f180 len=12 verify=1 rc=0
[kpwn:error] func=kpwn_spray_key err=-ENOSPC failed_at=137
```

Avoid free-form strings in machine logs. Escape user-controlled strings.

---

## 5. Missing primitives for 2024-2025-style exploits

### `physrw.h`

Add it, but as a backend abstraction, not a single “do physical R/W” magic API.

Recommended design:

```c
struct kpwn_physrw {
    const char *backend;
    size_t page_size;
    uint64_t direct_map_base;
    void *ctx;
};

typedef int (*kpwn_phys_read_fn)(struct kpwn_physrw *p,
                                 uint64_t paddr,
                                 void *buf,
                                 size_t len);

typedef int (*kpwn_phys_write_fn)(struct kpwn_physrw *p,
                                  uint64_t paddr,
                                  const void *buf,
                                  size_t len);
```

Separate architecture helpers:

```c
struct kpwn_pte_desc {
    uint64_t raw;
    uint64_t pfn;
    uint64_t flags;
};

uint64_t kpwn_x86_64_phys_to_pte(uint64_t phys, uint64_t flags);
uint64_t kpwn_x86_64_pte_to_phys(uint64_t pte);
```

Do not bake in assumptions about `/proc/self/pagemap`, direct-map base, or page-table layout. Put those in `struct kpwn_target`.

### `usma.h` / PACKET_MMAP

Add it, but consider naming it `packet_mmap.h` with `usma.h` as an alias. `USMA` is less greppable than `packet_mmap`.

Current `kpwn_spray_pgv()` is too thin. PACKET_MMAP rings have explicit constraints: block size must be page-size aligned, frame size must satisfy packet header/alignment constraints, and `tp_frame_nr` must match frames-per-block times block count. The API should validate these before calling `setsockopt()`. ([Kernel Documentation][8])

Recommended:

```c
enum kpwn_packet_ring_kind {
    KPWN_PACKET_RX_RING,
    KPWN_PACKET_TX_RING,
};

struct kpwn_packet_ring {
    int fd;
    void *map;
    size_t map_len;
    enum kpwn_packet_ring_kind kind;
    unsigned version;
    struct tpacket_req req;
};

int kpwn_packet_ring_create(struct kpwn_packet_ring *r,
                            enum kpwn_packet_ring_kind kind,
                            unsigned version,
                            unsigned block_size,
                            unsigned block_nr,
                            unsigned frame_size,
                            unsigned flags);

int kpwn_packet_ring_mmap(struct kpwn_packet_ring *r);
int kpwn_packet_ring_free(struct kpwn_packet_ring *r);
```

Then `kpwn_spray_pgv()` can be a wrapper around many `kpwn_packet_ring` objects.

### `dirtycred.h`

Add it and move `alloc_n_creds()` there.

Recommended pieces:

```c
struct kpwn_cred_spray {
    int *ring_fds;
    size_t n;
    size_t done;
};

int kpwn_spray_uring_creds(struct kpwn_cred_spray *s, size_t n);
int kpwn_free_uring_creds(struct kpwn_cred_spray *s);
```

Also add file-object helpers:

```c
struct kpwn_file_spray {
    int *fds;
    size_t n;
    size_t done;
};

int kpwn_spray_files(struct kpwn_file_spray *s,
                     const char *path,
                     size_t n,
                     int flags);
```

For “dirty cred” style attacks, require an explicit layout profile:

```c
struct kpwn_cred_layout {
    size_t usage_off;
    size_t uid_off;
    size_t gid_off;
    size_t cap_effective_off;
    size_t cap_permitted_off;
    size_t security_off;
};
```

Do not hard-code a universal `struct cred` layout.

### Race helpers

Add `race.h`.

Keep it generic and dependency-free:

```c
struct kpwn_race_worker {
    pthread_t tid;
    int cpu;
    volatile int start;
    volatile int stop;
    uint64_t iterations;
    int last_errno;
};

typedef void *(*kpwn_race_fn)(void *);

int kpwn_race_start(struct kpwn_race_worker *w,
                    kpwn_race_fn fn,
                    void *arg,
                    int cpu);

int kpwn_race_stop(struct kpwn_race_worker *w);
int kpwn_race_join(struct kpwn_race_worker *w);
```

Add barriers and affinity helpers:

```c
struct kpwn_barrier;
int kpwn_barrier_init(struct kpwn_barrier *b, unsigned n);
int kpwn_barrier_wait(struct kpwn_barrier *b);
```

For io_uring races, do not depend on liburing. Continue using raw syscalls, but move the constants/wrappers out of `kernel.c` into `uring.h`.

For FUSE-based stalls, libfuse would violate the zero-external-deps rule. Either implement a minimal raw `/dev/fuse` helper in an optional `fuse_raw.h`, or document FUSE as an external recipe outside the core library. Userfaultfd stalls should be a separate helper gated by `kpwn_inspect()` fields, because unprivileged userfaultfd is frequently disabled.

---

## Concrete source-level fixes

### `src/spray.c`

* `kpwn_spray_msg()`:

  * Initialize all `qids[i] = -1` before spraying.
  * On failure, return `-errno` and set `done`.
  * Make `flags` either meaningful or remove it.
  * Add `kpwn_spray_msg_cache()` that accepts cache size and computes payload length.

* `kpwn_free_msg()`:

  * Do not use `SYSCHK` unconditionally during cleanup.
  * Continue freeing remaining queues even if one `msgctl()` fails.
  * Return the first negative errno.

* `kpwn_read_msg()`:

  * Log `errno` on failure.
  * Optionally add a non-destructive peek mode if useful for exploit debugging.

* `kpwn_spray_pipe()`:

  * Rename to `kpwn_open_pipes()` or make it also prime pipe buffers.
  * Add a helper to set pipe size where permitted.
  * `kpwn_write_pipe()` must handle partial writes and `EINTR`.

* `kpwn_spray_xattr()`:

  * Check every `setxattr()` return value.
  * Add `kpwn_free_xattr(path, n, prefix)`.
  * Let callers specify `name_prefix`.
  * Log `done`.

* `kpwn_spray_key()` / `kpwn_free_key()`:

  * Fix `KEYCTL_REVOKE`.
  * Add explicit revoke/unlink/invalidate modes.
  * Use unique descriptions with PID or caller prefix to avoid collisions.

* `kpwn_spray_skb()`:

  * Add `SO_SNDBUF` control.
  * Support nonblocking mode.
  * Track sent datagrams.
  * Add drain helper.

* `kpwn_spray_pgv()`:

  * Validate `block_size`, `frame_size`, and `frame_nr`.
  * Support RX/TX ring selection.
  * Return mappings, not only fds.
  * Clean up partially created sockets on failure.

### `src/crosscache.c`

* Rename `kpwn_defrag_msg(..., obj_size)` argument to `payload_len`, or change behavior to accept real cache size.
* Replace page-array drain with a tracked drain context.
* Add CPU pin/restore to cross-cache context.
* Add slab info query support.
* Add explicit reclaim helpers.

### `src/overwrite.c`

* Replace `void *kbase` / `void *target` with `uint64_t`.
* Replace asserts with `-EINVAL`.
* Make `write_fn == NULL` return `-EINVAL`.
* `kpwn_trigger_modprobe()` should write exactly four invalid bytes.
* Replace `system()` with `execve()` path.
* Add optional verification through `kpwn_kread_fn`.
* Add generic string overwrite target API, then implement wrappers.

### `src/kernel.c`

* Rename logs to `[kpwn:kchecksec]` and `[kpwn:credspray]`.
* `capsh` output violates single-line logging; escape or summarize.
* Move io_uring cred spray into `credspray.h` / `uring.h`.
* Move `kbase` into `struct kpwn_target`.
* Rename `kfunc_abs/off()` to explicitly unsafe names or gate under legacy compatibility.

---

## Final recommended public API shape

The stable layer should look like this:

```c
#include <kpwn/kpwn.h>

struct kpwn_target target;
kpwn_target_init(&target);
kpwn_inspect(&target);

struct kpwn_msg_spray msg;
kpwn_msg_spray_init_cache(&target, &msg, qids, 256, 256, flags);
kpwn_msg_spray_fill(&msg, payload, payload_len);

/* vulnerability trigger */

kpwn_msg_spray_free(&msg);
```

The recipe layer should look like this:

```c
struct kpwn_recipe_modprobe r = {
    .target = &target,
    .modprobe_path_addr = addr,
    .payload_path = "/tmp/x",
    .write = kwrite,
    .read = kread_or_null,
    .ctx = primitive_ctx,
};

kpwn_recipe_modprobe_run(&r);
```

The log should be enough for a model or harness to reconstruct what happened:

```text
[kpwn:target] arch=x86_64 release=6.x page_size=4096 rc=0
[kpwn:spray] kind=msg action=alloc n=256 done=256 cache_goal=256 payload_len=208 rc=0
[kpwn:overwrite] target=modprobe_path addr=0xffffffff82a3f180 len=8 verify=0 rc=0
[kpwn:recipe] name=modprobe stage=trigger path=/tmp/dummy rc=0
```

That is the right design direction: typed primitive APIs for simple exploit generation, a generic ops layer for orchestration, explicit target profiles for kernel-version-sensitive assumptions, and recipe helpers that are transparent rather than magical.

END_ORACLE_REVIEW libkpwn-design-review

[1]: https://github.com/torvalds/linux/blob/master/include/uapi/linux/keyctl.h?utm_source=chatgpt.com "linux/include/uapi/linux/keyctl.h at master · torvalds/linux"
[2]: https://github.com/torvalds/linux/blob/master/ipc/msgutil.c "linux/ipc/msgutil.c at master · torvalds/linux · GitHub"
[3]: https://github.com/torvalds/linux/blob/master/include/linux/msg.h "linux/include/linux/msg.h at master · torvalds/linux · GitHub"
[4]: https://github.com/torvalds/linux/blob/master/include/keys/user-type.h "linux/include/keys/user-type.h at master · torvalds/linux · GitHub"
[5]: https://github.com/torvalds/linux/blob/master/include/linux/skbuff.h "linux/include/linux/skbuff.h at master · torvalds/linux · GitHub"
[6]: https://github.com/torvalds/linux/blob/master/include/linux/pipe_fs_i.h "linux/include/linux/pipe_fs_i.h at master · torvalds/linux · GitHub"
[7]: https://github.com/torvalds/linux/blob/master/kernel/reboot.c?utm_source=chatgpt.com "linux/kernel/reboot.c at master · torvalds/linux"
[8]: https://docs.kernel.org/networking/packet_mmap.html "Packet MMAP — The Linux Kernel  documentation"
