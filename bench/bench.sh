#!/usr/bin/env bash
# usage: bench/bench.sh [path-to-minicc]
# Compiles every bench/*.cpp at -O0 and -O1, checks both print the same result,
# and compares assembly size and best-of-3 runtime.
set -eu
MINICC="${1:-./build/minicc}"
tmp="$(mktemp -d)"
now() { date +%s.%N; }

run_best() {   # $1 = binary, $2 = output file; prints best-of-3 seconds
  local best="" t0 t1 d i
  for i in 1 2 3; do
    t0="$(now)"; "$1" > "$2"; t1="$(now)"
    d="$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.4f", b-a}')"
    if [ -z "$best" ] || awk -v x="$d" -v y="$best" 'BEGIN{exit !(x<y)}'; then best="$d"; fi
  done
  echo "$best"
}

printf '%-10s %9s %9s %9s %9s %8s\n' program O0-lines O1-lines O0-sec O1-sec speedup
status=0
for src in bench/*.cpp; do
  name="$(basename "$src" .cpp)"
  for lvl in 0 1; do
    "$MINICC" -O$lvl --compile "$src" > "$tmp/$name.O$lvl.s"
    gcc "$tmp/$name.O$lvl.s" -o "$tmp/$name.O$lvl"
  done
  l0="$(wc -l < "$tmp/$name.O0.s")"
  l1="$(wc -l < "$tmp/$name.O1.s")"
  t0="$(run_best "$tmp/$name.O0" "$tmp/$name.O0.out")"
  t1="$(run_best "$tmp/$name.O1" "$tmp/$name.O1.out")"
  if ! cmp -s "$tmp/$name.O0.out" "$tmp/$name.O1.out"; then
    echo "MISMATCH in $name: -O0 and -O1 print different results"; status=1
  fi
  sp="$(awk -v a="$t0" -v b="$t1" 'BEGIN{ if (b>0) printf "%.2fx", a/b; else print "n/a" }')"
  printf '%-10s %9s %9s %9s %9s %8s   result=%s\n' "$name" "$l0" "$l1" "$t0" "$t1" "$sp" "$(tr -d '\n' < "$tmp/$name.O1.out")"
done
exit $status