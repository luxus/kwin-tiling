# kwin-tiling

Native dynamic tiling built **into** KWin — master-stack, stacked, scrolling, and
centred layouts, gaps, float/ignore window rules, and a settings KCM — packaged
as a small `overrideAttrs` + `hooks.patch` over stock `kdePackages.kwin` (not a
compositor fork).

It is **not** a fork: the bulk of the feature is vendored as normal source under
`pkgs/kwin-tiling/src/` (mirroring KWin's own layout) and copied into the build
tree; `hooks.patch` carries **only** the edits to existing KWin files plus the
CMake wiring. A nixpkgs/Plasma bump just needs that small patch re-tested.

See the docs site under `website/` (Overview / Features / Usage / Roadmap) for
the full feature list and shortcuts, and `pkgs/kwin-tiling/README.md` for
implementation/maintenance notes.

## Use it from your flake

```nix
{
  inputs.kwin-tiling.url = "github:luxus/kwin-tiling";

  # In a NixOS host config:
  imports = [ inputs.kwin-tiling.nixosModules.kwin-tiling ];
  # ^ replaces kdePackages.kwin with the patched build for this host.
}
```

Or just take the overlay / package directly:

```nix
nixpkgs.overlays = [ inputs.kwin-tiling.overlays.default ];   # kdePackages.kwin -> patched
# or
environment.systemPackages = [ inputs.kwin-tiling.packages.${system}.kwin-tiling ];
```

Enable tiling at runtime via `~/.config/kwinrc`:

```ini
[Tiling]
Enabled=true
```

…or in *System Settings → Window Management → Tiling*.

> Patching KWin rebuilds the compositor and its reverse-deps, so compose the
> module only onto the hosts that actually want native tiling — not globally.
> Wiring a binary cache for this repo is strongly recommended; otherwise every
> consumer rebuilds KWin from source.

## Build & check

```sh
nix build .#kwin-tiling     # the patched compositor (long: compiles KWin)
nix flake check             # fast: runs the pure column-math self-check, no KWin build
```

## Maintenance

Edit the feature directly under `pkgs/kwin-tiling/src/`. For changes to
*existing* KWin files, edit them in a KWin checkout and regenerate
`pkgs/kwin-tiling/hooks.patch` (never hand-edit the patch text). On a
nixpkgs/Plasma bump, rebuild and fix any rejected hunks — the vendored `src/`
files are additive and rarely conflict; the hooks into
`window`/`workspace`/`useractions`/`input`/`tiles` are the risk surface.

## Origin

The tiling feature was extracted from
[theblack-don/kwin-we](https://github.com/theblack-don/kwin-we) (the "KineticWE"
fork), curated down to just the dynamic-tiling feature — minus the binary
rename, distro/toolchain hacks, and hand-rolled borders/rounded corners. For
rounded corners use the separate
[kde-rounded-corners](https://github.com/matinlotfali/KDE-Rounded-Corners)
effect.
