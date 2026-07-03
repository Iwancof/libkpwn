# libkpwn v2 Design — Oracle + Deep Research 統合

Oracle (GPT-5.5 Pro) と Deep Research (105 agents, 23 sources, 24/25 claims confirmed)
の知見を統合した次期設計。

## Deep Research: 2024-2025 最重要テクニック (adversarially verified)

### 1. Cross-cache が支配的 primitive (3-0 confirmed)
- SLUB hardening (RANDOM_KMALLOC_CACHES, SLAB_BUCKETS) を**バイパスし、むしろ安定化させる**
- SLAB_VIRTUAL のみが対策だが**未マージ** (2025時点)
- 実証済みキャッシュペア:
  - `net_device` (kmalloc-cg-4k) → `packet_fanout` (kmalloc-4k)
  - `io_uring struct file` (filp cache) → pipe page
  - `virtio_vsock_sock` (kmalloc-96) → `msg_msg` (msg_msg-96)
- SLUB drain 定数: OBJS_PER_SLAB=16, CPU_PARTIAL=52, OVERFLOW_FACTOR=2

### 2. pipe_buffer が万能オブジェクト (3-0 confirmed)
- KASLR leak: `ops` @ offset 0x10 → `anon_pipe_buf_ops` (kernel .data)
- RIP制御: `ops->release()` on close()
- arbitrary free: `msg.security` フィールド経由
- Dirty-Pipe: `PIPE_BUF_FLAG_CAN_MERGE` でページ上書き
- cross-cache reclaim target: 4096B pipe page で full R/W

Layout: `page@0x00, offset+len@0x08, ops@0x10, flags@0x18, private@0x20` (40B)
16-slot ring → kmalloc-cg-1k (GFP_KERNEL_ACCOUNT)

### 3. msg_msg/msg_msgseg が主力 grooming/OOB primitive (3-0 confirmed)
- Header: 6 × u64 = 0x30 (m_list_next, m_list_prev, m_type, m_ts, next, security)
- msg_msgseg header: 0x08
- OOB read: m_ts=0x1000 + MSG_COPY → memcpy (bypass HARDENED_USERCOPY)
- unaligned write: チャンク境界 - sizeof(msg_msg) に配置して隣接上書き

### 4. KASLR: prefetch より固定アドレス DATA leak (3-0 confirmed)
- cpu_entry_area / IDT @ 0xfffffe0000000000 (非ランダム化)
- vmsplice vDSO → pipe_buffer offset 0x28 に struct page アドレス
- pipe_buffer ops leak → anon_pipe_buf_ops
- **ARM64 KASLR bypass は今回エビデンス無し**

### 5. userfaultfd/FUSE 代替 race helper (3-0 confirmed)
- timerfd+epoll: 8 procs × 500 epoll_create × 100 dup = 400k watches → ~80× window拡大
- mincore()+mprotect: 128MiB zeropage VMA → 500-1000ms 遅延

### 6. Page-level UAF → PTE spray / kernel stack reclaim (3-0 confirmed)
- freed slab page を PTE として reclaim → self-referential PTE → 任意物理 R/W
- freed slab page を kernel stack として reclaim → clone()
- Dirty Pagedirectory: PTE+PMD を同一物理ページにエイリアス

## Oracle: API 設計改善 (GPT-5.5 Pro review)

### 即時修正 ✅ (適用済み)
- `kpwn_free_key()`: KEYCTL_REVOKE = 3 (was 0)
- `kpwn_trigger_modprobe()`: 4バイト書き込み + fork/execve
- `KPWN_MSG_KMALLOC_CG` flag: 削除

### 次期実装ロードマップ

#### Priority 1: `struct kpwn_target` 導入
```c
struct kpwn_target {
    uint64_t kbase;
    const char *release;
    const char *arch;
    size_t page_size;
    size_t msg_msg_hdr_size;    // 0x30
    size_t pipe_buffer_size;    // 0x28
    size_t skb_shared_info_size;// 320
    unsigned flags;
};
```

#### Priority 2: spray 構造体化 + done tracking
```c
struct kpwn_msg_spray {
    int *qids;
    size_t n, done;
    size_t payload_len;
};
```

#### Priority 3: `kpwn_kwrite_fn` を `uint64_t` に
```c
typedef int (*kpwn_kwrite_fn)(uint64_t kaddr, const void *data, size_t len, void *ctx);
typedef int (*kpwn_kread_fn)(uint64_t kaddr, void *buf, size_t len, void *ctx);
```

#### Priority 4: `race.h` — レースワーカー + バリア
```c
struct kpwn_race_worker { pthread_t tid; int cpu; volatile int start, stop; };
int kpwn_race_start(struct kpwn_race_worker *w, kpwn_race_fn fn, void *arg, int cpu);
```
2種のレースヘルパ:
- timerfd+epoll watch pile (~80× window)
- mincore()+mprotect (500-1000ms delay)

#### Priority 5: KASLR facade に fixed-address leak 追加
- cpu_entry_area IDT read (x86_64, 非ランダム, 0xfffffe0000000000)
- pipe_buffer ops leak
- vmsplice vDSO struct page leak

#### Priority 6: cross-cache orchestration 強化
- CPU ピン + slab query (`/sys/kernel/slab/`)
- reclaim-as-type ヘルパ (msg, pipe, key, skb, pgv, **PTE**, **kstack**)
- SLUB drain 定数のプロファイリング

#### Priority 7: Dirty Pagetable primitive
- page-level UAF → PTE spray → self-referential PTE → 任意物理 R/W
- PTE+PMD aliasing (Dirty Pagedirectory)

## Sources (23 fetched, top-tier)
- kernelCTF: CVE-2023-3390, CVE-2023-4622, CVE-2024-26808, CVE-2024-50264
- Zero-day: CVE-2024-1086 (pwning.tech), CVE-2025-38236 (Project Zero MSG_OOB)
- Blog: h0mbre kCTF data-only, a13xp0p0v CVE-2024-50264
- Academic: SLUBStick (USENIX'24), CROSS-X (CCS'25), PCP Massaging (NDSS'26), Page Spray (USENIX'24)
