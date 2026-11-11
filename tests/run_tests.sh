#!/usr/bin/env bash
# usage: tests/run_tests.sh [path-to-minicc] [-O0|-O1]
set -u
MINICC="${1:-./build/minicc}"
OPT="${2:--O1}"
pass=0
fail=0
echo "== optimization level: $OPT =="

# Programs that must compile, run, and print exactly tests/<name>.expected
for src in tests/*.cpp; do
  name="$(basename "$src" .cpp)"
  tmp="$(mktemp -d)"
  if ! "$MINICC" $OPT --compile "$src" > "$tmp/o.s" 2> "$tmp/err"; then
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
  if "$MINICC" $OPT --compile "$src" > /dev/null 2> "$tmp/err"; then
    echo "FAIL $name (expected a compile error)"; fail=$((fail+1))
  elif grep -qF -- "$want" "$tmp/err"; then
    echo "PASS $name"; pass=$((pass+1))
  else
    echo "FAIL $name (wrong error)"; echo "  wanted: $want"; echo "  got:    $(cat "$tmp/err")"; fail=$((fail+1))
  fi
done

# Optimizer checks (only meaningful with the optimizer on). Directives in the source:
#   // ASM-HAS: text    main() assembly must contain text
#   // ASM-NOT: text    main() assembly must not contain text
#   // FILE-NOT: text   the whole assembly file must not contain text
# The program must also still print tests/asm/<name>.expected.
if [ "$OPT" != "-O0" ]; then
  for src in tests/asm/*.cpp; do
    [ -e "$src" ] || continue
    base="$(basename "$src" .cpp)"
    name="asm/$base"
    tmp="$(mktemp -d)"
    ok=1
    if ! "$MINICC" $OPT --compile "$src" > "$tmp/o.s" 2> "$tmp/err"; then
      echo "FAIL $name (compile)"; cat "$tmp/err"; fail=$((fail+1)); continue
    fi
    awk '/^main:/{f=1;next} /^\.(globl|data|section)/{f=0} f' "$tmp/o.s" > "$tmp/main.s"

    while IFS= read -r want; do
      [ -n "$want" ] || continue
      if ! grep -qF -- "$want" "$tmp/main.s"; then echo "FAIL $name: main() lacks '$want'"; ok=0; fi
    done < <(sed -n 's#^// ASM-HAS: ##p' "$src" | tr -d '\r')

    while IFS= read -r nope; do
      [ -n "$nope" ] || continue
      if grep -qF -- "$nope" "$tmp/main.s"; then echo "FAIL $name: main() still contains '$nope'"; ok=0; fi
    done < <(sed -n 's#^// ASM-NOT: ##p' "$src" | tr -d '\r')

    while IFS= read -r nope; do
      [ -n "$nope" ] || continue
      if grep -qF -- "$nope" "$tmp/o.s"; then echo "FAIL $name: assembly still contains '$nope'"; ok=0; fi
    done < <(sed -n 's#^// FILE-NOT: ##p' "$src" | tr -d '\r')

    if gcc "$tmp/o.s" -o "$tmp/o" 2> "$tmp/gccerr"; then
      "$tmp/o" > "$tmp/out.txt" || true
      if ! diff -u "tests/asm/$base.expected" "$tmp/out.txt"; then echo "FAIL $name (output mismatch)"; ok=0; fi
    else
      echo "FAIL $name (assemble/link)"; cat "$tmp/gccerr"; ok=0
    fi

    if [ "$ok" -eq 1 ]; then echo "PASS $name"; pass=$((pass+1)); else fail=$((fail+1)); fi
  done
fi

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]