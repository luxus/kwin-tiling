#!/usr/bin/env bash
# Idempotent Cloud Agent setup for kwin-tiling.
#
# The documented developer workflow is Nix-based (`nix flake check`,
# `nix build .#kwin-tiling`, `bash pkgs/kwin-tiling/tests/run.sh`). This script
# installs Nix once, starts the daemon, and warms the store so the fast pure
# self-check runs instantly on later boots. It never compiles KWin (that is a
# long, on-demand build left to the developer via `nix build .#kwin-tiling`).
set -euo pipefail

NIX_PROFILE_SCRIPT="/nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh"

install_nix() {
  if [ -e /nix/var/nix/profiles/default/bin/nix ]; then
    echo "[install] Nix already present; skipping installation."
    return
  fi
  echo "[install] Installing Nix (Determinate installer, no init)..."
  curl --proto '=https' --tlsv1.2 -sSf -L https://install.determinate.systems/nix \
    | sudo sh -s -- install linux --init none --no-confirm
}

start_daemon() {
  if [ -S /nix/var/nix/daemon-socket/socket ]; then
    echo "[install] Nix daemon socket already present."
    return
  fi
  echo "[install] Starting nix-daemon..."
  sudo nohup /nix/var/nix/profiles/default/bin/nix-daemon >/tmp/nix-daemon.log 2>&1 &
  for _ in $(seq 1 30); do
    [ -S /nix/var/nix/daemon-socket/socket ] && break
    sleep 1
  done
  [ -S /nix/var/nix/daemon-socket/socket ] || { echo "[install] daemon socket not ready" >&2; exit 1; }
}

install_nix
start_daemon

# shellcheck disable=SC1090
[ -f "$NIX_PROFILE_SCRIPT" ] && . "$NIX_PROFILE_SCRIPT"
export NIX_CONFIG="experimental-features = nix-command flakes"

echo "[install] Warming the Nix store with the fast pure self-check (nix flake check)..."
nix flake check --print-build-logs

echo "[install] Done. Run 'nix build .#kwin-tiling' to compile the patched compositor on demand."
