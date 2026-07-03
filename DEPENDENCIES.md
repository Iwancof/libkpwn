# Dependencies

libkpwn itself needs only a C compiler and `make`. Everything below beyond the
"Core" row is optional and only matters for specific workflows.

| Tool | Needed for | Required? | Notes |
| --- | --- | --- | --- |
| `gcc` (or `clang`) + `make` | building the library, demo, tests | **yes** | C11-ish; `-Wall -Wextra` clean |
| — | unit tests | **yes (bundled)** | tests use `tests/ktest.h`; no external framework |
| `clang-format` | `make format` / `make format-check` / CI | for CI | LLVM style, see `.clang-format` |
| `aarch64-linux-gnu-gcc` | cross-building the aarch64 arch sources | optional | CI builds both arches |
| `musl-gcc` | building a static exploit binary from `template/` | optional | falls back to `gcc -static`; see `template/Makefile` |
| `upx` | shrinking the packed exploit (`cpmain`) | optional | packing only; skip if absent |
| `cpio`, `gzip` | repacking an initramfs (`rootfs` -> `.cpio.gz`) | optional | challenge-specific |
| `qemu-system-<arch>` | `tests/vm/` integration harness | optional | not a CI gate |
| `python3` + `pyelftools`, `vmlinux-to-elf` | `tools/gen-offsets.sh` (KSYM_* header) | optional | offset extraction from a `vmlinux` |

## Arch / hardware notes

- The prefetch KASLR side channel (`kasld`) has separate Intel and AMD code
  paths. Local integration testing here targets the **AMD** path (this dev
  machine is AMD); the Intel path is validated best-effort.
- Page-size assumptions: primitives that touch page tables assume 4 KiB pages
  and (on x86_64) 4-level paging unless noted.

## Arch Linux quick install

```sh
sudo pacman -S --needed base-devel clang qemu-base cpio
# optional:
sudo pacman -S --needed aarch64-linux-gnu-gcc musl upx
# python offset tooling:
pipx install vmlinux-to-elf pyelftools   # or use a venv
```
