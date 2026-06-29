---
title: Usage & Shortcuts
description: Keyboard shortcuts, KCM, gaps, configuration, and daily use of native KWin tiling.
---

All shortcuts are registered in KWin and rebindable via *System Settings → Shortcuts → KWin*.

## Default shortcuts

| Action                              | Default binding                  |
|-------------------------------------|----------------------------------|
| Focus left/right/up/down            | `Meta+Left/Right/Up/Down`        |
| Toggle floating                     | `Meta+W`                         |
| Promote to master                   | `Meta+Shift+Space`               |
| Move window prev/next in layout     | `Meta+Shift+Left/Right`          |
| Move window to left/right output    | `Meta+Shift+Ctrl+Left/Right`     |
| Increase / decrease master width    | `Meta+Ctrl+L` / `Meta+Ctrl+H`    |
| Increase / decrease master count    | `Meta+Ctrl+.` / `Meta+Ctrl+,`    |
| Cycle layout                        | (unbound)                        |
| Switch to MasterStack / Stacked     | (unbound)                        |

Mouse:

- Drag the **master/stack divider** to set the master ratio interactively.
- Drag a tiled window onto another to swap positions.
- Drag **horizontal borders inside a column** to resize individual window heights (MasterStack only; updates per-leaf weights).
- Other edges / unsupported resizes reflow/snap back.

## KCM / Settings

A dedicated tiling KCM lives at:

*System Settings → Window Management → Tiling*

Controls (from `tilingsettings.kcfg`):

- Enable tiling (global master switch)
- Available layouts (MasterStack, Stacked checkboxes)
- Default layout
- Master width (%)
- Master count
- Gap margins (Left/Right/Top/Bottom)
- Gap between tiles
- Per-output subgroups for overrides (the KCM shows a "Reset all per-monitor overrides" button to clear custom values)

Settings live in `~/.config/kwinrc` under the `[Tiling]` group.

Example keys:

- `Enabled=true`
- `DefaultLayout=MasterStack`
- `EnabledLayouts=MasterStack,Stacked`
- `MasterRatio=0.5`
- `MasterCount=1`
- `GapBetween=4`
- `GapLeft=8` etc.
- Output-specific under `[Tiling][Output HDMI-1]` etc.

## Automatic behavior

- Tiling is enabled by default once the module is active.
- New windows are automatically tiled according to the active layout engine for the (output, desktop).
- Moving windows between desktops or outputs triggers autotile + focus follow.
- Layout state (master count/ratio) persists via kwinrc writes.

## Layouts

- **MasterStack**: one (or N) primary column(s) + a vertical stack on the side.
- **Stacked**: a single full-width column; windows stacked vertically.
- **Scrolling**: PaperWM/niri-style horizontal strip of columns with a scrolling viewport.
- **Centered**: master centred, the rest split into left/right stack columns.

Cycle between enabled layouts with the cycle action (or pick a default in the KCM).

## Packaging / activation

Consume the flake and compose the module onto a host:

- `nixosModules.kwin-tiling` — applies the overlay (replaces `kdePackages.kwin` with the patched build) for that host.
- `overlays.default` / `packages.<sys>.kwin-tiling` — the overlay and package if you'd rather wire them yourself.
- Compose only onto hosts that want tiling — patching kwin rebuilds the compositor and its reverse-deps.

## Limitations (current)

- Master ratio is currently a single value (not per-desktop or per-output).
- Divider drag with non-zero gaps is approximate.

See the [Overview](/) for the full "patching vs forking" notes. Per-leaf height weights inside columns are implemented (mouse + keyboard).

For authoritative maintenance notes see the KWin source and packaging in `pkgs/kwin-tiling` (see [Overview](/)).
