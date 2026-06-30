# Native KWin tiling — reference

Dynamic tiling (master-stack, stacked, scrolling, centred layouts, gaps,
float/ignore rules, a settings KCM) built **into** KWin. The native impl lifts
the ceiling of the KWin script API.

**Source of truth:** `pkgs/kwin-tiling/` (the package). The flake exposes it as
`packages.<sys>.kwin-tiling`, `overlays.default`, and `nixosModules.kwin-tiling`.
Origin of the C++: the tiling feature of
[gitlab.com/theblackdon/kineticwe](https://gitlab.com/theblackdon/kineticwe) (the
KineticWE fork), curated down to just the feature.

## How it's packaged

`pkgs/kwin-tiling/default.nix` is `kdePackages.kwin.overrideAttrs` — it reuses
nixpkgs' exact deps and build flags so KWin stays in lockstep with the system
Plasma version. It is **not** a fork build.

| Path | Role |
|------|------|
| `pkgs/kwin-tiling/default.nix` | overrideAttrs: applies `hooks.patch`, copies `src/` into the kwin tree in `postPatch` |
| `pkgs/kwin-tiling/src/` | **brand-new** files, mirroring kwin's own `src/` layout (editable as normal source) |
| `pkgs/kwin-tiling/hooks.patch` | **only** the edits to existing kwin files + CMake wiring (~750 lines — the rebase surface) |
| `flake.nix` → `overlays.default` / `nixosModules.kwin-tiling` | sets `kdePackages.kwin = patched`; composing the module onto a host is the on-switch |

The split (new files vendored, hooks as a small patch) keeps the bulk of the
feature reviewable source and the patch — the part that breaks on a kwin bump —
small. Compose the module only onto hosts that want tiling: replacing
`kdePackages.kwin` rebuilds the compositor and its reverse-deps.

## Architecture

```
TilingController (src/tiling/tilingcontroller.cpp)   — singleton on Workspace
  ├─ owns TilingRules (float/ignore by class, utility/dialog/transient)
  ├─ per (output, desktop): a LayoutEngine, held by KWin's TileManager
  └─ reacts to window add/remove, desktop/output move, interactive move/resize

LayoutEngine (src/tiles/layoutengine.h)              — abstract base
  ├─ MasterStackLayoutEngine  (master column + vertical stack)
  └─ StackedLayoutEngine      (all windows full-area, stacked)
```

- The engine only sets **relative** tile geometry (`CustomTile::setRelativeGeometry`);
  KWin's existing `Tile`/`TileManager` drives the actual window geometry, gaps,
  and quick-tile machinery.
- The controller talks to engines through the base interface; layout-specific
  knobs go through generic virtuals (`setPrimarySplit`/`setPrimaryCount`/
  `primarySplit`, `adjustWindowHeight`, `endResizeWindow`, `dropWindow`,
  `pruneEmpty`) so non-master layouts simply no-op.
- Robustness: `Window::outputChanged` purges a window from the old output's
  engines and `pruneEmpty()` drops windowless leaves, so moving a window across
  monitors never leaves a phantom tile. Floating windows are kept above tiled
  ones (`FloatAbove`).

## Shortcuts

Registered in `src/useractions.cpp` (`Workspace::initShortcuts`); all rebindable
in *System Settings → Shortcuts → KWin*.

| Action | Default |
|--------|---------|
| Focus left/right/up/down | `Meta+Arrows` |
| Toggle floating | `Meta+W` |
| Promote to master | `Meta+Shift+Space` |
| Move window prev/next in layout | `Meta+Shift+Left/Right` |
| Move window to left/right output | `Meta+Shift+Ctrl+Left/Right` |
| Increase / decrease master width | `Meta+Ctrl+L` / `Meta+Ctrl+H` |
| Increase / decrease window height | `Meta+Ctrl+K` / `Meta+Ctrl+J` |
| Increase / decrease master count | `Meta+Ctrl+.` / `Meta+Ctrl+,` |
| Retile (rebuild current screen) | `Meta+Shift+R` |
| Cycle layout / Switch to MasterStack / Stacked / Scrolling | unbound |
| Reset sizes (master ratio + heights / column widths) | unbound |
| Toggle zoom (monocle: active window full-screen) | unbound |
| Flip master side / Toggle gaps | unbound |
| Scrolling: center column / cycle column width | unbound |
| Scrolling: consume / expel window into-column | unbound |

> KGlobalAccel only applies a code default when the combo is free **and** the
> action is new in `kglobalshortcutsrc`. An action that previously shipped
> unbound stays unbound on an existing profile — rebind it once in Settings (or
> delete its stale `kglobalshortcutsrc` line). Fresh profiles get the defaults.

Mouse: drag the **master/stack divider** to set the master ratio; **drop** a
window onto another to swap or onto empty space to insert there (master column
left of the divider, stack to the right); drag **horizontal borders within a
column** to adjust per-window heights (MasterStack, Stacked, and Scrolling);
drag **vertical borders** in Scrolling to resize the active column width; other
edges snap.
Window context menu: **Float (Tiling)** (this window) and **Always Float This
App (Tiling)** (class rule).

## Config — `[Tiling]` group in `kwinrc`

Read by the controller on `reconfigure`; also surfaced in the KCM
(*System Settings → Window Management → Tiling*, `kcm_kwin_tiling`). kcfg schema:
`pkgs/kwin-tiling/src/kcms/tiling/tilingsettings.kcfg`.

| Key | Type | Default | Meaning |
|-----|------|---------|---------|
| `Enabled` | bool | `true` | master switch |
| `DefaultLayout` | string | `MasterStack` | layout for new (output, desktop) pairs |
| `EnabledLayouts` | list | `MasterStack,Stacked,Scrolling` | available layouts + cycle order |
| `MasterRatio` | double | `0.5` | master column width fraction (0.1–0.9) |
| `MasterCount` | int | `1` | windows in the master area |
| `DefaultColumnWidth` | double | `0.5` | Scrolling: new column width fraction (0.1–1.0) |
| `FloatAbove` | bool | `true` | keep floating windows stacked above tiled ones |
| `GapLeft/Right/Top/Bottom` | int | `0` | outer gaps |
| `GapBetween` | int | `0` | gap between tiles |
| `Output <name>` subgroup | — | — | per-monitor `DefaultLayout` + gap overrides |

Live changes to master ratio/count (keyboard or divider drag) are written back
to `[Tiling]` so they persist across restart.

## Maintenance

**Editing the feature** — edit the real files under `pkgs/kwin-tiling/src/`
directly. For changes to *existing* kwin files, edit them in a kwin checkout and
regenerate `hooks.patch` (never hand-edit the patch text):

```
# apply current state to a kwin source tree, edit, then:
git diff <baseline> HEAD -- <modified existing files> > pkgs/kwin-tiling/hooks.patch
```

**nixpkgs / Plasma bump** — rebuild `kwin-tiling`. If `hooks.patch` no longer
applies, fix the rejected hunks (the vendored `src/` files are additive and rarely
conflict; the hooks into `window/workspace/useractions/input` are the risk).

**Build / verify:** `nix build .#kwin-tiling` (compiles KWin). Consumers pick up
the new compositor on their next rebuild/switch once they track this flake.
`nix flake check` runs the fast column-math self-check without building KWin.

## Known limitations / backlog

- Master ratio is one global value applied to the active engine (not yet
  per-output/per-desktop).
- Divider-drag ratio is approximate when gaps are non-zero.
- Per-app rules partial (always-tile + layout assign via TilingRules).
- Directional focus/move continue onto the adjacent monitor at a layout edge.
- Smart gaps basic (0 when <=1 window); manual on/off toggle available.
- Configurable new-window placement (postponed).
- Open features: more layouts.

## Collaboration with KineticWE (theblackdon)

This vendoring started from https://gitlab.com/theblackdon/kineticwe. We took the
best of both:
- Adopted the core multi-monitor move fix (pointer-driven moveResizeOutput +
  Wayland output pinning during interactive move, see commit aa26550).
- Kept/extended the richer cursor-aware drop, swap, and cross-output logic.
- Fixed remaining "ghost space" on same-spot releases (use cancelMoveWindow +
  pruneEmpty when the source leaf was left empty by KWin's untile-for-drag).
- Made mouse vertical height resize inside columns actually work (previously
  only keyboard `adjustWindowHeight` did; now `endResizeWindow` derives weights
  from final geometry, same model as keyboard).

The `hooks.patch` ends with a couple of additional hunks for the move
robustness fixes (they can be regenerated cleanly from a kwin tree). The src/
here is the canonical version of the engines + controller.

## Features and improvements added or extended beyond the original fork

The core tiling logic came from the fork, but we added or completed several things:

- **Master count** (number of windows in the primary/master area): full runtime control via keyboard (`Meta+Ctrl+.` / `Meta+Ctrl+,`), KCM, config persistence, and `setPrimaryCount` in the engine. The layout correctly clamps and reflows when the count changes.
- **Master size/ratio** with live persistence: keyboard adjust (`Meta+Ctrl+L/H`), divider drag, and writes back to `[Tiling]` in kwinrc so it survives restarts and applies to new engines.
- **Per-leaf height weights** inside columns (MasterStack): keyboard (`Meta+Ctrl+K/J`) and now **full mouse support** (horizontal splitters inside master or stack column). `endResizeWindow` derives the new weight from the final geometry (same relative model as keyboard). Previously limited or keyboard-only in our integration.
- Robustness fixes not (or only partially) present:
  - Proper handling for "same spot" or minimal drags that would otherwise leave ghost/phantom tiles (uses `cancelMoveWindow` + `pruneEmpty`).
  - Better cross-output and per-output-virtual-desktop move behavior (combined fork's prevention with our drop/swap logic).
  - Defensive `pruneEmpty` calls after structural changes.
- Smart gaps (zero gaps when ≤1 window in a layout).
- The packaging split itself: vendored new source + tiny hooks.patch so we don't carry a full fork.
- Per-output layout choice + cycle, and the full TilingController integration with KWin's move/resize/desktop signals.
- KCM settings (gaps, master count, master ratio) now apply live on reload without logout or session restart. Changes are pushed to existing engines and trigger reflow immediately.
- Per-monitor (per-output) override UI in the KCM now clearly distinguishes global defaults from custom values per monitor, with a "Reset all per-monitor overrides" button to clear them.

See the shipped list and roadmap for the complete current status.

## What we intentionally dropped from the fork

We took **only the tiling feature**, not the full KineticWE compositor experience:

- Binary rename (kineticwe) — we keep the stock `kwin_wayland` binary.
- All distro install scripts and packaging hacks (install-*.sh etc.).
- Toolchain / build system changes.
- Hand-rolled window borders and rounded corners (the fork baked custom corner/outline logic directly into the compositor).

### Rounded corners

Instead of the fork's custom implementation we use the separate, actively maintained
[`kde-rounded-corners`](https://github.com/matinlotfali/KDE-Rounded-Corners) KWin effect
(`pkgs.kde-rounded-corners`).

- Works for **both** `kwin-noctalia` and regular Plasma sessions.
- Configured declaratively via `hjem/desktop/kde-rounded-corners.nix` (oneshot units that
  set the right kwinrc keys + plugin enablement).
- We configure it for clean outlines (size 0 for "square" + thin outline focus hints) while
  still supporting CSD/frameless windows (Obsidian etc.).
- The effect is a normal KWin plugin; no forking of the compositor required.

This is the same pattern we follow everywhere: prefer a small, upstream-friendly patch or
separate effect over baking custom compositor changes.
