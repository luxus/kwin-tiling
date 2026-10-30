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
# Tracking KWin git master (Plasma 6.8.80). nixpkgs' Plasma 6.8.x satisfies
# master's PROJECT_DEP_VERSION (6.7.90), so only this src is replaced.
{
  kdePackages,
  fetchFromGitLab,
  libcap,
}:
kdePackages.kwin.overrideAttrs (old: {
  version = "6.8.80";

  src = fetchFromGitLab {
    domain = "invent.kde.org";
    owner = "plasma";
    repo = "kwin";
    rev = "246116fb92c10d814261cd9fcfb4c74c2f9ff4fa";
    hash = "sha256-D7b1HCU54XMDu4ada2ZRdkdfynH5EglDrfGNtd81xec=";
  };

  # nixpkgs' unwrap patch targets src/utils/serviceutils.h, removed in KWin 6.8.
  # Its QPA wrapper patch still uses QLatin1String; master uses QLatin1StringView.
  # noctalia-wallpaper-desktop-type: Noctalia's layer-shell wallpaper is not a
  # plasmashell Desktop window, so Overview paints black without this.
  patches =
    (builtins.filter (
      p:
      let
        n = baseNameOf (toString p);
      in
      builtins.match ".*Unwrap-executable-name.*" n == null
      && builtins.match ".*nixos-wrapper.*" n == null
    ) (old.patches or [ ]))
    ++ [
      ./patches/nixos-unwrap-6.8.patch
      ./patches/nixos-qpa-wrapper.patch
      ./patches/noctalia-wallpaper-desktop-type.patch
      ./hooks.patch
    ];

  # Libcap: KWin master setcaps CAP_SYS_NICE onto kwin_wayland. nixpkgs' kwin
  # derivation does not pull it in. The install step is neutered below.
  buildInputs = (old.buildInputs or [ ]) ++ [ libcap ];

  # Drop the new source files into the kwin tree after patches apply. They are
  # additive (new src/tiling, src/tiles/*layoutengine*, src/kcms/tiling); the
  # CMake wiring that references them is in hooks.patch. chmod because store
  # files are read-only.
  postPatch = (old.postPatch or "") + ''
    cp -r ${./src}/. src/
    chmod -R u+w src/

    # setcap cannot run in the Nix sandbox. nixpkgs grants CAP_SYS_NICE via
    # security.wrappers. Make Libcap optional and turn the install command
    # into a no-op.
    sed -i '/set_package_properties(Libcap/,/)/ s/TYPE REQUIRED/TYPE OPTIONAL/' CMakeLists.txt
    sed -i 's|''${SETCAP_EXECUTABLE}|true|' src/CMakeLists.txt
  '';
})
