# libkpwn

A C helper library for Linux kernel exploitation, designed to be used by
**LLMs writing kernel exploits**. Single-include, zero external dependencies,
greppable output.

## Quick start

```c
#include <kpwn/kpwn.h>

int main(int argc, char *argv[]) {
    noaslr(argc, argv);

    // Survey the target kernel
    kchecksec();

    // Break KASLR (prefetch side-channel; AMD/Intel auto-detected)
    uint64_t kb = kasld();
    set_kbase((void *)kb);

    // Spray msg_msg objects into kmalloc-256
    int qids[256];
    char payload[256 - KPWN_MSG_HDR_SIZE];
    memset(payload, 'A', sizeof(payload));
    kpwn_spray_msg(qids, 256, payload, sizeof(payload), 0);

    // ... trigger your vulnerability here ...

    kpwn_free_msg(qids, 256);
}
```

## Using libkpwn in your project

Point your Makefile at libkpwn and include `kpwn.mk`:

```makefile
CC      := musl-gcc          # or gcc
CFLAGS  := -static -Os
LIBKPWN := ../libkpwn        # relative path to your checkout
include $(LIBKPWN)/kpwn.mk

exploit: main.c $(KPWN_SRCS)
	$(CC) $(CFLAGS) $(KPWN_CFLAGS) $^ -o $@
```

`kpwn.mk` auto-detects the target architecture, selects the right source
files, and supplies `-I` flags. No absolute paths, no install step.

## Modules

| Header | Purpose |
| --- | --- |
| `kpwn/kpwn.h` | Umbrella — includes everything below |
| `kpwn/logger.h` | Leveled logging (`log_debug/info/warn/error/success`) |
| `kpwn/utils.h` | Pack/unpack, REP loop, SYSCHK/PTRCHK, file helpers |
| `kpwn/hexdump.h` | Colored hexdump with greppable output |
| `kpwn/memory.h` | `vmmap()`, `virt2phys()`, mmap helpers |
| `kpwn/slog.h` | Logged syscall wrappers (`dmmap`, `dmunmap`, `dmremap`) |
| `kpwn/flow.h` | `noaslr()`, `win()`, `grazing()`, core pinning, billy/core_pattern LPE |
| `kpwn/kernel.h` | `kchecksec()`, `alloc_n_creds()`, `kasld()`, `kfunc_abs/off()` |
| `kpwn/spray.h` | Heap spray: msg_msg, pipe_buffer, setxattr, add_key, sk_buff, pgv |
| `kpwn/crosscache.h` | Cross-cache attack orchestration (drain, defrag, reclaim) |
| `kpwn/overwrite.h` | Post-exploitation targets: modprobe_path, core_pattern overwrite |
| `kpwn/x86_64/*.h` | CPU state, PTE forge, prefetch side-channel KASLR |
| `kpwn/aarch64/*.h` | PAC (pointer authentication) forgery |

## LLM-friendly design

All library output follows a greppable format:

```
[kpwn:spray] msg_msg n=256 data_len=208
[kchecksec] KASLR=on
[kpwn:kbase] kbase=0xffffffff81000000
```

- Default log level is `LOG_INFO` (not `LOG_ERROR`) — output is visible by default
- No interactive prompts or blocking I/O
- Single-line output per operation for easy parsing
- `grep '\[kpwn:'` extracts all library output
- `grep '\[kchecksec\]'` extracts security posture

## Building

```sh
make lib       # build libkpwn.a
make test      # run unit tests (zero deps)
make demo      # build the demo binary
make format    # clang-format all sources
```

Cross-build for aarch64: `make lib CC=aarch64-linux-gnu-gcc`

## Dependencies

See [DEPENDENCIES.md](DEPENDENCIES.md) for the full list. The core library
needs only `gcc` + `make`. Tests are bundled (no Criterion or other framework).

## License

See the repository for license details.
