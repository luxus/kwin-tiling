#!/usr/bin/env bash
# Per-boot reconciliation: the multi-user Nix daemon process does not survive
# into a fresh boot (and a snapshot leaves a stale socket file behind), so
# probe liveness and (re)start the daemon when it is not answering.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
. "$SCRIPT_DIR/nix-daemon.sh"

if [ ! -e "$NIX_DAEMON_BIN" ]; then
  echo "[start] Nix is not installed; nothing to start."
  exit 0
fi

nix_ensure_daemon
