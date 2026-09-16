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
| Scrolling: consume / expel window | `Meta+Shift+[` / `Meta+Shift+]` |
| Switch to MasterStack / Stacked / Scrolling / Centered / Grid | *(unbound)* |

KGlobalAccel only applies a default when the shortcut is free **and** the
action is new in your profile. If an action was previously unbound, assign it
once in Settings (or remove its stale entry from `kglobalshortcutsrc`).

In **Scrolling**, `Meta+Alt+Up/Down` reorders the focused window **inside its
column** (niri `move-window-up/down`) — two stacked windows swap vertical order
and stay in the same column. `Meta+Alt+Left/Right` slides that **whole column**
along the strip (niri `move-column-left/right`). It does **not** consume or
expel; those stay `Meta+Shift+[` / `]`.

## Mouse

- Drag the **master/stack divider** to set the master column width
- Drag a tiled window **onto another** to swap positions
- Drag **onto empty space** to insert the window at that position
- Drag **horizontal borders inside a column** to resize individual window heights
- Drag **vertical borders in Scrolling** to resize the active column width
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
| Available layouts | Which layouts appear in the cycle (MasterStack, Stacked, Scrolling, Centered) |
| Default layout | Layout used on new monitor/desktop pairs |
| Master width | Master column as a fraction of screen width (0.1–0.9) |
| Master count | How many windows sit in the master area |
| Default column width | Scrolling layout: width of new columns |
| Center focused column | Scrolling: `never` (default, scroll to fit), `always` (center on focus; wide columns left-align), or `on-overflow` (center when the focused column and its neighbour do not both fit). `Meta+Shift+C` remains a one-shot center. Off-screen columns stay hidden until overflow tiles ([#40](https://github.com/luxus/kwin-tiling/issues/40) Path A). |
| Gap margins | Left, right, top, bottom screen margins |
| Gap between | Space between adjacent tiles |
| Per-output overrides | Different layout, gaps, or sizing per monitor |
| Per-desktop overrides | Layout and sizing (master ratio/count, column width) per (desktop, monitor) pair |

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
GapBetween=4
GapLeft=8
```

Per-monitor values live under `[Tiling][Output <name>]` subgroups. Per-desktop
layout and sizing live under `[Tiling][DesktopOutput <n>:<output>]`.

## Layouts in practice

- **MasterStack** — one or more primary windows on one side, the rest stacked on
  the other. Best for a main app plus side apps.
- **Stacked** — single column, full width, windows stacked vertically.
- **Scrolling** — horizontal strip of columns; viewport scrolls to the active one.
  `Meta+Alt+Up/Down` moves the window inside the column; `Meta+Alt+Left/Right`
  slides the column. Consume/expel is `Meta+Shift+[` / `]`. Optional
  `CenterFocusedColumn` (`never` / `always` / `on-overflow`) recenters on focus;
  `Meta+Shift+C` is still a one-shot center. Without overflow tiles
  ([#40](https://github.com/luxus/kwin-tiling/issues/40) Path A), `always` still
  hides off-screen columns.
- **Centered** — master window in the centre, others in left/right stacks.

Cycle between enabled layouts with the cycle action, or set a default in the KCM.

## Automatic behaviour

- Tiling is on by default once the module is active
- New windows tile into the layout for their monitor and desktop
- Moving between desktops or monitors retiles and moves focus with the window
- Master ratio, master count, and layout choices persist across restarts

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