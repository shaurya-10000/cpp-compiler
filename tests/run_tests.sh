#!/usr/bin/env bash
set -u
MINICC="${1:-./build/minicc}"
pass=0
fail=0

# Programs that must compile, run, and print exactly tests/<name>.expected
for src in tests/*.cpp; do
  name="$(basename "$src" .cpp)"
  tmp="$(mktemp -d)"
  if ! "$MINICC" --compile "$src" > "$tmp/o.s" 2> "$tmp/err"; then
    echo "FAIL $name (compile)"; cat "$tmp/err"; fail=$((fail+1)); continue
  fi
  if ! gcc "$tmp/o.s" -o "$tmp/o" 2> "$tmp/gccerr"; then
    echo "FAIL $name (assemble/link)"; cat "$tmp/gccerr"; fail=$((fail+1)); continue
  fi
  "$tmp/o" > "$tmp/out.txt" || true
  if diff -u "tests/$name.expected" "$tmp/out.txt"; then
    echo "PASS $name"; pass=$((pass+1))
  else
    echo "FAIL $name (output mismatch)"; fail=$((fail+1))
  fi
done

# Programs that must be rejected; line 1 is "// ERROR: <expected message substring>"
for src in tests/errors/*.cpp; do
  [ -e "$src" ] || continue
  name="errors/$(basename "$src" .cpp)"
  tmp="$(mktemp -d)"
  want="$(sed -n '1s#^// ERROR: ##p' "$src" | tr -d '\r')"
  if "$MINICC" --compile "$src" > /dev/null 2> "$tmp/err"; then
    echo "FAIL $name (expected a compile error)"; fail=$((fail+1))
  elif grep -qF -- "$want" "$tmp/err"; then
    echo "PASS $name"; pass=$((pass+1))
  else
    echo "FAIL $name (wrong error)"; echo "  wanted: $want"; echo "  got:    $(cat "$tmp/err")"; fail=$((fail+1))
  fi
done

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]