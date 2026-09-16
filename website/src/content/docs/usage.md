---
title: Usage & Shortcuts
description: Keyboard shortcuts, settings panel, and daily use of native KWin tiling.
---

Tiling works the same whether you run **KDE Plasma + tiling** (normal Plasma
session, easiest start) or a **KWin + Noctalia** custom session. See
[Overview → Two ways to start](/) for which path fits you; this page covers
shortcuts and the settings panel for both. After a system rebuild that replaces
KWin, **relogin** before testing shortcuts — the running compositor does not
hot-reload.

All shortcuts are registered as KWin actions. Rebind them in *System Settings
→ Shortcuts → KWin*.

## Default shortcuts

| Action | Default binding |
| --- | --- |
| Focus left / right / up / down | `Meta+Left` / `Meta+Right` / `Meta+Up` / `Meta+Down` |
| Toggle floating | `Meta+W` |
| Promote to master | `Meta+Shift+Space` |
| Toggle master pin | `Meta+S` |
| Move window prev / next in layout | `Meta+Shift+Left` / `Meta+Shift+Right` |
| Move window left / right / up / down in layout | `Meta+Alt+Left` / `Meta+Alt+Right` / `Meta+Alt+Up` / `Meta+Alt+Down` |
| Move window to left / right monitor | `Meta+Shift+Ctrl+Left` / `Meta+Shift+Ctrl+Right` |
| Increase / decrease master width | `Meta+Ctrl+L` / `Meta+Ctrl+H` |
| Increase / decrease window height | `Meta+Ctrl+K` / `Meta+Ctrl+J` |
| Increase / decrease master count | `Meta+Ctrl+.` / `Meta+Ctrl+,` |
| Retile (rebuild current screen) | `Meta+Shift+R` |
| Focus last window | `Meta+U` |
| Cycle layout | `Meta+Shift+T` |
| Reset sizes | `Meta+Ctrl+0` |
| Toggle zoom (monocle) | `Meta+Shift+Z` |
| Flip master side | `Meta+Shift+F` |
| Toggle gaps | `Meta+Shift+G` |
| Scrolling: center column / cycle column width | `Meta+Shift+C` / `Meta+Shift+V` |
| Scrolling: reverse cycle column width | `Meta+Ctrl+Shift+V` |
| Scrolling: expand column to available width | `Meta+Ctrl+F` |
| Scrolling: consume into column / expel from column | `Meta+Shift+[` / `Meta+Shift+]` |
| Scrolling: consume-or-expel left / right | `Meta+[` / `Meta+]` |
| Switch to MasterStack / Stacked / Scrolling / Centered / Grid / Columns | *(unbound)* |

KGlobalAccel only applies a default when the shortcut is free **and** the
action is new in your profile. If an action was previously unbound, assign it
once in Settings (or remove its stale entry from `kglobalshortcutsrc`).
Consume and expel keep their original action ids (`Meta+Shift+[` / `]`), so
existing bindings are not silently remapped; consume-or-expel is a new pair
(`Meta+[` / `Meta+]`).

### Scrolling consume-or-expel (niri `Mod+[` / `Mod+]`)

| | Left (`Meta+[`) | Right (`Meta+]`) |
| --- | --- | --- |
| **Solo** window in its column | merge into the left neighbour | merge into the right neighbour |
| **Stacked** (two or more tiles) | expel into a new column on the left | expel into a new column on the right |

On the first column, left-merge is a no-op; on the last column, right-merge is
a no-op. Neither crashes. `Meta+Shift+[` / `]` stay **Tiling Consume/Expel**
(niri consume-into-column / expel-from-column aliases).

In **Scrolling**, `Meta+Alt+Up/Down` reorders the focused window **inside its
column** (niri `move-window-up/down`) — two stacked windows swap vertical order
and stay in the same column. `Meta+Alt+Left/Right` slides that **whole column**
along the strip (niri `move-column-left/right`). It does **not** consume or
expel; those stay `Meta+Shift+[` / `]`.

## Mouse

- Drag the **master/stack divider** to set the master column width
- Drag a tiled window **onto another** to swap (middle) or insert above/below
  (top/bottom of the target) in MasterStack/Stacked/Grid/Columns. In **Scrolling**,
  drop on the top or bottom half of a window in **another** column consumes into
  that column at that index (niri-style); drop on a window in the **same** column
  still swaps
- Drag **onto empty space** to insert the window at that position. MasterStack
  uses master vs stack side of the divider; **Scrolling** uses cursor X to pick
  the strip index (a drop on the gap between columns inserts a new column there)
- Drag **horizontal borders inside a column** to resize individual window heights
- Drag **vertical borders in Scrolling or Columns** to resize column width
- Unsupported resize directions snap back

Right-click a window for **Float (Tiling)** (this window) or **Always Float This
App (Tiling)** (permanent class rule).

## Settings panel

*System Settings → Window Management → Tiling*

**Layout & Gaps** — defaults for every monitor, plus per-output overrides:

![Layout and gap settings with per-monitor overrides](/images/kcm-layout-gaps.webp)

**Rules** — float or ignore windows by app class or title:

![Float and ignore rules for apps and windows](/images/kcm-rules.webp)

| Setting | What it does |
| --- | --- |
| Enable tiling | Global on/off switch |
| Available layouts | Which layouts appear in the cycle (MasterStack, Stacked, Scrolling, Centered; Grid and Columns opt-in) |
| Default layout | Layout used on new monitor/desktop pairs |
| Master width | Master column as a fraction of screen width (0.1–0.9) |
| Master count | How many windows sit in the master area |
| Default column width | Scrolling layout: width of new columns |
| Center focused column | Scrolling: `never` (default, scroll to fit), `always` (center on focus; wide columns left-align), `on-overflow` (center when the focused column and its neighbour do not both fit), or `pair-center` (when more than two columns, always leave room for one neighbour of default width beside the active column). `Meta+Shift+C` remains a one-shot center. Peeking columns keep full width ([#40](https://github.com/luxus/kwin-tiling/issues/40) Path A + [#41](https://github.com/luxus/kwin-tiling/issues/41)); fully off-viewport columns stay hidden. |
| Column-width presets | Scrolling layout: widths visited by cycle / reverse cycle |
| Max columns | Columns layout: maximum side-by-side columns (2–5) |
| Gap margins | Left, right, top, bottom screen margins |
| Gap between | Space between adjacent tiles |
| Per-output overrides | Different layout, gaps, or sizing per monitor |
| Per-desktop overrides | Layout and sizing (master ratio/count, column width) per (desktop, monitor) pair |
| New window placement | `kwinrc` only: `NewWindowPlacement=end` (default) appends; `master` promotes new windows to master |

Per-monitor overrides can be reset with the **Reset all per-monitor overrides**
button in the KCM. Per-desktop sizing can be cleared with **Use default sizes**
on that pair.

Settings are stored in `~/.config/kwinrc` under `[Tiling]`:

```ini
[Tiling]
Enabled=true
DefaultLayout=MasterStack
EnabledLayouts=MasterStack,Stacked,Scrolling,Centered
MasterRatio=0.5
MasterCount=1
DefaultColumnWidth=0.5
CenterFocusedColumn=never
ColumnWidthPresets=1/3,1/2,2/3,1
NewWindowPlacement=end
GapBetween=4
GapLeft=8
```

Per-monitor values live under `[Tiling][Output <name>]` subgroups. Per-desktop
layout and sizing live under `[Tiling][DesktopOutput <n>:<output>]`.

Per-app output pinning lives under `[TilingRules]` as `AssignOutput` (e.g.
`AssignOutput=firefox:DP-2`). It is not in the KCM yet.

## Layouts in practice

- **MasterStack** — one or more primary windows on one side, the rest stacked on
  the other. Best for a main app plus side apps.
- **Stacked** — single column, full width, windows stacked vertically.
- **Scrolling** — horizontal strip of columns; viewport scrolls to the active one.
  `Meta+Alt+Up/Down` moves the window inside the column; `Meta+Alt+Left/Right`
  slides the column. Cycle column width through KCM-configured presets
  (`Meta+Shift+V`, reverse `Meta+Ctrl+Shift+V`). `Meta+Shift+[` pulls the first
  window of the next column into the focused column (niri consume-into-column);
  `Meta+Shift+]` expels the bottom tile into a new column to the right.
  `Meta+[` / `Meta+]` are niri consume-or-expel (solo merge / stacked expel).
  Existing column widths are left alone. Optional `CenterFocusedColumn`
  (`never` / `always` / `on-overflow` / `pair-center`) recenters on focus; `Meta+Shift+C` is
  still a one-shot center. `always` peeks neighbours at full width
  ([#40](https://github.com/luxus/kwin-tiling/issues/40) Path A +
  [#41](https://github.com/luxus/kwin-tiling/issues/41)); fully off-viewport
  columns stay hidden.
- **Centered** — master window in the centre, others in left/right stacks.
- **Grid** — opt-in smoothly scaling grid (not in the default `EnabledLayouts`
  list; enable it in the KCM or add `Grid` to `EnabledLayouts`).
- **Columns** — equal-width columns filling the view; extra windows stack (opt-in).

Cycle between enabled layouts with the cycle action, or set a default in the KCM.

## D-Bus (shell widgets)

The compositor publishes `org.kde.KWin.Tiling` at `/Tiling` on the existing
`org.kde.KWin` bus name. Noctalia's KineticWE layouts widget can talk to this
path; kind names are this project's (`MasterStack`, `Stacked`, `Scrolling`,
`Centered`, `Grid`, `Columns`).

```sh
qdbus-qt6 org.kde.KWin /Tiling org.kde.KWin.Tiling.currentLayout
qdbus-qt6 org.kde.KWin /Tiling org.kde.KWin.Tiling.enabledLayouts
qdbus-qt6 org.kde.KWin /Tiling org.kde.KWin.Tiling.setLayout Stacked
qdbus-qt6 org.kde.KWin /Tiling org.kde.KWin.Tiling.cycleLayout
```

`currentLayout` is empty when tiling is disabled. Layout switches also emit
`layoutChanged` so a bar can stay in sync with `Meta+Shift+T`.

## Automatic behaviour

- Tiling is on by default once the module is active
- New windows tile into the layout for their monitor and desktop
- Moving between desktops or monitors retiles and moves focus with the window
- Master ratio, master count, and layout choices persist across restarts

## Animated reflow (optional)

Tiling itself is instant snaps. Motion is an optional desktop effect, not part
of the layout engines.

This flake packages **Tiling Reflow** (`kwin-effects-tiling-reflow`), a fork of
[Geometry Change](https://github.com/peterfajdiga/kwin4_effect_geometry_change)
that reads `WindowTilingReflowRole` when the patched compositor publishes it
(directional slides, reason-based curves) and falls back to geometry-delta
inference on stock KWin or when the hint is missing.

```nix
environment.systemPackages = [
  inputs.kwin-tiling.packages.${system}.kwin-effects-tiling-reflow
];
# or: imports = [ inputs.kwin-tiling.nixosModules.kwin-effects-tiling-reflow ];
```

Then enable *System Settings → Desktop Effects → Tiling Reflow*, or:

```ini
[Plugins]
kwin4_effect_tiling_reflowEnabled=true
```

Leave it off for i3-style instant snaps. Do not also enable upstream Geometry
Change — the two would double-animate the same moves.

## Packaging

Consume the flake and compose the module onto hosts that should run tiling:

```nix
imports = [ inputs.kwin-tiling.nixosModules.kwin-tiling ];
```

Or use the overlay / package directly:

```nix
nixpkgs.overlays = [ inputs.kwin-tiling.overlays.default ];
```

Patching KWin rebuilds the compositor and its reverse-dependencies — only
enable on hosts that actually want native tiling. A binary cache for this repo is
strongly recommended.

## Current limitations

- Divider drag gives an approximate ratio when gaps are non-zero

See [Roadmap](roadmap) for planned improvements.