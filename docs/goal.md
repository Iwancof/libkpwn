# libkpwn — 実装ロードマップ v2 (oracle reviewed)

## 優先順位（oracle 推奨順に修正）

### P1: `kpwn_target` + `kpwn_report()` — 全ての土台
- `struct kpwn_target`: kbase/arch/page_size/symbols/offsets/surfaces/confidence
- `kpwn_target_init(t, flags)` で自動検出、`kpwn_target_load_profile()` で手動上書き
- `kpwn_report(t, flags)` で環境ダンプ（uid/caps/namespaces/slab/uffd 等）
- 全フィールドに `enum kpwn_value_source` (autodetect/kallsyms/btf/assumed/manual)

### P2: slab query + cross-cache 強化
- `kpwn_slab_query_object(type, payload_len, out)` — オブジェクト型からキャッシュ推定
- `kpwn_xcache_plan_auto(target, src, dst, plan)` — drain/defrag 量自動計算
- `kpwn_reclaim_as_msg/pipe/key/skb/pte/kstack` — reclaim ヘルパ
- CPU ピン内蔵 + affinity 復元

### P3: pipe_buffer 高レベル API
- `kpwn_pipebuf_parse_leak()` — OOB data から pipe_buffer 構造体パース
- `kpwn_target_kbase_from_anon_pipe_ops()` — ops → kbase 変換
- `kpwn_pipe_can_merge_write_file()` — Dirty-Pipe style ページ上書き
- pipe_buffer layout 定数（ops@0x10, flags@0x18, etc）

### P4: Dirty Pagetable / physrw → `page_primitive.h`
- `kpwn_pte_make/get_pfn/set_pfn` — PTE 構築ヘルパ
- `kpwn_pte_spray_alloc/touch/free` — PTE spray
- `kpwn_physrw_from_mapped_pte()` — マップ済み PTE ページから物理 R/W
- `kpwn_kstack_reclaim_page()` — clone() でスタックページ reclaim

### P5: race helpers
- `kpwn_epoll_pile_init/arm/destroy` — timerfd+epoll (~80× window)
- `kpwn_usercopy_stall_init/trigger/free` — mprotect stall (500-1000ms)
- `kpwn_race_run(n, fn, arg, opts)` — 1行レースワーカー

### P6: 追加 spray objects + surfaces
- `kpwn_spray_tty/timerfd/eventfd/seqfile` — 追加 spray 種
- `kpwn_userns_enter/netns_enter/ns_setup_net_admin` — namespace setup
- `kpwn_bpf_probe()` — BPF JIT/exploit 可否検出
- `kpwn_iouring_setup_many/register_pbuf_ring` — io_uring lifecycle

### P7: data-only attack helpers
- `kpwn_cred_patch_uid0(cred_addr, write_fn, ctx)` — cred 直接上書き
- `kpwn_file_replace_f_cred(file_addr, cred_addr, write_fn, ctx)` — DirtyCred
- `kpwn_kaslr_from_pipe_ops/from_func_ptr/try_all` — KASLR facade

## 検証方針
- 全実装は `make test` (ktest) + `make smoke` (実カーネル) + QEMU VM テストで検証
- KASLR はベアメタルで検証（VM 内は KVM passthrough で host kbase が漏れる）
