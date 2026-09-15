#!/usr/bin/env bash
# Idempotent Cloud Agent setup for kwin-tiling.
#
# The documented developer workflow is Nix-based (`nix flake check`,
# `nix build .#kwin-tiling`, `bash pkgs/kwin-tiling/tests/run.sh`). This script
# installs Nix once, starts the daemon, and warms the store so the fast pure
# self-check runs instantly on later boots. It never compiles KWin (that is a
# long, on-demand build left to the developer via `nix build .#kwin-tiling`).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
. "$SCRIPT_DIR/nix-daemon.sh"

install_nix() {
  if [ -e "$NIX_DAEMON_BIN" ]; then
    echo "[install] Nix already present; skipping installation."
    return
  fi
  echo "[install] Installing Nix (Determinate installer, no init)..."
  curl --proto '=https' --tlsv1.2 -sSf -L https://install.determinate.systems/nix \
    | sudo sh -s -- install linux --init none --no-confirm
}

install_nix
nix_ensure_daemon

echo "[install] Warming the Nix store with the fast pure self-check (nix flake check)..."
nix flake check --print-build-logs

echo "[install] Done. Run 'nix build .#kwin-tiling' to compile the patched compositor on demand."
