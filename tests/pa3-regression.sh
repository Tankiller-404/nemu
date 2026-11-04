#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
for name in add fact if-else max mul-longlong quick-sort struct to-lower-case add-longlong fib integral min3 pascal select-sort sub-longlong wanshu bit gotbaha leap-year mov-c prime shuixianhua sum bubble-sort hello-str matrix-mul-small movsx quadratic-eq string switch; do
    if ! printf 'c\nq\n' | timeout 60s ./obj/nemu/nemu "obj/testcase/$name" > /tmp/pa3-case.log 2>&1; then
        cat /tmp/pa3-case.log; exit 1
    fi
    if ! grep -q 'HIT GOOD TRAP' /tmp/pa3-case.log; then cat /tmp/pa3-case.log; exit 1; fi
    echo "PASS $name"
done
echo 'PA3 regression: 30/30 PASS'
