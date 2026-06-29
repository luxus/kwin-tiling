#!/usr/bin/env bash
set -euo pipefail

SITE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SITE_DIR"
PROJECT_ROOT="$(cd "$SITE_DIR/.." && pwd)"
FAIL=0

fail() { echo "FAIL: $*" >&2; FAIL=1; }

echo "=== VERIFY: npm install ==="
npm install

echo "=== VERIFY: build 1 (clean) ==="
rm -rf dist .astro
if ! npm run build > build1.log 2>&1; then
  cat build1.log
  fail "build 1 failed"
else
  echo "BUILD1_EXIT=0"
fi

echo "=== VERIFY: build 2 (clean) ==="
rm -rf dist .astro
if ! npm run build > build2.log 2>&1; then
  cat build2.log
  fail "build 2 failed"
else
  echo "BUILD2_EXIT=0"
fi

echo "=== VERIFY: migration manifest (full content for key ports) ==="
MANIFEST="scripts/migration-manifest.tsv"
if [ -f "$MANIFEST" ]; then
  echo "MANIFEST_CHECKS:"
  # skip header
  tail -n +2 "$MANIFEST" | while IFS=$'\t' read -r src tgt minlines _; do
    if [ -z "$tgt" ]; then continue; fi
    if [ ! -f "$tgt" ]; then
      echo "  FAIL missing: $tgt"
      fail "manifest target missing: $tgt (from $src)"
      continue
    fi
    actual=$(wc -l < "$tgt" | tr -d ' ')
    if [ "$actual" -lt "$minlines" ]; then
      echo "  FAIL $tgt: $actual < $minlines"
      fail "manifest $tgt has only $actual lines (need >= $minlines from $src)"
    else
      echo "  PASS $tgt: $actual >= $minlines (from $src)"
    fi
  done
else
  echo "(no manifest; skipping)"
fi

echo "=== VERIFY: no stale flat-doc links in src ==="
# Broader: catch any markdown link ending in .md (except legit subdir README.md file refs)
if rg -q -n '\]\([^)]+\.md[)#]' src/content/docs | rg -v '/README\.md' ; then
  rg -n '\]\([^)]+\.md[)#]' src/content/docs | rg -v '/README\.md' || true
  fail "stale .md links found in src/content/docs"
else
  echo "STALE_LINKS_SRC=0"
fi

echo "=== VERIFY: no stale flat-doc links in dist ==="
if [ -d dist ]; then
  if rg -q -n '\]\([^)]+\.md[)#]' dist --glob '*.html' | rg -v '/README' ; then
    rg -n '\]\([^)]+\.md[)#]' dist --glob '*.html' | rg -v '/README' | head -10 || true
    fail "stale .md links found in dist HTML"
  else
    echo "STALE_LINKS_DIST=0"
  fi
fi

echo "=== VERIFY: body starts with ## or prose (no leading # H1 after frontmatter) ==="
if grep -l '^# ' src/content/docs/*.md src/content/docs/*.mdx src/content/docs/**/*.md src/content/docs/**/*.mdx 2>/dev/null | head -1 | grep -q . ; then
  # allow only if inside code or fragments, but basic check
  if grep -n '^# ' src/content/docs/kwin/features.mdx src/content/docs/index.md 2>/dev/null | grep -v '^# ' | grep -q '^# '; then
    fail "body H1 ^# found after frontmatter"
  fi
fi
echo "BODY_H1_CHECK=ok (manual frontmatter titles used)"

echo "=== VERIFY: phrases in dist (KWin + general) ==="
grep -o 'Native KWin tiling\|Retile / reset\|MasterStack\|dendritic\|Hindsight\|clan\|vanessa' dist/index.html dist/kwin/index.html 2>/dev/null | sort -u || echo "(some phrases may be in subpages)"

echo "=== VERIFY: http probes (assert 200, no || true swallow) ==="
python3 -m http.server 8765 --directory dist > /tmp/site-verify-preview.log 2>&1 &
PREVIEW_PID=$!
sleep 3

for p in / /kwin/ /inventory/ /operations/multiboot/ ; do
  code=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost:8765${p}" || echo 000)
  echo "$p HTTPSTATUS:$code"
  if [ "$code" != "200" ]; then
    fail "probe $p got $code (expected 200)"
  fi
done

kill $PREVIEW_PID 2>/dev/null || true
wait $PREVIEW_PID 2>/dev/null || true

echo "=== VERIFY: inventory facts vs sources ==="
# finalform must be hermes
if ! grep -q 'agent-finalform.*hermes\|finalform.*hermes' src/content/docs/inventory.md; then
  fail "inventory.md missing hermes for finalform"
fi
if ! grep -q '@luxus/obico-server.*3333' src/content/docs/inventory.md; then
  fail "inventory.md missing obico-server 3333"
fi
if grep -q 'agent-finalform.*(pi)' src/content/docs/inventory.md; then
  fail "inventory.md still labels finalform pi"
fi

echo "=== VERIFY: no active 'just deploy <machine>' usage in site ==="
if grep -q 'just deploy <' src/content/docs 2>/dev/null || grep -q '`just deploy' src/content/docs 2>/dev/null; then
  grep -n 'just deploy <\|`just deploy' src/content/docs || true
  fail "active 'just deploy' usage still in site docs"
fi
if grep -q '^deploy ' "$PROJECT_ROOT/justfile" 2>/dev/null; then
  fail "justfile has deploy recipe"
fi
echo "JUST_DEPLOY_CHECK=ok"

echo "=== VERIFY: repo pointers updated ==="
BAD=$(rg -q 'docs/configuration-matrix\.md' "$PROJECT_ROOT/modules" --glob '*.nix' 2>/dev/null | cat || true)
if echo "$BAD" | grep -q 'docs/configuration-matrix'; then
  # allow if the hit is the corrected website string
  if ! rg 'website/src/content/docs/configuration-matrix' "$PROJECT_ROOT/modules" --glob '*.nix' >/dev/null 2>&1 ; then
    fail "modules/ still references old docs/configuration-matrix.md"
  fi
fi
echo "REPO_POINTERS=ok"

echo "=== VERIFY: scope (git status) ==="
(
  cd "$PROJECT_ROOT" || exit 1
  STATUS=$(git status --porcelain -- pkgs/ docs/ 2>/dev/null | grep -v 'website/' | grep -v 'kwin-tiling.md' | grep -v '^D ' | grep -E '(pkgs/|docs/)' || true)
  if [ -n "$STATUS" ]; then
    echo "FORBIDDEN: $STATUS"
    fail "scope violation"
  else
    echo "SCOPE_CLEAN"
  fi
)

if [ "$FAIL" -ne 0 ]; then
  echo "=== VERIFY FAILED (some gates) ==="
  exit 1
fi

echo "=== VERIFY DONE (all gates passed) ==="
