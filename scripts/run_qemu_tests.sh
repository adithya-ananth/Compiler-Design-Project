#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

TEST_HELPER=./scripts/qemu_run.sh
if [ ! -x "$TEST_HELPER" ]; then
  chmod +x "$TEST_HELPER" 2>/dev/null || true
fi

if [ ! -f "$TEST_HELPER" ]; then
  echo "Error: helper script '$TEST_HELPER' not found." >&2
  exit 1
fi

if [ $# -gt 0 ]; then
  case "$1" in
    --list)
      echo "factorial_tail_recursive"
      exit 0
      ;;
    *)
      echo "Usage: $0 [--list]" >&2
      exit 1
      ;;
  esac
fi

PASS=0
FAIL=0

run_test() {
  local src="$1"
  local input="$2"
  local expected="$3"
  local name="$4"

  printf "Running %s... " "$name"
  local actual
  if ! actual=$(bash "$TEST_HELPER" "$src" "$input" 2>/dev/null); then
    echo "FAIL (execution error)"
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$actual" = "$expected" ]; then
    echo "PASS"
    PASS=$((PASS + 1))
  else
    echo "FAIL"
    echo "Expected: $expected"
    echo "Actual:   $actual"
    FAIL=$((FAIL + 1))
  fi
}

# Regression test for tail-recursive factorial bug.
# The program should compute 7! = 5040.
run_test "test/complex/factorial_tail_recursive.c" "7\n" "Enter a positive integer: Tail-Recursive Factorial is 5040" "factorial_tail_recursive"

printf "\nResults: Passed=%d  Failed=%d\n" "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
