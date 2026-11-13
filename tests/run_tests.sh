#!/usr/bin/env bash
# Runs every test in tests/:
#   ok_*.src   compile + run, stdout must equal ok_*.out (stdin from ok_*.in if present)
#   err_*.src  compilation must FAIL and stderr must contain the text in err_*.err
#   rt_*.src   compiles, but running must FAIL and stderr must contain the text in rt_*.err
cd "$(dirname "$0")/.." || exit 1
pass=0; fail=0
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

report() { # status name [detail]
  if [ "$1" = PASS ]; then pass=$((pass+1)); echo "PASS  $2"
  else fail=$((fail+1)); echo "FAIL  $2  $3"; fi
}

for src in tests/ok_*.src; do
  n=${src%.src}
  if ! bin/compile "$src" -o "$tmp/o.tac" 2>"$tmp/err"; then report FAIL "$n" "(compile error: $(head -1 "$tmp/err"))"; continue; fi
  in=/dev/null; [ -f "$n.in" ] && in="$n.in"
  if ! bin/run "$tmp/o.tac" <"$in" >"$tmp/out" 2>"$tmp/err"; then report FAIL "$n" "(runtime error: $(head -1 "$tmp/err"))"; continue; fi
  if diff -q "$tmp/out" "$n.out" >/dev/null; then report PASS "$n"; else report FAIL "$n" "(output differs)"; fi
done

for src in tests/err_*.src; do
  n=${src%.src}
  if bin/compile "$src" -o "$tmp/o.tac" 2>"$tmp/err"; then report FAIL "$n" "(compiled but should fail)"; continue; fi
  if grep -qF -- "$(cat "$n.err")" "$tmp/err"; then report PASS "$n"; else report FAIL "$n" "(wrong message: $(head -1 "$tmp/err"))"; fi
done

for src in tests/rt_*.src; do
  n=${src%.src}
  if ! bin/compile "$src" -o "$tmp/o.tac" 2>"$tmp/err"; then report FAIL "$n" "(compile error)"; continue; fi
  if bin/run "$tmp/o.tac" </dev/null >/dev/null 2>"$tmp/err"; then report FAIL "$n" "(should fail at run time)"; continue; fi
  if grep -qF -- "$(cat "$n.err")" "$tmp/err"; then report PASS "$n"; else report FAIL "$n" "(wrong message: $(head -1 "$tmp/err"))"; fi
done

echo "----"; echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
