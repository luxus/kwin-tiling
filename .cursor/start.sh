#!/usr/bin/env bash
# Per-boot service: the multi-user Nix daemon process does not survive into a
# fresh boot (and a snapshot leaves a stale socket file behind). This phase is
# run detached, so the daemon is launched in the foreground and kept attached
# for the whole boot rather than backgrounded (a backgrounded child would be
# reaped when this script returns).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
. "$SCRIPT_DIR/nix-daemon.sh"

if [ ! -e "$NIX_DAEMON_BIN" ]; then
  echo "[start] Nix is not installed; nothing to start."
  exit 0
fi

nix_run_daemon_foreground
