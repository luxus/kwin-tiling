# Native KWin tiling — reference

Dynamic tiling (master-stack, stacked, scrolling, centred, and grid layouts,
gaps, float/ignore rules, a settings KCM) built **into** KWin. The native impl
lifts the ceiling of the KWin script API.

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
| `pkgs/kwin-tiling/default.nix` | overrideAttrs: applies `hooks.patch` plus `patches/`, copies `src/` into the kwin tree in `postPatch` |
| `pkgs/kwin-tiling/src/` | **brand-new** files, mirroring kwin's own `src/` layout (editable as normal source) |
| `pkgs/kwin-tiling/hooks.patch` | **only** the edits to existing kwin files + CMake wiring (~1,000 lines — the rebase surface) |
| `pkgs/kwin-tiling/patches/` | stock-kwin mini-patches kept out of `hooks.patch` (NixOS unwrap; Noctalia wallpaper → Desktop) |
| `flake.nix` → `overlays.default` / `nixosModules.kwin-tiling` | sets `kdePackages.kwin = patched`; composing the module onto a host is the on-switch |

The split (new files vendored, hooks as a small patch) keeps the bulk of the
feature reviewable source and the patch — the part that breaks on a kwin bump —
small. Compose the module only onto hosts that want tiling: replacing
`kdePackages.kwin` rebuilds the compositor and its reverse-deps.

## Architecture

```
TilingController (src/tiling/tilingcontroller.cpp)   — singleton on Workspace
  ├─ owns TilingRules (float/ignore by class, utility/dialog/transient)
  ├─ pure helpers: movefsm (move finish), sizingpolicy, suspendpolicy
  ├─ per (output, desktop): one LayoutEngine on KWin's TileManager
  └─ window add/remove, desktop/output move, interactive move/resize

LayoutEngine (src/tiles/layoutengine.h)              — abstract base
  │   shared: takeOwnershipOfRoot, endResize gate, reflowZoomed (monocle)
  ├─ MasterStackLayoutEngine  (MasterStack + Centered kinds)
  ├─ StackedLayoutEngine      (single full-area StackColumn)
  ├─ GridLayoutEngine         (StackColumn + gridmath slot order)
  └─ ScrollingLayoutEngine    (many StackColumns + viewport; isolated)

StackColumn (src/tiles/stackcolumn.h)                — shared vertical primitive
  └─ height weights via columnmath; order/weight via slotlist; move cancel via movestate
```

**Composition rule (new layouts):** compose `StackColumn` + pure math headers.
Do **not** fork vertical leaf lifecycle into each engine. Cross-layout quirks
(zoom, resize gating) live on `LayoutEngine`. Scrolling keeps viewport state
(`scrollOffset`, column widths, consume/expel) private — never leak into
MasterStack/Stacked/Grid. Absolute geometry, gaps, and quick-tile stay on
KWin's `Tile`/`TileManager`; engines only set relative geometry.

- **Tile association (#14):** `StackColumn` calls `requestTile(leaf)` when a leaf
  takes a window, and `requestTileAssociation` on inactive desktops (no configure).
  Scripting `window.tile` reads `requestedTile`.
- **Reverse index (#15):** `TilingController` maps `Window*` → engine so
  `layoutEngineForWindow` is O(1). `LayoutEngine::contains()` is the non-allocating
  membership check.
- Pure, KWin-free arithmetic (unit-tested): `columnmath`, `masterstackmath`,
  `gridmath`, `directionmath`, `slotlist`, `movestate`, `leafcolumn`, `movefsm`,
  `sizingpolicy`, `suspendpolicy`, `tilingconfig`, `scrollingmove`, `viewportmath`,
  `engineindex`, `columnwidthpresets`, `scrollingcolumn`, `overflowmath`.
- Kind switch **replaces** the engine and re-adds windows; durable layout
  memory is keyed by output/desktop id, not engine pointer.
- Cross-monitor moves: cancel source leaf, drop on destination — no phantoms.
  Floating never forces Keep Above.

## Shortcuts

Registered in `src/useractions.cpp` (`Workspace::initShortcuts`); all rebindable
in *System Settings → Shortcuts → KWin*.

| Action | Default |
|--------|---------|
| Focus left/right/up/down | `Meta+Arrows` |
| Toggle floating | `Meta+W` |
| Promote to master | `Meta+Shift+Space` |
| Toggle master pin | `Meta+S` |
| Move window prev/next in layout | `Meta+Shift+Left/Right` |
| Move window left/right/up/down in layout | `Meta+Alt+Arrows` |
| Move window to left/right output | `Meta+Shift+Ctrl+Left/Right` |
| Increase / decrease master width | `Meta+Ctrl+L` / `Meta+Ctrl+H` |
| Increase / decrease window height | `Meta+Ctrl+K` / `Meta+Ctrl+J` |
| Increase / decrease master count | `Meta+Ctrl+.` / `Meta+Ctrl+,` |
| Retile (rebuild current screen) | `Meta+Shift+R` |
| Focus last window | `Meta+U` |
| Cycle layout | `Meta+Shift+T` |
| Reset sizes (master ratio + heights / column widths) | `Meta+Ctrl+0` |
| Toggle zoom (monocle: active window full-screen) | `Meta+Shift+Z` |
| Flip master side | `Meta+Shift+F` |
| Toggle gaps | `Meta+Shift+G` |
| Scrolling: center column / cycle column width | `Meta+Shift+C` / `Meta+Shift+V` |
| Scrolling: reverse cycle column width | `Meta+Ctrl+Shift+V` |
| Scrolling: consume into column / expel from column | `Meta+Shift+[` / `Meta+Shift+]` |
| Switch to MasterStack / Stacked / Scrolling / Centered / Grid | unbound |

In **Scrolling**, `Meta+Alt+Up/Down` reorders the window **inside its column**
(niri `move-window-up/down`) and does not slide columns. `Meta+Alt+Left/Right`
slides the **whole column** along the strip (niri `move-column-left/right`).
Consume/expel stays `Meta+Shift+[` / `]` and is a different action.

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
| `EnabledLayouts` | list | `MasterStack,Stacked,Scrolling,Centered` | available layouts + cycle order (Grid opt-in) |
| `MasterRatio` | double | `0.5` | master column width fraction (0.1–0.9) |
| `MasterCount` | int | `1` | windows in the master area |
| `DefaultColumnWidth` | double | `0.5` | Scrolling: new column width fraction (0.1–1.0) |
| `CenterFocusedColumn` | string | `never` | Scrolling: `never` (fit-scroll), `always` (center on focus; wide columns left-align), or `on-overflow` (center when the focused column and its neighbour do not both fit). `Meta+Shift+C` stays a one-shot center. Path A overflow (#40) lets peeking columns keep full width without migrating; fully off-viewport columns stay hidden until #41. |
| `ColumnWidthPresets` | list | `1/3,1/2,2/3,1` | Scrolling: cycle/reverse-cycle widths (fractions, `1/3`, or percents). Full width is a preset. |
| `BorderlessWhenTiled` | bool | `false` | hide window decorations on tiled windows |
| `NewWindowPlacement` | string | `end` | `master` promotes new windows to master (master-style layouts; respects an active master pin); `end` appends them |
| `GapLeft/Right/Top/Bottom` | int | `0` | outer gaps |
| `GapBetween` | int | `0` | gap between tiles |
| `Output <name>` subgroup | — | — | per-monitor layout, gap, and sizing overrides |
| `DesktopOutput <desktop>:<output>` subgroup | — | — | per-(desktop, monitor) layout and sizing overrides (`DefaultLayout`, `MasterRatio`, `MasterCount`, `DefaultColumnWidth`) |

Live changes to master ratio/count (keyboard or divider drag) are written back
to `[Tiling][DesktopOutput <desktop>:<output>]` so each virtual desktop keeps
its own values (falling back to `[Tiling][Output <name>]` or global `[Tiling]`
when that pair is unknown).

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

**Build / verify:** `nix build .#kwin-tiling` (compiles KWin; tracks **KWin 6.8
beta / 6.7.90**, overriding `kdePackages.kwin` + its Plasma-versioned deps to the
beta and requiring KDE Frameworks ≥ 6.30 — the flake tracks nixpkgs `master`
until 6.30 reaches `nixos-unstable`). Consumers pick up the new compositor
on their next rebuild/switch once they track this flake — then **relogin**.
`nix flake check` / `tests/run.sh` run the pure suite (see Tests below) without
building KWin. Session regression: `scripts/session-smoke.md`. Production
KWin+Noctalia session packaging: [luxusAi](https://github.com/luxus/luxusAi)
`kwin-noctalia-session`.

## Known limitations / backlog

- Divider-drag ratio is approximate when gaps are non-zero.
- Per-app rules: always-tile + float/ignore via TilingRules. Class match is
  exact or trailing-`*` prefix (not substring). `[TilingRules] AssignOutput`
  pins a class to a monitor, e.g.
  `AssignOutput=firefox:DP-2,org.kde.konsole:HDMI-A-1` (applied to new windows;
  falls back to normal placement when the output is disconnected).
  `xwaylandvideobridge` / `org.kde.xwaylandvideobridge` are always ignored
  (built-in; NixOS Plasma ships the bridge). Late `windowClassChanged` untiles
  if the class arrives after map.
- Live `[Tiling] Enabled=false` detaches tiled windows and restores borders.
- Directional focus/move continue onto the adjacent monitor at a layout edge.
- Smart gaps basic (0 when ≤1 window); manual on/off toggle available.
- Next: scrolling layout polish (consume/expel UX). Path A overflow (#40) is
  in: peeking columns keep full width without migrating. Hide-for-offscreen
  remains until #41. `center-focused-column` never/always/on-overflow is in
  kcfg/KCM.
- Session smoke checklist: `pkgs/kwin-tiling/scripts/session-smoke.md`

## Tests

Two layers, matching #7. Part A is what `nix flake check` runs today.

### Pure (no KWin) — Part A

```sh
pkgs/kwin-tiling/tests/run.sh    # all *_test.cpp via g++
# or: nix flake check
```

Covers geometry (`columnmath`, `gridmath`, `masterstackmath`, `directionmath`,
`overflowmath`), StackColumn order/weight (`slotlist`), layout + sizing precedence
(`tilingconfig`), Scrolling viewport modes (`viewportmath`), in-column move
(`scrollingmove`), Window→engine reverse index (`engineindex`), move cancel
(`movestate`, `leafcolumn`), and controller policy (`movefsm`, `sizingpolicy`,
`suspendpolicy`, `classmatch`). No compositor link.

### KWin integration (Part B, follow-up)

Not wired yet. #7 Part B needs `BUILD_TESTING` on the kwin derivation, a vendored
`autotests/integration/` test modeled on KWin's `tiles_test.cpp`, a one-line
`hooks.patch` CMake registration, and `doCheck` with
`ctest -R kwin-testNativeTiling --output-on-failure` (never a bare `ctest` —
KWin's full suite needs a GPU). Expected rects for that first test should come
from `columnmath::distribute` / `masterstackmath`, not hand-derived numbers.
Until that lands, `nix build .#kwin-tiling` compiles the compositor only.

## Move/resize robustness

Interactive move and resize got extra attention because native tiling lives in
the compositor hot path:

- Pointer-driven `moveResizeOutput` + Wayland output pinning during interactive
  move (`window.cpp`, `waylandwindow.cpp` hunks in `hooks.patch`).
- Cursor-aware drop, swap, and cross-output logic.
- "Ghost space" on same-spot releases: `cancelMoveWindow` + `pruneEmpty` when
  KWin's untile-for-drag leaves an empty source leaf.
- Mouse vertical height resize inside columns: `endResizeWindow` derives weights
  from final geometry (same model as keyboard `adjustWindowHeight`).

### Desktop-switch and tile-geometry hooks

Three `hooks.patch` hunks touch shared KWin code paths (not just tiling-prefixed
actions). Design choices documented here so they survive the next rebase:

- **`updateWindowVisibilityAndActivateOnDesktopChange`** — keeps the upstream
  `isOnOutput(output)` filter so each per-output desktop switch only walks
  windows on that output. Tiled windows still get an explicit
  `moveResize(tile->windowGeometry())` when their desktop becomes active, so
  background reflows stay correct without widening the loop to every window on
  every monitor. Fullscreen windows are skipped (`!isFullScreen()`): they keep
  tile membership so they restore on exit, and force-resizing them on desktop
  return would destile the fullscreen geometry.
- **`Tile::setRelativeGeometry`** — keeps upstream's `isActive()` guard so
  geometry changes on invisible desktops do not push Wayland configure round-trips
  to hidden windows (and quick-tile users are unaffected). The desktop-activation
  resync above is the paired fix.
- **`windowToDesktop` / `activeWindowToDesktop`** — honors
  `options->isRollOverDesktops()` instead of hardcoding wrap-around. The
  i3/dwm-style "moved window takes focus on the new desktop" behavior runs only
  when native tiling is enabled (`TilingController::isEnabled()`), so users who
  disabled `[Tiling] Enabled` keep stock Plasma focus behavior.

### Path A overflow (Scrolling, issue #40) — GO

Peeking columns can keep full width with ~40% past the output edge **without**
migrating to the neighbour. Instant jump-scroll stays; `scrollOffset` is not
interpolated. This is a stock `hooks.patch`, not a render-backend fork.

**Go.** The failing invariants (CustomTile `[0,1]` clamp, `windowGeometry()`
output intersect, `outputAt(center)` / `outputsIntersecting` migration, neighbour
paint) are all gated on `Tile::allowOverflow()`, set on the Scrolling root and
inherited by leaves. Proof: `tests/overflowmath_test.cpp` (1/3 column, 40% past
a 1920px edge → width stays 640; pin stays home; no paint/input on neighbour).

Hooks sketch (scrolling-gated; files + conditions):

| File | Condition | What |
| --- | --- | --- |
| `scrollinglayoutengine.cpp` `attach` | Scrolling root | `root->setAllowOverflow(true)` (inherit to leaves) |
| `layoutengine.cpp` `takeOwnershipOfRoot` | other layouts | `setAllowOverflow(false)` |
| `tiles/tile.cpp` `windowGeometry()` | `m_allowOverflow` | skip `intersected(output->geometryF())` |
| `tiles/customtile.cpp` `setRelativeGeometry` | `allowOverflow()` | skip `[0,1]` intersect, `right/bottom > 1` early return, and Floating parent intersect |
| `window.cpp` `tilingPinnedOutput()` | tile `allowOverflow()` | return `tile->manager()->output()` |
| `waylandwindow.cpp` `updateGeometry` | pinned && not interactive move | `m_output = pinned` (no `outputAt(center)`) |
| `waylandwindow.cpp` `updateClientOutputs` | pinned && not interactive move | `setOutputs({pinned})` (no `outputsIntersecting`) |
| `window.cpp` `setMoveResizeGeometry` | pinned && not interactive | `setMoveResizeOutput(pinned)` |
| `window.cpp` `isOnOutput` / `hitTest` | pinned | occupancy and clicks only on the pinned output |
| `scene/workspacescene.cpp` `createStackingOrder` | pinned && view ≠ pin | skip the window so it does not **paint** on the neighbour |

Not a Hard no: KWin still composites by geometry, but `createStackingOrder`
plus Wayland surface pin is enough to keep overflow off the neighbour without
scene-graph work. Fully off-viewport columns remain `setHidden` until #41.

Regenerate `hooks.patch` from a matching KWin tree when rebasing (see
Maintenance above).

## Features shipped

- **Grid layout** — smoothly-interpolating grid layout kind (`gridmath.h` +
  `GridLayoutEngine`); enable via KCM, cycle or switch at runtime.
- **Master pin** — sticky master per output/desktop (`Meta+S`).
- **Focus last** — toggle to previously active window (`Meta+U`).
- **Borderless when tiled** — optional `BorderlessWhenTiled` KCM setting.
- **Resize axis gating** — per-axis split updates with start-geometry snapshot.
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
- Per-desktop layout **and** sizing overrides (master ratio/count, scrolling
  column width) via the same `DesktopOutput N:name` groups as layout choice.

See the shipped list and roadmap for the complete current status.

## Rounded corners

Hand-rolled compositor borders are out of scope. Use the separate
[`kde-rounded-corners`](https://github.com/matinlotfali/KDE-Rounded-Corners) KWin
effect (`pkgs.kde-rounded-corners`) — a normal plugin, same pattern as this
patch: small upstream-friendly integration, not a fork.

## Reflow animation (optional desktop effect)

Layout engines only publish **hints**; they do not own duration or easing.
`WindowTilingReflowRole` is a `QVariantMap` set on the `EffectWindow` immediately
before a tiling `moveResize` (`tilingreflow.h` / `publishTilingReflowHint`).

JS/QML effects read it as:

```js
effect.readWindowData(window, Effect.WindowTilingReflowRole)
```

| Field | Meaning |
| --- | --- |
| `reason` | Why this `moveResize` ran (`Reflow`, `Add`, `Remove`, `LayoutSwitch`, …) |
| `direction` | Dominant travel (`FromLeft`/`FromRight`/`FromAbove`/`FromBelow`/`None`) |
| `layout` | `LayoutKind` of the engine that reflowed |
| `groupId` | Generation counter for one reflow batch |
| `staggerIndex` | Index within that batch (0..n-1) |

If no effect reads the role, tiling behaviour is unchanged. This repo packages a
geometry_change fork as `kwin-effects-tiling-reflow` that uses the map when
present and infers motion from the geometry delta when it is absent. Details:
`pkgs/kwin-effects-tiling-reflow/README.md`.

## Compared to KineticWE

Early inspiration from
[theblackdon/kineticwe](https://gitlab.com/theblackdon/kineticwe) — a fork that
proved native tiling could work inside KWin. We ported ideas and features, not
the fork; little of that code remains.

| | KineticWE fork | this package |
| --- | --- | --- |
| Compositor | entire KWin tree (~3,300 tracked files) | `kdePackages.kwin.overrideAttrs` |
| Files touched | 123 `src/` files diverge from upstream KWin | 62 (44 vendored + 18 in `hooks.patch`) |
| Existing KWin edits | spread across the fork | +552 / −33 lines in 18 files |
| Workarounds dropped | QPainter backend (~20 files, ~1.9k LOC), hand-rolled borders (~500 LOC), install scripts (~2k LOC), `kineticwe` binary | stock `kwin_wayland`; effects as plugins |

The 44 vendored files are everything under `pkgs/kwin-tiling/src/` (40 `.cpp`/`.h`/`.qml` plus 4 KCM/CMake glue files). Counts drift as layouts and helpers are added; re-count with `find pkgs/kwin-tiling/src -type f | wc -l` and `grep -c '^diff --git' pkgs/kwin-tiling/hooks.patch`.

Most of our 18 hooked files are the same integration points KineticWE changed for
tiling; the fork also modifies **100+ other** `src/` files (render backends,
OpenGL, plugins, scene) that we don't carry. Layout engines set relative
geometry on KWin's `CustomTile` tree — the compositor's own tile machinery
handles gaps, geometry, and rendering, so we don't need a parallel render path
or compositor rebrand.

On a Plasma bump you re-test `hooks.patch`, not an entire fork rebase.
