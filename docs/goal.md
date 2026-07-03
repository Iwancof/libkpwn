# libkpwn — 実装すべき機能ロードマップ

## 設計原則（再掲）
- LLM がカーネル exploit を書くためのライブラリ
- 1呼び出しで高情報、非対話、greppable `[kpwn:*]` 出力
- 依存ゼロ（libc + linux headers のみ）
- x86_64 主軸、aarch64 は best-effort

## 現在の実装状態

### 実機検証済み ✅
- `kasld()` — prefetch side-channel KASLR bypass（AMD Zen + Intel Core Ultra で正確一致）
- `kchecksec()` — カーネルセキュリティ姿勢の取得
- `alloc_n_creds()` — io_uring cred spray
- `kfunc_abs/off()` — 任意カーネル関数呼び出し
- spray: msg_msg / pipe_buffer / sk_buff / add_key / crosscache drain（データ照合済み）
- hexdump / vmmap / pack-unpack / noaslr

### 未検証だが実装済み
- setxattr spray（xattr対応FS上で未テスト）
- pgv spray（CAP_NET_RAW 必要）
- overwrite helpers（modprobe_path/core_pattern — root + kwrite primitive 必要）

---

## Priority 1: レース拡大ヘルパ (`race.h`)

### 背景
userfaultfd は `perf_event_paranoid >= 1` + user namespace 制限で多くの環境で使用不能。
FUSE はサンドボックス環境（Chrome, kCTF）で不可。
2024-2025 の exploit は以下の代替手法を使う:

### 実装項目

#### 1a. timerfd + epoll watch pile
- 原理: 大量の epoll watch を登録し、kworker のワークキューを飽和させてレース窓を拡大
- CVE-2024-50264 (a13xp0p0v) で実証: 8 procs × 500 epoll_create × 100 dup = 400k watches → ~80× window
- API案:
```c
struct kpwn_race_slowdown {
    int *epoll_fds;
    int timerfd;
    size_t n_procs;
    size_t n_epolls;
    size_t n_dups;
    pid_t *children;
};
int kpwn_race_timerfd_start(struct kpwn_race_slowdown *s,
                            size_t n_procs, size_t n_epolls, size_t n_dups);
int kpwn_race_timerfd_stop(struct kpwn_race_slowdown *s);
```

#### 1b. mincore() + mprotect() stall
- 原理: 巨大 anonymous VMA (128 MiB) を zeropage で埋め、mprotect() で全 PTE を走査させて 500-1000ms の遅延
- Project Zero MSG_OOB で使用
- API案:
```c
struct kpwn_race_mprotect_stall {
    void *vma;
    size_t vma_size;
};
int kpwn_race_mprotect_stall_init(struct kpwn_race_mprotect_stall *s, size_t size_mb);
int kpwn_race_mprotect_stall_trigger(struct kpwn_race_mprotect_stall *s);
int kpwn_race_mprotect_stall_free(struct kpwn_race_mprotect_stall *s);
```

#### 1c. 汎用レースワーカー
- pthread ベース、CPU ピン付き、start/stop barrier
```c
struct kpwn_race_worker {
    pthread_t tid;
    int cpu;
    volatile int go;
    volatile int stop;
    uint64_t iterations;
};
typedef void *(*kpwn_race_fn)(void *);
int kpwn_race_start(struct kpwn_race_worker *w, kpwn_race_fn fn, void *arg, int cpu);
int kpwn_race_barrier_wait(struct kpwn_race_worker *workers, size_t n);
int kpwn_race_stop_all(struct kpwn_race_worker *workers, size_t n);
```

---

## Priority 2: pipe_buffer 高レベル exploit API (`pipe_exploit.h`)

### 背景
deep-research で pipe_buffer が「万能 exploit オブジェクト」と判明:
- KASLR leak: `ops` @ offset 0x10 → `anon_pipe_buf_ops`（kernel .data）
- RIP 制御: `ops->release()` on close()
- Dirty-Pipe: `PIPE_BUF_FLAG_CAN_MERGE` でページ上書き
- cross-cache reclaim: 4096B pipe page で full R/W

### 実装項目
```c
// KASLR leak via pipe_buffer ops pointer (requires prior OOB read)
uint64_t kpwn_pipe_leak_ops(const void *oob_data, size_t oob_offset);
uint64_t kpwn_pipe_ops_to_kbase(uint64_t ops_addr, uint64_t anon_pipe_buf_ops_offset);

// Dirty-Pipe: overwrite a page via PIPE_BUF_FLAG_CAN_MERGE
int kpwn_pipe_dirty_write(int pipe_fd[2], const void *data, size_t len,
                          loff_t target_offset);

// pipe_buffer struct layout constants
#define KPWN_PIPE_BUF_PAGE_OFF     0x00
#define KPWN_PIPE_BUF_OFFSET_OFF   0x08
#define KPWN_PIPE_BUF_LEN_OFF      0x0c
#define KPWN_PIPE_BUF_OPS_OFF      0x10
#define KPWN_PIPE_BUF_FLAGS_OFF    0x18
#define KPWN_PIPE_BUF_PRIVATE_OFF  0x20
#define KPWN_PIPE_BUF_SIZE         0x28

#define KPWN_PIPE_BUF_FLAG_CAN_MERGE 0x10
```

---

## Priority 3: `struct kpwn_target` — カーネルプロファイル集約

### 背景
oracle レビューで「最も重要なアーキテクチャ改善」と指摘。
LLM が exploit を書くとき、カーネルバージョン依存の前提を明示する場所が必要。

### 実装項目
```c
struct kpwn_target {
    uint64_t kbase;
    const char *release;      // uname -r
    const char *arch;          // x86_64 / aarch64
    size_t page_size;

    // struct sizes (kernel-version dependent)
    size_t msg_msg_hdr;        // typically 0x30
    size_t msg_msgseg_hdr;     // typically 0x08
    size_t pipe_buffer_size;   // typically 0x28
    size_t skb_shared_info;    // typically 320
    size_t user_key_hdr;       // typically 0x18
    size_t cred_size;          // typically 0xa8-0xc0

    struct kchecksec_t checksec;
};

int kpwn_target_init(struct kpwn_target *t);  // auto-detect from running kernel
int kpwn_target_print(struct kpwn_target *t); // dump all fields as [kpwn:target] ...

// Helper: compute payload size for a target cache
size_t kpwn_msg_payload_for_cache(struct kpwn_target *t, size_t cache_size);
size_t kpwn_key_payload_for_cache(struct kpwn_target *t, size_t cache_size);
```

---

## Priority 4: kpwn_report() — 1発環境ダンプ

### 背景
LLM が exploit の冒頭で呼んで、ターゲット環境を把握するための非対話版。

### 実装項目
```c
int kpwn_report(void);
```
出力:
```
[kpwn:report] arch=x86_64 kernel=7.0.11-arch1-1
[kpwn:report] kaslr=on kbase=0xffffffff8fe00000
[kpwn:report] smep=1 smap=1 kpti=0 seccomp=0
[kpwn:report] userfaultfd=0 io_uring=0 bpf_disabled=2
[kpwn:report] uid=1000 euid=1000
[kpwn:report] page_size=4096
```

---

## Priority 5: PTE spray → 物理 R/W (`dirty_pt.h`)

### 背景
deep-research で「Dirty Pagetable/Dirty Pagedirectory は 2024-2025 の最強 primitive」と判明。
既存の `phy_to_pte`/`pte_to_phy` 部品の上に完成 primitive を組む。

### 実装項目
```c
// PTE spray: 巨大 read-only VMA + read fault で大量の PTE を割り当て
struct kpwn_pte_spray {
    void *vma;
    size_t vma_size;
    size_t pages_touched;
};
int kpwn_pte_spray_init(struct kpwn_pte_spray *s, size_t vma_size_mb);
int kpwn_pte_spray_touch(struct kpwn_pte_spray *s);
int kpwn_pte_spray_free(struct kpwn_pte_spray *s);

// Self-referential PTE: forge a PTE that points to its own page table
// → arbitrary physical R/W
struct kpwn_physrw {
    uint64_t pte_page_phys;   // physical addr of the controlled page table
    void *user_mapping;        // userland VA that maps to the PTE page
    size_t page_size;
};
int kpwn_physrw_init(struct kpwn_physrw *p, uint64_t controlled_phys_page,
                     kpwn_kwrite_fn write_fn, void *ctx);
int kpwn_physrw_read(struct kpwn_physrw *p, uint64_t paddr, void *buf, size_t len);
int kpwn_physrw_write(struct kpwn_physrw *p, uint64_t paddr, const void *data, size_t len);
```

---

## Priority 6: slab query (`slab.h`)

### 背景
cross-cache 攻撃の計画に objs_per_slab / slab order / active objects が必要。

### 実装項目
```c
struct kpwn_slab_info {
    char name[64];
    size_t object_size;
    size_t objs_per_slab;
    size_t slab_order;
    size_t active_objs;
    size_t total_slabs;
    size_t partial_slabs;
};
int kpwn_slab_query(const char *cache_name, struct kpwn_slab_info *out);
int kpwn_slab_dump(void);  // dump all caches as [kpwn:slab] lines
```

---

## Priority 7: cross-cache 強化

- `kpwn_xcache_begin/end` に CPU ピンを内蔵
- `kpwn_reclaim_as_pipe/msg/key/skb/pte` ヘルパ
- slab query ベースの drain 量自動計算

---

## 評価基準（oracle レビュー用）

このロードマップを以下の観点でレビューしてほしい:
1. **優先順位は正しいか？** 実際の kernel exploit ワークフローで最も頻繁に必要になるのは何か
2. **API 設計は LLM-friendly か？** 関数名・引数・構造体は LLM が正しく使えるか
3. **漏れている重要テクニックはないか？** 2024-2025 の kernelCTF/zero-day で使われていて上記にないもの
4. **過剰な抽象化はないか？** CTF の使い捨て exploit には過剰で、シンプルな関数の方が良い箇所
5. **struct kpwn_target のフィールド**: 足りないもの、不要なもの
