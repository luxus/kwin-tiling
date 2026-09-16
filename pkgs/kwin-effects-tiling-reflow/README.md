# Tiling Reflow — KWin desktop effect

Fork of [peterfajdiga/kwin4_effect_geometry_change](https://github.com/peterfajdiga/kwin4_effect_geometry_change)
(v1.5) that **reads `WindowTilingReflowRole` when present** and falls back to
geometry-delta inference when it is not.

This is an optional plugin. Tiling is correct with the effect disabled (instant
snaps). Animation timing lives here, not in layout engines or `hooks.patch`.

Flake attr: `packages.<system>.kwin-effects-tiling-reflow` (and the default
overlay's `kwin-effects-tiling-reflow`).

## Enable

Install the package so it is on `XDG_DATA_DIRS`, then:

```ini
[Plugins]
kwin4_effect_tiling_reflowEnabled=true
```

in `~/.config/kwinrc`, or *System Settings → Desktop Effects → Tiling Reflow*.

Do **not** also enable upstream Geometry Change — both would animate the same
`windowFrameGeometryChanged` events.

## Hint contract (`WindowTilingReflowRole`)

Published by the patched compositor immediately before a tiling
`moveResize` (`publishTilingReflowHint` in `tilingreflow.cpp`). JS effects read:

```js
effect.readWindowData(window, Effect.WindowTilingReflowRole)
```

Payload is a `QVariantMap` / JS object. Integer enums match
`pkgs/kwin-tiling/src/tiling/tilingreflow.h`:

| Field | Type | Values |
| --- | --- | --- |
| `reason` | int | `Reflow=0`, `Add=1`, `Remove=2`, `Migrate=3`, `GapChange=4`, `LayoutSwitch=5`, `Retile=6`, `DesktopSwitch=7`, `Float=8`, `Swap=9`, `Insert=10`, `Resize=11`, `Reorder=12` |
| `direction` | int | `None=0`, `FromLeft=1`, `FromRight=2`, `FromAbove=3`, `FromBelow=4` |
| `layout` | int | `MasterStack=0`, `Stacked=1`, `Scrolling=2`, `Centered=3`, `Grid=4` |
| `groupId` | int | reflow generation (shared by windows in one batch) |
| `staggerIndex` | int | 0..n-1 within that batch |

`direction` follows `inferReflowDirection`: `FromRight` means the window center
moved **+x** (rightward travel). The effect starts the window to the *left* of
the new geometry so it slides right into place. v2 engine `hintFor()` overrides
can set a direction that does not match the raw delta; the effect still treats
the enum as travel direction.

On stock KWin (role enum absent) or when the map is missing, the effect uses the
upstream translation+scale-from-old-rect path (`OutExpo`).

This effect **clears** the role after reading it so a later geometry event in
the same window does not reuse a stale hint.

## Reason-based motion

| Reason | Behaviour |
| --- | --- |
| `Add` / `Remove` / `Swap` / `Insert` / `Reorder` / `Migrate` / `Retile` / `Reflow` | Cardinal slide on the hinted axis (`OutCubic`); optional stagger |
| `Resize` / `GapChange` | Geometry-accurate scale+translate (ignore direction) |
| `LayoutSwitch` | Cross-fade + InOutCubic; ignore direction |
| `Float` | Opacity fade + scale/translate from the real delta |
| `DesktopSwitch` | Shorter duration (60%); cardinal slide only if direction ≠ None |

Interactive user move/resize is skipped (same as geometry_change).

## Defaults

- Duration 220 ms (upstream was 250, `OutExpo`)
- Exclusions: `krunner,yakuake,plasmashell,org.kde.plasmashell`
- Prefer hints: on
- Stagger: 16 ms × `staggerIndex`
- Enabled by default: **no**

## Tests

```sh
node pkgs/kwin-effects-tiling-reflow/tests/reflowanimation_test.js
# or: nix flake check  (tiling-reflow-js)
```
