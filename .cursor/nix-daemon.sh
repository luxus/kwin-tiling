#!/usr/bin/env bash
# Shared helpers for managing the multi-user Nix daemon in Cloud Agents.
#
# The daemon is installed with `--init none` (no systemd in the VM), so its
# process must be (re)launched on every boot. A snapshot preserves the socket
# *file* on disk even though the daemon process is gone, so liveness must be
# probed by actually talking to the daemon — not by testing for the socket.

NIX_DAEMON_BIN="/nix/var/nix/profiles/default/bin/nix-daemon"
NIX_DAEMON_SOCKET="/nix/var/nix/daemon-socket/socket"
NIX_PROFILE_SCRIPT="/nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh"

# Make the `nix` client available for the liveness probe when possible.
nix_load_profile() {
  # shellcheck disable=SC1090
  [ -f "$NIX_PROFILE_SCRIPT" ] && . "$NIX_PROFILE_SCRIPT"
  export NIX_CONFIG="experimental-features = nix-command flakes"
}

# Succeeds only when a daemon is actually answering (not just when a stale
# socket file exists).
nix_daemon_alive() {
  if command -v nix >/dev/null 2>&1; then
    nix store info --store daemon >/dev/null 2>&1 && return 0
  fi
  pgrep -x nix-daemon >/dev/null 2>&1 && [ -S "$NIX_DAEMON_SOCKET" ] && return 0
  return 1
}

# (Re)start the daemon if it is not answering; clears a stale socket first.
nix_ensure_daemon() {
  nix_load_profile
  if nix_daemon_alive; then
    echo "[nix] daemon already running."
    return 0
  fi
  if [ -e "$NIX_DAEMON_SOCKET" ]; then
    echo "[nix] clearing stale daemon socket."
    sudo rm -f "$NIX_DAEMON_SOCKET"
  fi
  echo "[nix] starting nix-daemon..."
  sudo nohup "$NIX_DAEMON_BIN" >/tmp/nix-daemon.log 2>&1 &
  for _ in $(seq 1 30); do
    nix_daemon_alive && break
    sleep 1
  done
  if nix_daemon_alive; then
    echo "[nix] daemon ready."
  else
    echo "[nix] daemon failed to start; see /tmp/nix-daemon.log" >&2
    return 1
  fi
}
