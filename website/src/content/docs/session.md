---
title: KWin + Noctalia session
description: Minimum KDE services and session wiring for native tiling with Noctalia — without full Plasma. Production reference is luxusAi’s kwin-noctalia package.
---

This page is for **way 2** from [Overview → Two ways to start](/):

| | **1. KDE Plasma + tiling** | **2. KWin + Noctalia** (this page) |
| --- | --- | --- |
| Session | Normal Plasma Wayland | Custom `wayland-sessions` entry |
| Shell | plasmashell | [Noctalia](https://github.com/noctalia-dev/noctalia) |
| Effort | Flake module + enable tiling | Patched KWin + session plumbing below |
| Tiling package | This flake only | This flake + session stack |

If you already use Plasma, stop at way 1 — add the flake module, set
`[Tiling] Enabled=true`, and you are done. You do not need anything on this page.

Way 2 is everything around the compositor: how KWin starts, which KDE daemons
run with it, how Noctalia attaches as the shell, and how logout and shortcuts
behave. Shortcuts and the tiling KCM are the same as way 1 — see
[Usage & Shortcuts](usage).

**Tested against:** Plasma / KWin **6.7.x** (currently **6.7.3** via nixpkgs
`kdePackages.kwin`). After a host switch that rebuilds KWin, **relogin** (or
restart the compositor session) so the running process picks up the new binary.

## Production reference (recommended)

A full, multi-session-safe implementation lives in
[**luxusAi**](https://github.com/luxus/luxusAi) — not as a second tiling fork,
but as the session packaging that consumes this flake:

| Piece | Where in luxusAi |
| --- | --- |
| Patched KWin input | `inputs.kwin-tiling` → `modules/nixos/kwin-tiling.nix` (overlay) |
| Session package | `pkgs/kwin-noctalia-session` (`kwin-noctalia.service`, ready oneshot, stop script, scoped KDE units) |
| User profile | `hjem/profiles/kwin-noctalia.nix` (packages, QML paths, tiling enable, QML cache wipe) |
| Session registry | `lib/desktop-sessions.nix` — `kwin-noctalia` pulls `nixos.kwin-tiling` + `hjem.shell` + `hjem.kwin-noctalia` |
| Re-exports for other flakes | `modules/flake/kwin-reexport.nix` → `nixosModules.kwin-tiling`, `hjemModules.kwin-noctalia`, package overlay |

**Compose onto a host (conceptual):** list `kwin-noctalia` in that machine’s
`sessions` (see `lib/desktops.nix`); the clan registry expands
`sessionFeatures` automatically. Host-only extras (greeter, remote desktop,
NVIDIA, …) stay on the machine.

**External flakes (Hjem required)** can re-use luxusAi without copying units:

```nix
inputs.luxusAi.url = "github:luxus/luxusAi";
inputs.kwin-tiling.url = "github:luxus/kwin-tiling"; # optional if you only use luxusAi’s pin

# NixOS host — patches kdePackages.kwin
imports = [ inputs.luxusAi.nixosModules.kwin-tiling ];
nixpkgs.overlays = [ inputs.luxusAi.overlays.default ]; # pkgs.kwin-noctalia-session, …

# Hjem user — session files + packages
imports = [ inputs.luxusAi.hjemModules.kwin-noctalia ];
```

Noctalia KWin-only patches (layer-shell lock, graceful logout) come from
[luxus/noctalia-kwin](https://github.com/luxus/noctalia-kwin), not from this
repo.

The rest of this page documents the **contract** that packaging must satisfy —
useful if you reimplement the session yourself. Prefer luxusAi’s package over
copy-pasting the old minimal Home Manager sketch.

## Architecture

```text
greetd / greeter (wayland-sessions)
    └── Exec → kwin-noctalia-session   (session script)
            ├── marker: $XDG_RUNTIME_DIR/kwin-noctalia-active
            ├── systemctl --user start kwin-noctalia.service
            │         └── kwin_wayland_wrapper --xwayland   (patched KWin)
            ├── kwin-noctalia-session-ready.service
            │         └── import WAYLAND_DISPLAY + env; start noctalia
            ├── scoped KDE user units (PartOf=kwin-noctalia.service)
            └── on exit: stop noctalia first, then kwin (no SIGHUP restart)
```

Noctalia replaces **plasmashell**. KWin still owns the compositor, window
management, and native tiling from this flake.

**Multi-session hosts:** every daemon scopes to `kwin-noctalia.service` (or
that session’s ready target), never bare `graphical-session.target`. Upstream
Plasma units are re-scoped with drop-ins (`PartOf=` / `WantedBy=` the compositor
unit). A runtime marker (`kwin-noctalia-active`) plus
`ConditionPathExists=` prevents orphan compositors after logout (black screen /
DRM busy on next login).

## Minimum KDE services

| Component | Package (KDE 6) | Why it is needed |
| --- | --- | --- |
| **KWin** | `kdePackages.kwin` (patched via this flake) | Compositor, tiling engine, global shortcuts backend |
| **Noctalia** | `noctalia` (prefer noctalia-kwin build) | Shell UI — bar, launcher, lock, session menu |
| **kded6** | `kdePackages.kded` | KDE daemon framework; global shortcut registration |
| **xdg-desktop-portal-kde** | `kdePackages.xdg-desktop-portal-kde` | Screen capture, file picker, remote desktop portal APIs |
| **powerdevil** | `kdePackages.powerdevil` | Suspend, lid close, idle — KWin does not handle power policy |
| **KWallet** | wallet init oneshot | Credential storage for apps that expect KDE secrets |
| **kbuildsycoca6** | `kdePackages.kservice` | Rebuild app/service DB so `.desktop` entries appear |
| **Union QQC style** | `kdePackages.union` | Plasma 6.7+ QtQuick apps expect Union, not legacy Breeze |

Optional: **krdp** (remote desktop), **kde-rounded-corners** effect (separate
plugin — not compositor borders).

### Deliberately omitted (vs full Plasma)

| Not started | Why |
| --- | --- |
| **plasmashell** | Noctalia is the shell |
| **ksmserver** | Save-on-logout prompts; omitted for fast greetd logout (`loginctl` fallback) |
| **plasma-workspace.target** | Full Plasma session target — conflicts with kwin-only model |

## Systemd user model (production shape)

luxusAi’s `kwin-noctalia-session` installs roughly:

```text
kwin-noctalia.service                    # kwin_wayland_wrapper --xwayland
├── kwin-noctalia-session-ready.service  # Wayland socket + import-environment + noctalia
├── plasma-kded6.service                 # re-scoped PartOf/WantedBy compositor
├── plasma-xdg-desktop-portal-kde.service
├── plasma-powerdevil.service
├── kwallet-kwin-noctalia.service
├── noctalia-kglobalaccel-register.service
├── (optional) krdp portal-auth + krdpserver
kwin-noctalia-shutdown.target            # Conflicts= compositor (clean stop)
```

Profile extras (Hjem):

```text
kwin-tiling-config.service    # kwriteconfig6 [Tiling] Enabled=true; drop store symlink on kwinrc
kcm-qmlcache-reset.service    # wipe ~/.cache/systemsettings|kcmshell6/qmlcache (Nix mtimes)
```

Drop-in pattern for upstream Plasma units:

```ini
# plasma-kded6.service.d/kwin-noctalia.conf
[Unit]
PartOf=
After=
PartOf=kwin-noctalia.service
After=kwin-noctalia.service kwin-noctalia-session-ready.service
Requires=kwin-noctalia-session-ready.service
BindsTo=kwin-noctalia.service

[Service]
TimeoutStopSec=5
KillMode=control-group

[Install]
WantedBy=kwin-noctalia.service
```

**Logout order matters:** stop **Noctalia first**, then KWin. If KWin dies
first, Noctalia can hang in stop. Session script trap + shutdown target +
explicit `pkill` fallbacks handle greetd teardown. **Do not** restart the
compositor on SIGHUP from greetd.

## Session environment

Set before starting KWin and import into `systemd --user` once the Wayland
socket exists:

```ini
KDE_FULL_SESSION=true
KDE_SESSION_VERSION=6
XDG_CURRENT_DESKTOP=KDE
XDG_SESSION_DESKTOP=KDE
XDG_SESSION_TYPE=wayland
QT_QUICK_CONTROLS_STYLE=org.kde.union
```

**`XDG_DATA_DIRS`** must include Nix profile paths or Noctalia bindings and KDE
app pickers look empty:

```text
$XDG_DATA_HOME:$HOME/.nix-profile/share:/etc/profiles/per-user/$USER/share:/run/current-system/sw/share
```

For the tiling **settings KCM** on Nix, set `QML2_IMPORT_PATH` and
`NIXPKGS_QT6_QML_IMPORT_PATH` to include at least patched `kwin`, `union`,
`plasma-desktop`, and `systemsettings` QML plugin dirs. Without this, *System
Settings → Window Management → Tiling* may not load.

Qt’s QML cache under `~/.cache/systemsettings/qmlcache` uses store-pinned
mtimes — **wipe it at session start** after KCM updates or new options never
appear.

## Enable native tiling

Patching KWin is not enough — turn tiling on at runtime:

```ini
# ~/.config/kwinrc
[Tiling]
Enabled=true
```

Or *System Settings → Window Management → Tiling*. Keep `kwinrc` **writable**
(not a read-only store symlink) so KWin and the KCM can persist changes.
luxusAi’s oneshot removes a symlink and runs:

```bash
kwriteconfig6 --file kwinrc --group Tiling --key Enabled --type bool true
```

## Noctalia on KWin

Upstream Noctalia supports KWin as a backend. For **kwin-only** sessions you
typically need:

1. **Layer-shell lock** — when `ext-session-lock-v1` is missing, show lock UI
   via layer-shell instead of failing silently.
2. **Graceful logout** — prefer `org.kde.Shutdown` / `org.kde.LogoutPrompt` when
   present; otherwise `loginctl terminate-session`.

Use [luxus/noctalia-kwin](https://github.com/luxus/noctalia-kwin):

```nix
inputs.noctalia-kwin.url = "github:luxus/noctalia-kwin";
inputs.noctalia-kwin.inputs.noctalia.follows = "noctalia";
# package = inputs.noctalia-kwin.packages.${system}.default;
```

Register a `.desktop` with Desktop Actions and `org.kde.kglobalaccel` /
`doRegister` so bindings appear under *System Settings → Shortcuts*.

## Minimal standalone sketch

If you are **not** using luxusAi, you still need the same pieces: session
script, compositor unit, ready oneshot, re-scoped KDE units, marker file, clean
stop order. A complete maintained package is
`pkgs/kwin-noctalia-session` in luxusAi — copy or depend on it rather than
reimplementing from a blog-sized HM snippet.

Bare minimum flake surface for **tiling only** (Plasma session, way 1):

```nix
{
  inputs.kwin-tiling.url = "github:luxus/kwin-tiling";
  # host:
  imports = [ inputs.kwin-tiling.nixosModules.kwin-tiling ];
}
```

Bare minimum for **session + tiling** (way 2) via luxusAi re-exports — see
[Production reference](#production-reference-recommended) above.

### Wayland session desktop entry

```ini
# share/wayland-sessions/kwin-noctalia.desktop
[Desktop Entry]
Type=Application
Name=KWin + Noctalia
Comment=KWin compositor with Noctalia shell
Exec=/etc/profiles/per-user/YOURUSER/bin/kwin-noctalia-session
TryExec=/etc/profiles/per-user/YOURUSER/bin/kwin-noctalia-session
DesktopNames=KDE
```

Point greetd at the same `Exec=` path, or use a greeter that lists
`wayland-sessions`.

### Checklist after first login

1. Noctalia bar visible after a few seconds (waits on `session-ready`).
2. *System Settings → Window Management → Tiling* opens and shows layouts
   (including any newly shipped options after QML cache wipe).
3. `Meta+Left` / `Meta+Right` focus tiled windows ([shortcuts](usage)).
4. Lock and session menu work (layer-shell patch).
5. Logout returns to greeter; next login has no black screen / DRM denied.
6. Optional: full regression — `pkgs/kwin-tiling/scripts/session-smoke.md` in
   this repo.

## Common pitfalls

| Symptom | Likely cause |
| --- | --- |
| Empty shortcut / app binding menus | `XDG_DATA_DIRS` missing Nix profile paths; run `kbuildsycoca6 --noincremental` |
| Tiling KCM missing or stale options | `QML2_IMPORT_PATH` unset; clear `~/.cache/systemsettings/qmlcache` and `kcmshell6/qmlcache` |
| Lock screen does nothing on KWin | noctalia-kwin layer-shell patch not applied |
| Logout hangs (greetd) | Session script waiting on compositor restart; stop noctalia then kwin explicitly |
| Next login black screen / DRM busy | Previous kwin/noctalia still holding GPU; marker + ConditionPathExists + stop script |
| Config changes do not persist | `kwinrc` store symlink — make it a writable file |
| Host switched but behavior unchanged | Still running old `kwin_wayland` process — **relogin** |

## After `nh os switch` / rebuild

1. Confirm generation: `readlink /run/current-system`
2. Confirm binary: `readlink -f /run/current-system/sw/bin/kwin_wayland` (expect
   `…-kwin-6.7.x…` with your pin)
3. **Relogin** into KWin+Noctalia (or Plasma)
4. Run [session-smoke.md](https://github.com/luxus/kwin-tiling/blob/main/pkgs/kwin-tiling/scripts/session-smoke.md)

## Next steps

Session packaging stays in **luxusAi** (`kwin-noctalia-session` + Hjem profile).
This repo owns the **compositor patch** only. Tiling bugs → issues on
[luxus/kwin-tiling](https://github.com/luxus/kwin-tiling). Session wiring →
luxusAi / noctalia-kwin.
