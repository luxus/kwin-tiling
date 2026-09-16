# Session smoke checklist (~15–20 min)

Run after a fresh `kwin-tiling` build is **active in the running session**
(not only after `nix build` / `nh os switch` — the compositor process must be
restarted, usually by **relogin**).

**Current train:** KWin / Plasma **6.8 beta (6.7.90)**.  
**Sessions:** Plasma Wayland **or** KWin + Noctalia (`kwin-noctalia`).

```sh
# Quick sanity (on NixOS after switch)
readlink -f /run/current-system/sw/bin/kwin_wayland   # expect …-kwin-6.7.90…
# Pure suite (no session needed)
bash pkgs/kwin-tiling/tests/run.sh
# or: nix flake check
```

## Setup

- [ ] Relogin into Plasma Wayland **or** KWin+Noctalia after the host switch
- [ ] Tiling enabled (`[Tiling] Enabled=true` or KCM; luxusAi oneshot does this)
- [ ] Open **3+** normal windows on one monitor in **MasterStack**
- [ ] Note: window menu **Keep Above** still works independently of float

## 1. Flip master + directional focus

- [ ] `Meta+Shift+F` (flip master) — master column moves to the other side
- [ ] `Meta+Left/Right/Up/Down` focus follows **geometry** (after flip, Left from
      the stack does not jump to a master that is visually on the right)
- [ ] With 2+ masters (`Meta+Ctrl+.`), Up/Down move within the master column

## 2. Cross-monitor drag (move FSM)

- [ ] Drag a tiled window to another monitor and release
- [ ] Source monitor reflows with **no empty/phantom slot**
- [ ] Window is tiled on the destination (unless float rules say otherwise)
- [ ] Drop on another tiled window swaps/inserts reasonably
- [ ] Same-monitor drop on empty space inserts (no ghost empty tile)

## 3. Toggle tiling off/on (KCM or kwinrc)

- [ ] Disable tiling (*System Settings → Window Management → Tiling* or
      `[Tiling] Enabled=false` + reconfigure)
- [ ] Windows leave the tile grid; decorations return if borderless-when-tiled
      was on
- [ ] Re-enable tiling — previously tiled windows return; **manual** `Meta+W`
      floats stay floating

## 4. Float + Keep Above

- [ ] `Meta+W` float a tiled window — sizes under cursor, not stuck fullscreen
- [ ] Window menu **Keep Above** still toggles independently (float does **not**
      force Keep Above)
- [ ] `Meta+W` again re-tiles

## 5. Layout cycle + zoom

- [ ] `Meta+Shift+T` cycles enabled layouts without crash
- [ ] `Meta+Shift+Z` zoom/monocle expands active window; again restores layout
- [ ] Switch to **Grid** (if enabled in KCM) places windows in a grid

## 6. Scrolling (if exercised)

- [ ] Switch to Scrolling; open several windows
- [ ] Focus along a **5-column** strip at default width **1/3**: unfocused
      columns keep their width; neighbours **peek at full size** (not resized
      to the visible sliver). Fully off-viewport columns stay hidden (monocle
      `Meta+Shift+Z` still hides siblings). Peeking neighbour (~40% past the
      output edge) must **not** appear on the adjacent monitor or change
      `output()` (Path A / #40 + #41).
- [ ] `Meta+Shift+C` / `Meta+Shift+V` center / cycle column width
- [ ] KCM `Center focused column`: `never` (default fit), `always` recenters on
      every focus (wide columns left-aligned), `on-overflow` centers only when
      the focused column and its neighbour do not both fit. Apply round-trips.
      Peeking neighbours keep full width (Path A / #40 + #41). Fully
      off-viewport columns stay hidden.
- [ ] `Meta+Ctrl+Shift+V` reverse-cycles only the configured presets (e.g. `0.25,0.5`)
- [ ] Two single-window columns: `Meta+Shift+[` consume-into-column → one stacked column; remaining widths unchanged
- [ ] `Meta+Shift+]` expel-from-column pushes the bottom tile out to the right; remaining widths unchanged
- [ ] `Meta+Ctrl+F` expand column to available width (two 1/3 columns: focused
      fills remainder, neighbour width unchanged; alone toggles full width)

### 6a. Directional move (issue #42)

Three-column fixture: open **three** windows (each in its own column), then
focus the **left** column and `Meta+Shift+[` consume-into-column so that
column has **two stacked** windows and a neighbour column remains.

- [ ] Focus the **upper** window of the stacked pair; `Meta+Alt+Down`
- [ ] Expected: the two windows **swap vertical order**; the column stays put
      (neighbour columns do not move; the pair is still the same column)
- [ ] `Meta+Alt+Up` restores the previous vertical order
- [ ] `Meta+Alt+Left` / `Meta+Alt+Right` **slides the whole column** along the
      strip (niri `move-column-left/right`). Does **not** consume or expel —
      those stay `Meta+Shift+[` / `]`

### 6b. Scrolling mouse drop-insert (W1-4)

Two single-window columns (A | B), then a third (C) if needed. No ghost tiles
after any drop (`cancelMove` / `pruneEmpty`).

- [ ] Drag C onto the **lower half** of B → B becomes a 2-high column (C below B);
      C’s source column is gone
- [ ] Drag onto the **upper half** of a window in another column → consumed
      **above** that window
- [ ] Drag onto the **gap between columns** (empty space) → a **new column**
      inserts at that strip index (cursor X), not always next to the active
      column
- [ ] Drag onto another window **in the same column** still **swaps**

## 7. Promote to master (reorder, not swap)

- [ ] Open **4** tiled windows in **MasterStack** (order A master, then B, C, D in the stack)
- [ ] Focus C or D (not adjacent to master) and `Meta+Shift+Space` promote
- [ ] Expected: promoted window is master; former master and intermediates **shift down**
      (A,B,C,D + promote C → **C,A,B,D**). Not a pairwise swap (which would yield C,B,A,D)
- [ ] With 2 masters (`Meta+Ctrl+.`), promote a stack window: it becomes first master;
      previous masters shift (not swap with only the first)

## 8. Quick regression

- [ ] `Meta+Shift+R` retile recovers a weird layout without crash
- [ ] KCM opens and shows current options (after Nix rebuild: if stale, wipe
      `~/.cache/systemsettings/qmlcache` and `~/.cache/kcmshell6/qmlcache`)
- [ ] Per-Desktop tab: set a master ratio on desktop 1, switch to desktop 2,
      confirm desktop 2 still uses the default; `Meta+Ctrl+L` on desktop 1
      does not change desktop 2

## 9. xwaylandvideobridge (#29)

NixOS Plasma autostarts `xwaylandvideobridge`. Without the built-in ignore it
tiles as a black box covering a monitor.

- [ ] After login, no black fullscreen tile on any output
- [ ] `xwaylandvideobridge` is not in the tiling layout (ignore, not float)
- [ ] Screen sharing into an X11 capture app still works (bridge is not
      Hidden=true — we ignore tiling, we do not kill the process)
- [ ] If WM_CLASS arrives late, the window is untilled once the class/role is
      known (no leftover stretched box)

## 10. Maximize leave/rejoin (#30)

- [ ] Open 3+ tiled windows on one monitor in MasterStack
- [ ] Maximize one (`double-click title` or maximize shortcut) — siblings reflow
      to fill; **no empty/ghost slot**
- [ ] Unmaximize — window re-joins the layout (appended; no crash)
- [ ] Minimize/restore still vacates and re-joins (no regression)

## 11. Fullscreen desktop-switch (#31)

- [ ] Fullscreen a tiled window (app F11 or KWin fullscreen)
- [ ] Switch to another desktop and back
- [ ] Window stays fullscreen (not force-resized back onto its tile)
- [ ] Exit fullscreen — window restores to its tile

## 12. Authoritative tile association (#14) + reverse index (#15)

- [ ] Tiled window reports as tiled in KWin scripting: `window.tile` is non-null
      (Overview / window rules / effects that key off the tile association)
- [ ] Tile a window, switch to another virtual desktop and back — it still shows
      as tiled (association is not dropped on the inactive desktop)
- [ ] `Meta+W` float clears the association; re-tile restores it
- [ ] `Meta+Shift+T` layout cycle: windows stay tiled, no crash, no lost slots
- [ ] Directional focus (`Meta+Arrows`) and move still follow the layout after
      a cycle (index stays in sync with add/remove/switch)


## KWin + Noctalia extras (way 2 only)

- [ ] Noctalia bar appears after session-ready
- [ ] Lock + session menu work
- [ ] Logout returns to greeter; next login has no black screen / DRM denied
- [ ] Overview / desktop grid / present windows show the Noctalia wallpaper
      (not a black background). Stock KWin maps layer-shell scope
      `noctalia-wallpaper*` to Desktop type.

## Result

| Date | Build/commit / kwin path | Session | Pass? | Notes |
|------|--------------------------|---------|-------|-------|
|      |                          |         |       |       |

## Links

- Docs: [KWin + Noctalia session](../../../website/src/content/docs/session.md)
- Package notes: `pkgs/kwin-tiling/README.md`
- Production session packaging: [luxusAi `kwin-noctalia-session`](https://github.com/luxus/luxusAi/tree/main/pkgs/kwin-noctalia-session)
