#!/usr/bin/env bash
# Fast pure-header test suite — no KWin link. Exit non-zero on first failure.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TESTS="$ROOT/tests"
OUT="${TMPDIR:-/tmp}/kwin-tiling-tests-$$"
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT

CXXFLAGS=(-std=c++20 -O2 -Wall -Wextra)
pass=0
for src in "$TESTS"/*_test.cpp; do
  name="$(basename "${src%.cpp}")"
  echo "=== $name ==="
  g++ "${CXXFLAGS[@]}" -o "$OUT/$name" "$src"
  "$OUT/$name"
  pass=$((pass + 1))
done
echo "ALL $pass tests OK"
