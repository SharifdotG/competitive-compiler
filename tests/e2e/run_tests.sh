#!/usr/bin/env bash
# End-to-end test runner: for each tests/e2e/*.cl, compile, run with .stdin (if present),
# and diff stdout against .stdout.
set -u

cd "$(dirname "$0")/../.."

if [ ! -x ./cpc ]; then
    echo "cpc not built; run 'make' first" >&2
    exit 1
fi

declare -i pass=0 fail=0
failures=()

for prog in tests/e2e/*.cl; do
    name=$(basename "$prog" .cl)
    stdin_file="tests/e2e/${name}.stdin"
    stdout_file="tests/e2e/${name}.stdout"

    if [ ! -f "$stdout_file" ]; then continue; fi

    exe="/tmp/cpc-test-${name}"
    if ! ./cpc "$prog" -o "$exe" 2> /tmp/cpc-test-stderr; then
        printf "FAIL %-25s (compile failed)\n" "$name"
        cat /tmp/cpc-test-stderr >&2
        fail+=1
        failures+=("$name")
        continue
    fi

    if [ -f "$stdin_file" ]; then
        actual=$("$exe" < "$stdin_file" 2>&1)
    else
        actual=$("$exe" 2>&1)
    fi
    expected=$(cat "$stdout_file")

    if [ "$actual" = "$expected" ]; then
        printf "PASS %-25s\n" "$name"
        pass+=1
    else
        printf "FAIL %-25s\n" "$name"
        diff <(printf '%s' "$expected") <(printf '%s' "$actual") | head -20
        fail+=1
        failures+=("$name")
    fi
    rm -f "$exe"
done

echo
echo "------"
echo "passed: $pass    failed: $fail"
[ "$fail" -eq 0 ] || exit 1
