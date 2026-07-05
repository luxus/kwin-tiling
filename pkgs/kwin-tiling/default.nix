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
{ kdePackages }:
kdePackages.kwin.overrideAttrs (old: {
  patches = (old.patches or [ ]) ++ [ ./hooks.patch ];

  # Drop the new source files into the kwin tree after patches apply. They are
  # additive (new src/tiling, src/tiles/*layoutengine*, src/kcms/tiling); the
  # CMake wiring that references them is in hooks.patch. chmod because store
  # files are read-only.
  postPatch = (old.postPatch or "") + ''
    cp -r ${./src}/. src/
    chmod -R u+w src/
  '';
})
