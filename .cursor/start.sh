#!/usr/bin/env bash
# Per-boot reconciliation: the multi-user Nix daemon process does not survive
# into a fresh boot, so (re)start it if its socket is missing, confirm
# readiness, then return.
set -euo pipefail

if [ ! -e /nix/var/nix/profiles/default/bin/nix-daemon ]; then
  echo "[start] Nix is not installed; nothing to start."
  exit 0
fi

if [ -S /nix/var/nix/daemon-socket/socket ]; then
  echo "[start] Nix daemon already running."
  exit 0
fi

echo "[start] Starting nix-daemon..."
sudo nohup /nix/var/nix/profiles/default/bin/nix-daemon >/tmp/nix-daemon.log 2>&1 &

for _ in $(seq 1 30); do
  [ -S /nix/var/nix/daemon-socket/socket ] && break
  sleep 1
done

if [ -S /nix/var/nix/daemon-socket/socket ]; then
  echo "[start] Nix daemon ready."
else
  echo "[start] Nix daemon socket not ready" >&2
  exit 1
fi
