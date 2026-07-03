#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOTFS="$SCRIPT_DIR/rootfs"
INITRAMFS="$SCRIPT_DIR/initramfs.cpio.gz"
LIBKPWN="$SCRIPT_DIR/../.."

echo "[*] Building rootfs at $ROOTFS"
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,sbin,dev,proc,sys,tmp,etc,lib/modules,exploit}

# Install busybox
cp "$(which busybox)" "$ROOTFS/bin/busybox"
cd "$ROOTFS/bin"
for cmd in sh ls cat echo mkdir mount umount insmod rmmod \
           dmesg grep sed awk chmod chown mknod sleep \
           id whoami ps kill sysctl head tail wc; do
  ln -sf busybox "$cmd"
done
cd "$SCRIPT_DIR"

# Copy vulnerable module
cp "$SCRIPT_DIR/vuln_module/vuln.ko" "$ROOTFS/lib/modules/"

# Build the libkpwn smoke test binary (static, for the VM)
echo "[*] Building exploit test binary"
EXPLOIT_SRC="$SCRIPT_DIR/exploit_test.c"
gcc -static -O0 -ggdb3 -Wall -Wextra \
    -I"$LIBKPWN/include" \
    "$EXPLOIT_SRC" \
    "$LIBKPWN"/src/crosscache.c "$LIBKPWN"/src/flow.c \
    "$LIBKPWN"/src/hexdump.c "$LIBKPWN"/src/kernel.c \
    "$LIBKPWN"/src/logger.c "$LIBKPWN"/src/memory.c \
    "$LIBKPWN"/src/overwrite.c "$LIBKPWN"/src/slog.c \
    "$LIBKPWN"/src/spray.c "$LIBKPWN"/src/utils.c \
    "$LIBKPWN"/src/x86_64/cpu.c "$LIBKPWN"/src/x86_64/memory.c \
    "$LIBKPWN"/src/x86_64/side_channel.c \
    "$LIBKPWN"/src/x86_64/side_channel.s \
    "$LIBKPWN"/src/x86_64/win.s \
    -o "$ROOTFS/exploit/test" -lpthread

# Init script
cat > "$ROOTFS/init" << 'INIT'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs devtmpfs /dev
mount -t tmpfs tmpfs /tmp

# Relax security for testing
echo 0 > /proc/sys/kernel/kptr_restrict
echo 0 > /proc/sys/kernel/perf_event_paranoid
echo 1 > /proc/sys/vm/unprivileged_userfaultfd 2>/dev/null || true

# Load vulnerable module
insmod /lib/modules/vuln.ko
chmod 666 /dev/vuln

echo "========================================="
echo " libkpwn test VM ($(uname -r))"
echo " /dev/vuln loaded, kptr_restrict=0"
echo "========================================="

# If "autotest" is in cmdline, run test and exit
if grep -q autotest /proc/cmdline; then
  /exploit/test
  echo "AUTOTEST_EXIT_CODE=$?"
  poweroff -f
fi

# Otherwise drop to interactive shell
echo " Run: /exploit/test"
setsid cttyhack sh
poweroff -f
INIT
chmod +x "$ROOTFS/init"

# Pack initramfs
echo "[*] Packing initramfs"
cd "$ROOTFS"
find . -print0 | cpio -o --null --format=newc 2>/dev/null | gzip > "$INITRAMFS"
echo "[+] Done: $INITRAMFS ($(du -h "$INITRAMFS" | cut -f1))"
