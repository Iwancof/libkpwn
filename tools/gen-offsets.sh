
#!/usr/bin/env bash
# filepath: /home/iwancof/WorkSpace/CTF/libkpwn/tools/gen-offsets.sh
# Generate kernel symbol offsets from vmlinux file
# Usage: gen-offsets.sh <vmlinux_path> <output_path>

set -euo pipefail

# Get the real script directory (resolve symlinks)
SCRIPT_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
PYTHON_HELPER="$SCRIPT_DIR/gen-offsets-helper.py"

usage() {
    echo "Usage: $0 <vmlinux_path> <output_path>"
    echo "Generate kernel symbol offsets header file from vmlinux"
    echo ""
    echo "Arguments:"
    echo "  vmlinux_path  Path to the vmlinux file"
    echo "  output_path   Path for the output offsets.h file"
    exit 1
}

# Check arguments
if [ $# -ne 2 ]; then
    echo "Error: Invalid number of arguments" >&2
    usage
fi

VMLINUX_PATH="$1"
OUTPUT_PATH="$2"

# Check if vmlinux file exists
if [ ! -f "$VMLINUX_PATH" ]; then
    echo "Error: vmlinux file not found: $VMLINUX_PATH" >&2
    exit 1
fi

# Check if vmlinux-to-elf is available
if ! command -v vmlinux-to-elf >/dev/null 2>&1; then
    echo "Error: vmlinux-to-elf tool not found in PATH" >&2
    exit 1
fi

# Check if Python helper exists
if [ ! -f "$PYTHON_HELPER" ]; then
    echo "Error: Python helper script not found: $PYTHON_HELPER" >&2
    exit 1
fi

echo "Processing vmlinux: $VMLINUX_PATH"

# Create temporary ELF file
TEMP_ELF=$(mktemp --suffix=.elf)
trap "rm -f '$TEMP_ELF'" EXIT

# Convert vmlinux to ELF format
echo "Converting vmlinux to ELF format..."
if ! vmlinux-to-elf "$VMLINUX_PATH" "$TEMP_ELF"; then
    echo "Error: Failed to convert vmlinux to ELF format" >&2
    exit 1
fi

# Generate offsets header
echo "Generating offsets header: $OUTPUT_PATH"
if ! /usr/bin/python3.13 "$PYTHON_HELPER" "$TEMP_ELF" > "$OUTPUT_PATH"; then
    echo "Error: Failed to generate offsets header" >&2
    exit 1
fi

echo "Successfully generated $OUTPUT_PATH"

