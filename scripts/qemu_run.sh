#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

PARSER=./build/parser
if [ ! -x "$PARSER" ]; then
  make parser
fi

RISCVC=${RISCV64_GCC:-$(command -v riscv64-linux-gnu-gcc || true)}
if [ -z "$RISCVC" ]; then
  RISCVC=$(command -v riscv64-unknown-elf-gcc || true)
fi
if [ -z "$RISCVC" ]; then
  echo "Error: riscv64 cross-compiler not found. Install riscv64-linux-gnu-gcc or riscv64-unknown-elf-gcc." >&2
  exit 1
fi

QEMU=${QEMU_RISCV:-$(command -v qemu-riscv64 || true)}
if [ -z "$QEMU" ]; then
  echo "Error: qemu-riscv64 not found. Install QEMU with RISC-V support." >&2
  exit 1
fi

if [ $# -lt 1 ]; then
  cat <<'EOF' >&2
Usage: ./scripts/qemu_run.sh <source.c> [input-string]

This helper compiles <source.c> through the compiler pipeline, assembles the generated
RISC-V output, and executes it using QEMU.
EOF
  exit 1
fi

SRC_FILE=$1
shift
INPUT="${1:-}"

if [ ! -f "$SRC_FILE" ]; then
  echo "Error: source file '$SRC_FILE' not found." >&2
  exit 1
fi

# Generate RISC-V assembly from the compiler pipeline
"$PARSER" "$SRC_FILE" > /dev/null

# Assemble and link the generated assembly
TMP_EXE="$(mktemp /tmp/qemu_run_XXXXXX.elf)"
cleanup() {
  rm -f "$TMP_EXE"
}
trap cleanup EXIT
$RISCVC -static -o "$TMP_EXE" output.s

# Run the generated executable under QEMU
if [ -n "$INPUT" ]; then
  printf '%s' "$INPUT" | "$QEMU" "$TMP_EXE"
else
  "$QEMU" "$TMP_EXE"
fi
