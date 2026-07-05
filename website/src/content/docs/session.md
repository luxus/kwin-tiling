---
title: KWin + Noctalia session
description: Minimum KDE services and session wiring for native tiling with Noctalia — without full Plasma.
---

This page is for **way 2** from [Overview → Two ways to start](/):

| | **1. KDE Plasma + tiling** | **2. KWin + Noctalia** (this page) |
| --- | --- | --- |
| Session | Normal Plasma Wayland | Custom `wayland-sessions` entry |
| Shell | plasmashell | [Noctalia](https://github.com/noctalia-dev/noctalia) |
| Effort | Flake module + enable tiling | Patched KWin + session plumbing below |

If you already use Plasma, stop at way 1 — add the flake module, set
`[Tiling] Enabled=true`, and you are done. You do not need anything on this page.

Way 2 is everything around the compositor: how KWin starts, which KDE daemons
run with it, how Noctalia attaches as the shell, and how logout and shortcuts
behave. Shortcuts and the tiling KCM are the same as way 1 — see
[Usage & Shortcuts](usage).

## Architecture

```text
greetd / SDDM
    └── wayland session: kwin-noctalia.desktop
            ├── kwin_wayland (patched — this flake)
            ├── noctalia (shell: bar, launcher, lock UI, session menu)
            └── minimal KDE user services (scoped to the compositor unit)
```

Noctalia replaces **plasmashell** (panel, launcher, notifications chrome). KWin
still owns the compositor, window management, and native tiling.

## Minimum KDE services

| Component | Package (KDE 6) | Why it is needed |
| --- | --- | --- |
| **KWin** | `kdePackages.kwin` (patched) | Compositor, tiling engine, global shortcuts backend |
| **Noctalia** | `noctalia` | Shell UI — bar, launcher, lock, session menu |
| **kded6** | `kdePackages.kded` | KDE daemon framework; global shortcut registration, some integrations |
| **xdg-desktop-portal-kde** | `kdePackages.xdg-desktop-portal-kde` | Screen capture, file picker, remote desktop portal APIs |
| **powerdevil** | `kdePackages.powerdevil` | Suspend, lid close, idle — KWin does not handle power policy |
| **KWallet / ksecretd** | wallet init at session start | Credential storage for apps that expect KDE secrets |
| **kbuildsycoca6** | from `kdePackages.kservice` | Rebuild app/service DB so `.desktop` entries appear in launchers and shortcut pickers |
| **Union QQC style** | `kdePackages.union` | Plasma 6.7+ QtQuick apps expect Union, not legacy Breeze controls |

Optional: **krdp** if you want KDE remote desktop from the session.

### Deliberately omitted (vs full Plasma)

| Not started | Why |
| --- | --- |
| **plasmashell** | Noctalia is the shell |
| **ksmserver** | Session manager + save-on-logout prompts; omitted for fast greetd logout (falls back to `loginctl`) |
| **plasma-workspace.target** | Full Plasma session target — conflicts with kwin-only model |

If you need save prompts on logout, add a scoped **ksmserver** stack — KWin alone
is not a session manager.

## Session environment

Set these before starting KWin and import them into `systemd --user` once the
Wayland socket exists:

```ini
KDE_FULL_SESSION=true
KDE_SESSION_VERSION=6
XDG_CURRENT_DESKTOP=KDE
XDG_SESSION_DESKTOP=KDE
XDG_SESSION_TYPE=wayland
QT_QUICK_CONTROLS_STYLE=org.kde.union
```

**`XDG_DATA_DIRS`** must include paths where your `.desktop` files live. If it is
wrong, Noctalia's binding menus and KDE's app picker look empty even though
packages are installed. A typical Nix user session prepends:

```text
$XDG_DATA_HOME:$HOME/.nix-profile/share:/etc/profiles/per-user/$USER/share:/run/current-system/sw/share
```

For the tiling **settings KCM** on Nix, set `QML2_IMPORT_PATH` and
`NIXPKGS_QT6_QML_IMPORT_PATH` to a colon-separated list of KCM plugin paths.
At minimum include the patched KWin package and Union:

```nix
let
  kde = pkgs.kdePackages;
  kcmQmlPath = lib.concatStringsSep ":" (map (p: "${p}/lib/qt-6/qml") [
    kde.union
    kde.kwin
    kde.plasma-desktop
    kde.systemsettings
  ]);
in
{
  environment.sessionVariables = {
    QML2_IMPORT_PATH = kcmQmlPath;
    NIXPKGS_QT6_QML_IMPORT_PATH = kcmQmlPath;
  };
}
```

Without this, *System Settings → Window Management → Tiling* may not load the
panel.

## Enable native tiling

Patching KWin is not enough — tiling must be switched on at runtime:

```ini
# ~/.config/kwinrc
[Tiling]
Enabled=true
```

Or use *System Settings → Window Management → Tiling*. On Nix, ensure `kwinrc`
is writable (not a read-only store symlink) so KWin and the KCM can persist
changes.

A small oneshot at session start avoids forgetting:

```bash
kwriteconfig6 --file kwinrc --group Tiling --key Enabled --type bool true
```

**Nix-specific:** Qt caches compiled KCM QML under `~/.cache/systemsettings/qmlcache`
with store-pinned mtimes, so new KCM options can fail to appear until that cache
is cleared at session start (`rm -rf ~/.cache/systemsettings/qmlcache`).

## Noctalia on KWin

Upstream Noctalia supports KWin as a compositor backend. For a **kwin-only**
session you typically need two small Noctalia patches:

1. **Layer-shell lock** — KWin 6.7 has no `ext-session-lock-v1`; when the
   session-lock protocol is missing but layer-shell is available, show the lock
   UI as an overlay instead of failing silently.
2. **Graceful logout** — on KWin, try `org.kde.Shutdown` / `org.kde.LogoutPrompt`
   when Plasma session D-Bus is present; otherwise fall back to
   `loginctl terminate-session` (normal for kwin-only + greetd).

Use the packaged build from [luxus/noctalia-kwin](https://github.com/luxus/noctalia-kwin)
instead of hand-applying patches:

```nix
inputs.noctalia-kwin.url = "github:luxus/noctalia-kwin";
inputs.noctalia-kwin.inputs.noctalia.follows = "noctalia";

noctaliaPkg = inputs.noctalia-kwin.packages.${system}.default;
```

Ship a `.desktop` file with **Desktop Actions** (launcher, lock, session menu,
etc.) and register it at session start via `org.kde.kglobalaccel` / `doRegister`
so bindings appear in *System Settings → Shortcuts*.

## Systemd user model

Scope every session daemon to the compositor unit (e.g. `kwin-noctalia.service`),
not bare `graphical-session.target`.

```text
kwin-noctalia.service
├── kwin-noctalia-session-ready.service   (Wayland socket + import-environment)
├── noctalia.service                        (after session-ready)
├── plasma-kded6.service                    (re-scoped PartOf/WantedBy)
├── plasma-xdg-desktop-portal-kde.service
├── plasma-powerdevil.service
├── kwallet-kwin-noctalia.service
├── noctalia-kglobalaccel-register.service
└── kwin-tiling-config.service              (kwriteconfig6: [Tiling] Enabled=true)
```

Upstream Plasma units default to `PartOf=graphical-session.target`. Re-scope them
with a drop-in:

```ini
# plasma-kded6.service.d/kwin-noctalia.conf
[Unit]
PartOf=
After=
PartOf=kwin-noctalia.service
After=kwin-noctalia.service kwin-noctalia-session-ready.service
Requires=kwin-noctalia-session-ready.service
BindsTo=kwin-noctalia.service

[Install]
WantedBy=kwin-noctalia.service
```

**greetd lifecycle:** the session script must not restart the compositor on
SIGHUP. Start `kwin-noctalia.service`, wait for the Wayland socket, then block
until the display manager ends the session. On exit, stop Noctalia and KWin
explicitly — otherwise the next login can hit DRM denied / black screen.

## Minimal NixOS example

Below is a self-contained starting point: flake inputs, a greetd session entry,
Home Manager packages/env, and the critical session script + compositor unit.
Extend with krdp, rounded corners, or a greeter picker as needed.

### Flake inputs

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    kwin-tiling.url = "github:luxus/kwin-tiling";
    noctalia.url = "github:noctalia-dev/noctalia";
    noctalia-kwin.url = "github:luxus/noctalia-kwin";
    noctalia-kwin.inputs.noctalia.follows = "noctalia";
    home-manager.url = "github:nix-community/home-manager";
    home-manager.inputs.nixpkgs.follows = "nixpkgs";
  };
}
```

### Host configuration

```nix
{ inputs, pkgs, ... }:
{
  imports = [
    inputs.kwin-tiling.nixosModules.kwin-tiling
    inputs.home-manager.nixosModules.home-manager
  ];

  # Patched KWin + portals + audio for the compositor unit.
  services.pipewire.enable = true;
  xdg.portal = {
    enable = true;
    extraPortals = [ pkgs.kdePackages.xdg-desktop-portal-kde ];
  };

  services.greetd = {
    enable = true;
    settings = {
      initial_session = {
        # Home Manager installs kwin-noctalia-session into the user profile.
        command = "${config.users.users.YOURUSER.home}/.nix-profile/bin/kwin-noctalia-session";
        user = "YOURUSER";
      };
      # Or use a greeter (noctalia-greeter, regreet, …) that lists wayland-sessions.
    };
  };

  home-manager.users.YOURUSER = import ./home-kwin-noctalia.nix;
}
```

**greetd runs the session script; the script starts systemd user units** — not
raw `kwin_wayland` from the greeter.

### Home Manager module (`home-kwin-noctalia.nix`)

```nix
{ config, inputs, pkgs, lib, ... }:
let
  system = pkgs.stdenv.hostPlatform.system;
  kde = pkgs.kdePackages;

  noctaliaPkg = inputs.noctalia-kwin.packages.${system}.default;
  noctaliaBin = lib.getExe noctaliaPkg;

  kcmQmlPath = lib.concatStringsSep ":" (map (p: "${p}/lib/qt-6/qml") [
    kde.union kde.kwin kde.plasma-desktop kde.systemsettings
  ]);

  kwinWrap = lib.getExe' kde.kwin "kwin_wayland_wrapper";
  kbuildsycoca6 = lib.getExe' kde.kservice "kbuildsycoca6";
  kwriteconfig6 = lib.getExe' kde.kconfig "kwriteconfig6";

  compositorUnit = "kwin-noctalia.service";
  sessionMarker = "kwin-noctalia-active";

  scopeDropIn = ''
    [Unit]
    PartOf=
    After=
    PartOf=${compositorUnit}
    After=${compositorUnit} kwin-noctalia-session-ready.service
    Requires=kwin-noctalia-session-ready.service
    BindsTo=${compositorUnit}
    [Install]
    WantedBy=${compositorUnit}
  '';

  sessionScript = pkgs.writeShellScriptBin "kwin-noctalia-session" ''
    set -euo pipefail
    runtime="''${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

    cleanup() {
      rm -f "$runtime/${sessionMarker}"
      systemctl --user stop --no-block noctalia.service ${compositorUnit} 2>/dev/null || true
      pkill -TERM -x noctalia 2>/dev/null || true
      pkill -TERM -x kwin_wayland 2>/dev/null || true
    }
    trap cleanup EXIT INT TERM HUP

    export KDE_FULL_SESSION=true KDE_SESSION_VERSION=6
    export XDG_CURRENT_DESKTOP=KDE XDG_SESSION_DESKTOP=KDE XDG_SESSION_TYPE=wayland
    export QT_QUICK_CONTROLS_STYLE=org.kde.union
    export QML2_IMPORT_PATH=${kcmQmlPath} NIXPKGS_QT6_QML_IMPORT_PATH=${kcmQmlPath}
    xdgData="$HOME/.local/share:$HOME/.nix-profile/share:/etc/profiles/per-user/$USER/share:/run/current-system/sw/share"
    export XDG_DATA_DIRS="''${XDG_DATA_DIRS:+$xdgData:}$xdgData"

    rm -f "$runtime/${sessionMarker}"
    touch "$runtime/${sessionMarker}"

    systemctl --user import-environment \
      KDE_FULL_SESSION KDE_SESSION_VERSION \
      XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP XDG_SESSION_TYPE \
      XDG_DATA_DIRS QML2_IMPORT_PATH NIXPKGS_QT6_QML_IMPORT_PATH
    dbus-update-activation-environment --systemd -- \
      KDE_FULL_SESSION XDG_CURRENT_DESKTOP XDG_SESSION_TYPE XDG_DATA_DIRS 2>/dev/null || true

    ${kbuildsycoca6} --noincremental >/dev/null 2>&1 || true
    ${kwriteconfig6} --file kwinrc --group Tiling --key Enabled --type bool true

    systemctl --user start --no-block ${compositorUnit}
    for _ in $(seq 1 150); do
      systemctl --user -q is-active ${compositorUnit} && break
      sleep 0.1
    done
    systemctl --user -q is-active ${compositorUnit} || { echo "kwin failed to start" >&2; exit 1; }

    # greetd owns session lifetime — wait for teardown, do not restart kwin on SIGHUP
    sleep infinity
  '';

in
{
  home.packages = [
    sessionScript
    noctaliaPkg
    kde.powerdevil
    kde.kded
    kde.union
    kde.xdg-desktop-portal-kde
    kde.systemsettings
  ];

  programs.noctalia = {
    enable = true;
    package = noctaliaPkg;
    systemd.enable = true;
    systemd.target = compositorUnit;
  };

  xdg.data.files."applications/noctalia.desktop".text = ''
    [Desktop Entry]
    Type=Application
    Name=Noctalia
    Exec=${noctaliaBin}
    Icon=noctalia
    Actions=ToggleLauncher;LockSession
    [Desktop Action ToggleLauncher]
    Name=Toggle Launcher
    Exec=${noctaliaBin} msg panel-toggle launcher
    [Desktop Action LockSession]
    Name=Lock Screen
    Exec=${noctaliaBin} msg session lock
  '';

  # Compositor + session-ready oneshot (install under ~/.config/systemd/user/)
  systemd.user.services.${compositorUnit} = {
    Unit = {
      Description = "KWin Wayland (Noctalia session)";
      After = [ "pipewire.service" "pipewire-pulse.service" ];
      ConditionPathExists = "%t/${sessionMarker}";
    };
    Service = {
      Type = "simple";
      Environment = [
        "KDE_FULL_SESSION=true"
        "KDE_SESSION_VERSION=6"
        "XDG_CURRENT_DESKTOP=KDE"
        "XDG_SESSION_TYPE=wayland"
        "QML2_IMPORT_PATH=${kcmQmlPath}"
      ];
      PassEnvironment = "XDG_DATA_DIRS";
      ExecStart = "${kwinWrap} --xwayland";
      Restart = "no";
      TimeoutStopSec = 5;
    };
  };

  systemd.user.services."kwin-noctalia-session-ready" = {
    Unit = {
      Description = "Import Wayland env after KWin starts";
      PartOf = [ compositorUnit ];
      After = [ compositorUnit ];
    };
    Service = {
      Type = "oneshot";
      RemainAfterExit = true;
      ExecStart = pkgs.writeShellScript "import-env" ''
        set -euo pipefail
        runtime="''${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
        for _ in $(seq 1 150); do
          for s in "$runtime"/wayland-[0-9]*; do
            [ -S "$s" ] && export WAYLAND_DISPLAY="''${s##*/}" && break 2
          done
          sleep 0.1
        done
        systemctl --user import-environment WAYLAND_DISPLAY XDG_DATA_DIRS
        dbus-update-activation-environment --systemd WAYLAND_DISPLAY 2>/dev/null || true
        ${kbuildsycoca6} --noincremental >/dev/null 2>&1 || true
        systemctl --user start --no-block noctalia.service
      '';
    };
    Install.WantedBy = [ compositorUnit ];
  };

  # Re-scope KDE portal/kded/powerdevil to the compositor unit (copy units from
  # kde packages, add .service.d/kwin-noctalia.conf with scopeDropIn above).
  # See systemd.user.services / systemd.user.startServices in your setup.
}
```

### Wayland session desktop entry

Install so greetd / SDDM can list the session (path depends on your DM):

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

Point greetd at the same `Exec=` path, or use a greeter that reads
`wayland-sessions` and lets the user pick.

### Checklist after first login

1. Noctalia bar visible after a few seconds (waits on `session-ready`).
2. *System Settings → Window Management → Tiling* opens and shows layouts.
3. `Meta+Left` / `Meta+Right` focus tiled windows ([shortcuts](usage)).
4. Lock and session menu work (layer-shell patch applied).
5. Logout returns to greeter without a black screen on next login.

## Common pitfalls

| Symptom | Likely cause |
| --- | --- |
| Empty shortcut / app binding menus | `XDG_DATA_DIRS` missing Nix profile paths; run `kbuildsycoca6 --noincremental` |
| Tiling KCM missing or stale options | `QML2_IMPORT_PATH` not set; clear `~/.cache/systemsettings/qmlcache` on Nix |
| Lock screen does nothing on KWin | Noctalia layer-shell patch not applied |
| Logout hangs (greetd) | Session script waiting on compositor restart; stop units explicitly on exit |
| Next login black screen / DRM busy | Previous kwin/noctalia process still holding the GPU |
| Decoration / theme changes do not persist | `kwinrc` symlinked read-only from the store — use a writable config file |

## Next steps

Session packaging as a dedicated flake module (like `kwin-tiling.nixosModules`) is
planned for this repo. Until then, use the example above as a template. Tiling
bugs belong here; session wiring feedback is welcome via GitHub issues on this
repository.