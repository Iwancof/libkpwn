# LLM Integration Guide

How to use libkpwn when an LLM is writing the exploit.

## Output conventions

All libkpwn output uses stable grep tags:

```
[kpwn:<module>] key=value ...
[kchecksec] field=value ...
[ktest] PASS/FAIL suite.name
```

To extract all library output from a log:
```sh
grep '\[kpwn:\|^\[kchecksec\]' exploit.log
```

## Recommended exploit skeleton

```c
#define _GNU_SOURCE
#include <kpwn/kpwn.h>

int main(int argc, char *argv[]) {
    // 1. Setup
    noaslr(argc, argv);
    log_level = LOG_DEBUG;  // Show everything during development
    process_assign_to_core(0);

    // 2. Recon
    kchecksec();  // Prints mitigation status with [kchecksec] tags

    // 3. KASLR break
    uint64_t kb = kasld();  // Auto-detects Intel/AMD
    if (!kb) { log_error("KASLR break failed"); return 1; }
    set_kbase((void *)kb);

    // 4. Heap spray + trigger
    // ... vulnerability-specific code ...

    // 5. Post-exploitation
    // kpwn_overwrite_modprobe(...) or trigger_corewin(...)
}
```

## Key APIs for LLM use

### Sizing spray objects

To hit a specific kmalloc cache, subtract the header:

```c
// Target kmalloc-256:
size_t payload = 256 - KPWN_MSG_HDR_SIZE;  // 256 - 48 = 208

// Target kmalloc-1024 with user_key_payload:
size_t payload = 1024 - KPWN_KEY_HDR_SIZE;  // 1024 - 24 = 1000
```

### Error handling

- `SYSCHK(expr)` — for syscalls returning int/ssize_t; exits on `< 0`
- `PTRCHK(expr)` — for pointer-returning calls; exits on `MAP_FAILED`/`NULL`
- `ASSERT(cond)` / `ASSERT_MSG(cond, msg)` — general assertions

All error paths print file:line and `strerror(errno)`, then `exit(1)`.

### Pack/unpack for payload construction

```c
char buf[0x100];
up64(0xdeadbeef, &buf[0]);   // Write 8 bytes LE
up64(kbase + 0x1234, &buf[8]); // Write kbase-relative address
uint64_t leaked = pc64(&buf[0]); // Read 8 bytes LE
```

### Hexdump for inspection

```c
hexdump(log_info, ptr, 0x80);
// Output:
// [ INFO ] > 0x...: 41 42 43 44 ...|ABCD...|
```

## Non-interactive design

libkpwn has **no interactive prompts**. All functions return immediately.
There is no `getc(stdin)`, no `fgets(stdin)`, no `system("/bin/sh")` in the
library path. An LLM can call any API without risk of blocking.

The old `interactive()` REPL and `WAIT` macro have been removed.
