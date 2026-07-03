#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
KVER="${KVER:-7.0.12-arch1-1}"
KERNEL="/usr/lib/modules/${KVER}/vmlinuz"
INITRAMFS="$SCRIPT_DIR/initramfs.cpio.gz"

if [ ! -f "$KERNEL" ]; then
  echo "ERROR: kernel not found at $KERNEL"
  echo "Set KVER= to match your installed kernel"
  exit 1
fi

if [ ! -f "$INITRAMFS" ]; then
  echo "ERROR: initramfs not found. Run ./build-rootfs.sh first"
  exit 1
fi

MODE="${1:-interactive}"

COMMON_ARGS=(
  -kernel "$KERNEL"
  -initrd "$INITRAMFS"
  -append "console=ttyS0 nokaslr panic=1 quiet"
  -m 512M
  -cpu host
  -enable-kvm
  -smp 2
  -nographic
  -no-reboot
  -monitor /dev/null
)

case "$MODE" in
  interactive)
    echo "[*] Starting interactive VM (Ctrl-A X to exit)"
    qemu-system-x86_64 "${COMMON_ARGS[@]}"
    ;;
  kaslr)
    # Test with KASLR enabled
    COMMON_ARGS_KASLR=("${COMMON_ARGS[@]}")
    # Replace nokaslr with nothing
    COMMON_ARGS_KASLR[5]="console=ttyS0 panic=1 quiet"
    echo "[*] Starting VM with KASLR enabled"
    qemu-system-x86_64 "${COMMON_ARGS_KASLR[@]}"
    ;;
  test)
    echo "[*] Running automated test (timeout 120s)"
    timeout 120 qemu-system-x86_64 "${COMMON_ARGS[@]}" \
      -append "console=ttyS0 nokaslr panic=1 autotest" \
      2>&1 | tee /tmp/vm-test.log
    echo ""
    echo "[*] Test output saved to /tmp/vm-test.log"
    grep -c '\[vm-test\] PASS' /tmp/vm-test.log || true
    grep '\[vm-test\] FAIL' /tmp/vm-test.log && echo " FAILURES ABOVE" || echo " no failures"
    ;;
  *)
    echo "Usage: $0 [interactive|kaslr|test]"
    exit 1
    ;;
esac
