#!/bin/bash
# Week 4: IR - parse a file and get its IR, or run validation tests

cd "$(dirname "$0")/.."
PARSER=./build/parser

make -q parser 2>/dev/null || make parser

if [ -n "$1" ]; then
    # Parse single file, show IR (saved to ir.txt)
    $PARSER "$1"
else
    # Batch validation: pass/fail only (no ir.txt left behind)
    rm -f ir.txt
    PASS=0 FAIL=0
    for f in test/add.c test/test_good.c test/test_func_call.c test/continue.c test/test_struct.c test/test_struct_ptr.c; do
        if $PARSER "$f" > /dev/null 2>&1; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
    done
    if $PARSER test/test_bad.c > /dev/null 2>&1; then FAIL=$((FAIL+1)); else PASS=$((PASS+1)); fi
    echo "Passed: $PASS  Failed: $FAIL"
    [ $FAIL -eq 0 ] && exit 0 || exit 1
fi
