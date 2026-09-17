# Upstream KWin + native dynamic tiling.
#
# Split for maintainability: the brand-new tiling code lives as
# real source under ./src (mirroring kwin's own src/ layout) and is copied into
# the build tree in postPatch; hooks.patch carries ONLY the edits to existing
# kwin files plus the CMake wiring. That keeps the rebase surface (the patch)
# small and the bulk of the feature reviewable as normal source.
#
# Native tiling patched into stock KWin (not a fork). Early ideas from
# gitlab.com/theblackdon/kineticwe; implementation here has diverged since.
# Rounded corners: separate pkgs.kde-rounded-corners effect (see README.md).
#
# Why overrideAttrs (not a fork build): it reuses nixpkgs' exact deps + flags so
# KWin stays in lockstep with the system Plasma version — a nixpkgs bump only
# needs hooks.patch re-tested, not a whole compositor fork rebased.
#
# Tracking KWin 6.8 beta (6.7.90): nixpkgs still ships stable KWin (6.7.x), so
# the version + src are overridden to the beta tarball from KDE's `unstable/`
# tree while reusing nixpkgs' buildInputs/flags. This requires KDE Frameworks
# >= 6.30 (only on nixpkgs master/staging today — see flake.nix), plus two new
# deps KWin 6.8 introduced (libcap for the CAP_SYS_NICE setcap step, and
# KF6::Runner for the Overview effect).
{
  lib,
  kdePackages,
  fetchurl,
  libcap,
}:
let
  # KWin 6.7.90 hard-requires several Plasma-versioned libraries at the matching
  # beta version (KDecoration3, KWayland, KNightTime — find_package(... 6.7.90
  # REQUIRED)), but nixpkgs only ships the stable 6.7.x of the Plasma set. Build
  # the matching betas of these small libraries. (Its other Plasma-versioned
  # deps — Plasma, Breeze, Aurorae, PlasmaActivities — are optional CONFIG
  # lookups, so KWin still configures without their betas.)
  plasmaBeta =
    name: hash:
    kdePackages.${name}.overrideAttrs (_: {
      version = "6.7.90";
      src = fetchurl {
        url = "mirror://kde/unstable/plasma/6.7.90/${name}-6.7.90.tar.xz";
        inherit hash;
      };
    });
  kdecorationBeta = plasmaBeta "kdecoration" "sha256-JIcoWowV38Xp9IDnUbcdxm5fMjjTdwnKeOTpogkJ5qY=";
  knighttimeBeta = plasmaBeta "knighttime" "sha256-V+vvLe+aKr3IWTXRTEslvRgIHG/fp9xn62cG4IsfqiI=";

  # plasma-wayland-protocols 1.22: KWin 6.7.90 and KWayland 6.7.90 require it,
  # but nixpkgs still ships 1.21. It's a tiny data-only package (protocol XML +
  # CMake config), so build the real 1.22 and feed it to both.
  pwp122 = kdePackages.plasma-wayland-protocols.overrideAttrs (_: {
    version = "1.22.0";
    src = fetchurl {
      url = "mirror://kde/stable/plasma-wayland-protocols/plasma-wayland-protocols-1.22.0.tar.xz";
      hash = "sha256-9ihYXEwtXjqfRHpidOL1nXgRJy2lV+dTQeNpSKo/nUM=";
    };
  });

  # KWayland 6.7.90 also needs the pwp 1.22 above at build time.
  kwaylandBeta = (plasmaBeta "kwayland" "sha256-zoc4gSP86xkxWw0WWkE1XPtHPCg54FA14YlkyduwIIA=").overrideAttrs (o: {
    buildInputs = (o.buildInputs or [ ]) ++ [ pwp122 ];
  });

  # kglobalacceld 6.7.90: KWin 6.8 uses new KGlobalAccelD methods (keyEvent,
  # pointerPressed, axisTriggered, resetModifierOnlyState) added this cycle, so
  # the stable 6.7.x header does not compile.
  kglobalacceldBeta = plasmaBeta "kglobalacceld" "sha256-onISzWRx7DI9Jk2+1zKRaRxHrbt4JU9AVHquEcQAOWw=";

  # These Plasma-versioned libs are consumed via direct #include or hard version
  # pins, so the beta must *replace* the stable copy nixpkgs pulls into KWin's
  # closure (appending is not enough — the stable -I dir would shadow the beta).
  betaReplacements = [
    kdecorationBeta
    kwaylandBeta
    knighttimeBeta
    kglobalacceldBeta
    pwp122
  ];
  replacedNames = [
    "kdecoration"
    "kwayland"
    "knighttime"
    "kglobalacceld"
    "plasma-wayland-protocols"
  ];
in
kdePackages.kwin.overrideAttrs (old: {
  version = "6.7.90";

  src = fetchurl {
    url = "mirror://kde/unstable/plasma/6.7.90/kwin-6.7.90.tar.xz";
    hash = "sha256-QR8hq2uVtXwMSO7lnDpE5l1Zl5sAFfXHkwEqMV/QIlg=";
  };

  # nixpkgs' "[NixOS] Unwrap executable name for .desktop search" patch targets
  # src/utils/serviceutils.h, which KWin 6.8 removed — so it fails to apply.
  # Drop it and substitute a 6.8-compatible version (creates nixos_utils.h and
  # unwraps only in waylandwindow.cpp::updateResourceName, which still exists and
  # feeds the resourceClass this project's class rules match on).
  #
  # noctalia-wallpaper-desktop-type: Noctalia paints wallpaper as layer-shell
  # (scope noctalia-wallpaper, optionally suffixed per output). Overview / desktop
  # grid / present windows look for Desktop-type windows (plasmashell desktop) and
  # otherwise show a black background. Kept as a mini-patch, not in hooks.patch.
  patches =
    (builtins.filter (
      p: builtins.match ".*Unwrap-executable-name.*" (baseNameOf (toString p)) == null
    ) (old.patches or [ ]))
    ++ [
      ./patches/nixos-unwrap-6.8.patch
      ./patches/noctalia-wallpaper-desktop-type.patch
      # 6.8 waits on ${LIBEXEC_DIR}/plasma-setup-xwayland before claiming WM_S0.
      # That binary lives in plasma-workspace, so kwin-noctalia never unblocks
      # Xwayland listenfds (Steam hangs in connect() to @/tmp/.X11-unix/X0).
      ./patches/xwayland-missing-setup-script.patch
      # 6.7.90 scripted CrossFade (niri glass-warp, etc.) SIGSEGVs in
      # OffscreenData::paint: redirect -> maybeRender -> drawWindow re-enters
      # the same OffscreenData. Upstream Plasma/6.8 !9898; drop on 6.7.91+.
      ./patches/offscreeneffect-recursion-guard.patch
      ./hooks.patch
    ];

  # KWin 6.8 additions over the stock 6.7.x derivation: libcap (setcap
  # CAP_SYS_NICE on kwin_wayland) and KF6::Runner (required by the Overview
  # effect, KWIN_BUILD_OVERVIEW=ON by default).
  buildInputs =
    (lib.filter (d: !(builtins.elem (lib.getName d) replacedNames)) (old.buildInputs or [ ]))
    ++ betaReplacements
    ++ [
      libcap
      kdePackages.krunner
    ];

  # Drop the new source files into the kwin tree after patches apply. They are
  # additive (new src/tiling, src/tiles/*layoutengine*, src/kcms/tiling); the
  # CMake wiring that references them is in hooks.patch. chmod because store
  # files are read-only.
  postPatch = (old.postPatch or "") + ''
    cp -r ${./src}/. src/
    chmod -R u+w src/

    # KWin 6.8 newly requires Libcap only to run `setcap CAP_SYS_NICE=+ep
    # kwin_wayland` at install time. That step cannot run in the Nix sandbox
    # (no privileges), and nixpkgs applies CAP_SYS_NICE via a runtime wrapper
    # (security.wrappers) instead. Make the dependency optional and neuter the
    # setcap install command to a no-op so the build succeeds; the capability
    # is granted at deploy time.
    sed -i '/set_package_properties(Libcap/,/)/ s/TYPE REQUIRED/TYPE OPTIONAL/' CMakeLists.txt
    sed -i 's|''${SETCAP_EXECUTABLE}|true|' src/CMakeLists.txt
  '';
})
