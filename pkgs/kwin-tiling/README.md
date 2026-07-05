# Native KWin tiling — reference

Dynamic tiling (master-stack, stacked, scrolling, centred layouts, gaps,
float/ignore rules, a settings KCM) built **into** KWin. The native impl lifts
the ceiling of the KWin script API.

**Source of truth:** `pkgs/kwin-tiling/` (the package). The flake exposes it as
`packages.<sys>.kwin-tiling`, `overlays.default`, and `nixosModules.kwin-tiling`.
Motivation: native tiling inside KWin (smoother than the script API) via a slim
patch over stock KWin, not a fork. Early ideas from
[gitlab.com/theblackdon/kineticwe](https://gitlab.com/theblackdon/kineticwe);
little of that code remains.

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

## Move/resize robustness

Interactive move and resize got extra attention because native tiling lives in
the compositor hot path:

- Pointer-driven `moveResizeOutput` + Wayland output pinning during interactive
  move (see commit aa26550).
- Cursor-aware drop, swap, and cross-output logic.
- "Ghost space" on same-spot releases: `cancelMoveWindow` + `pruneEmpty` when
  KWin's untile-for-drag leaves an empty source leaf.
- Mouse vertical height resize inside columns: `endResizeWindow` derives weights
  from final geometry (same model as keyboard `adjustWindowHeight`).

The `hooks.patch` carries a couple of additional hunks for move robustness;
regenerate them cleanly from a kwin tree when rebasing.

## Features shipped

- **Master count** — runtime control via keyboard (`Meta+Ctrl+.` / `Meta+Ctrl+,`),
  KCM, config persistence, and `setPrimaryCount`; layout clamps and reflows on
  change.
- **Master size/ratio** — keyboard (`Meta+Ctrl+L/H`), divider drag, persisted to
  `[Tiling]` in kwinrc.
- **Per-leaf height weights** — keyboard (`Meta+Ctrl+K/J`) and mouse (horizontal
  splitters inside master or stack column).
- **Cross-output moves** — per-output virtual-desktop behavior; defensive
  `pruneEmpty` after structural changes.
- Smart gaps (zero when ≤1 window in a layout).
- Per-output layout choice + cycle; full `TilingController` integration with
  KWin's move/resize/desktop signals.
- KCM settings apply live on reload; per-monitor override UI with reset.

See the shipped list and roadmap for the complete current status.

## Rounded corners

Hand-rolled compositor borders are out of scope. Use the separate
[`kde-rounded-corners`](https://github.com/matinlotfali/KDE-Rounded-Corners) KWin
effect (`pkgs.kde-rounded-corners`) — a normal plugin, same pattern as this
patch: small upstream-friendly integration, not a fork.

## Compared to KineticWE

Early inspiration from
[theblackdon/kineticwe](https://gitlab.com/theblackdon/kineticwe) — a fork that
proved native tiling could work inside KWin. We ported ideas and features, not
the fork; little of that code remains.

| | KineticWE fork | this package |
| --- | --- | --- |
| Compositor | entire KWin tree (~3,300 tracked files) | `kdePackages.kwin.overrideAttrs` |
| Files touched | 123 `src/` files diverge from upstream KWin | 37 (22 vendored + 15 in `hooks.patch`) |
| Existing KWin edits | spread across the fork | +468 / −38 lines in 15 files |
| Workarounds dropped | QPainter backend (~20 files, ~1.9k LOC), hand-rolled borders (~500 LOC), install scripts (~2k LOC), `kineticwe` binary | stock `kwin_wayland`; effects as plugins |

14 of our 15 hooked files are the same integration points KineticWE changed for
tiling; the fork also modifies **109 other** `src/` files (render backends,
OpenGL, plugins, scene) that we don't carry. Layout engines set relative
geometry on KWin's `CustomTile` tree — the compositor's own tile machinery
handles gaps, geometry, and rendering, so we don't need a parallel render path
or compositor rebrand.

On a Plasma bump you re-test `hooks.patch`, not an entire fork rebase.
