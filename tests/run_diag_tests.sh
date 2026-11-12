#!/usr/bin/env bash
# usage: tests/run_diag_tests.sh [path-to-minicc]
set -u
MINICC="${1:-./build/minicc}"
tmp="$(mktemp -d)"
pass=0
fail=0
ok()  { echo "PASS $1"; pass=$((pass+1)); }
bad() { echo "FAIL $1"; fail=$((fail+1)); }

# semantic error: file:line, message, and the source line are shown
"$MINICC" --compile tests/errors/undeclared_var.cpp > /dev/null 2> "$tmp/e1"
if grep -q 'undeclared_var.cpp:2: error: undeclared variable' "$tmp/e1" &&
   grep -qF 'int main() { return y; }' "$tmp/e1"; then ok diag/sema-location; else bad diag/sema-location; cat "$tmp/e1"; fi

# parse error: file:line:col, message, source line, caret under column 22
printf 'int main() { int x = ; return 0; }\n' > "$tmp/bad.cpp"
"$MINICC" --compile "$tmp/bad.cpp" > /dev/null 2> "$tmp/e2"
if grep -q 'bad.cpp:1:22: error: expected expression' "$tmp/e2"; then ok diag/parse-location; else bad diag/parse-location; cat "$tmp/e2"; fi
if grep -qE '\| {22}\^' "$tmp/e2"; then ok diag/caret; else bad diag/caret; cat "$tmp/e2"; fi

# error without a line number is still reported cleanly
printf 'int f() { return 1; }\n' > "$tmp/nomain.cpp"
"$MINICC" --compile "$tmp/nomain.cpp" > /dev/null 2> "$tmp/e3"
if grep -q "nomain.cpp: error: .*no 'main' function defined" "$tmp/e3"; then ok diag/no-line; else bad diag/no-line; cat "$tmp/e3"; fi

# -o writes the assembly file
"$MINICC" --compile tests/hello.cpp -o "$tmp/h.s" > "$tmp/stdout" 2>&1
if [ -s "$tmp/h.s" ] && grep -q '^main:' "$tmp/h.s" && [ ! -s "$tmp/stdout" ]; then ok cli/output-file; else bad cli/output-file; fi

# a failed compile must not leave an output file behind
rm -f "$tmp/none.s"
"$MINICC" --compile "$tmp/bad.cpp" -o "$tmp/none.s" > /dev/null 2>&1
if [ ! -e "$tmp/none.s" ]; then ok cli/no-output-on-error; else bad cli/no-output-on-error; fi

# --version, --help, bad option, missing file
if "$MINICC" --version | grep -q '^minicc [0-9]'; then ok cli/version; else bad cli/version; fi
if "$MINICC" --help | grep -q 'Usage: minicc'; then ok cli/help; else bad cli/help; fi
"$MINICC" --bogus x > /dev/null 2>&1; rc=$?
if [ "$rc" -eq 1 ]; then ok cli/unknown-option; else bad cli/unknown-option; fi
"$MINICC" --compile "$tmp/does-not-exist.cpp" > /dev/null 2> "$tmp/e4"; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'cannot open' "$tmp/e4"; then ok cli/missing-file; else bad cli/missing-file; fi

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]