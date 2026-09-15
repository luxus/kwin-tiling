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
  kdePackages,
  fetchurl,
  libcap,
}:
kdePackages.kwin.overrideAttrs (old: {
  version = "6.7.90";

  src = fetchurl {
    url = "mirror://kde/unstable/plasma/6.7.90/kwin-6.7.90.tar.xz";
    hash = "sha256-QR8hq2uVtXwMSO7lnDpE5l1Zl5sAFfXHkwEqMV/QIlg=";
  };

  patches = (old.patches or [ ]) ++ [ ./hooks.patch ];

  # KWin 6.8 additions over the stock 6.7.x derivation: libcap (setcap
  # CAP_SYS_NICE on kwin_wayland) and KF6::Runner (required by the Overview
  # effect, KWIN_BUILD_OVERVIEW=ON by default).
  buildInputs = (old.buildInputs or [ ]) ++ [
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

    # KWin 6.7.90 bumps its PlasmaWaylandProtocols floor to 1.22, but nixpkgs
    # still ships 1.21 and KWin 6.7.90 consumes no protocol added in 1.22
    # (verified against the 1.21 protocol set). Relax the version gate until
    # nixpkgs ships plasma-wayland-protocols >= 1.22.
    substituteInPlace CMakeLists.txt \
      --replace-fail "find_package(PlasmaWaylandProtocols 1.22.0" "find_package(PlasmaWaylandProtocols 1.21.0"
  '';
})
