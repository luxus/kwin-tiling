# Session smoke checklist (~15–20 min)

Run after a fresh `kwin-tiling` build is **active in the running session**
(not only after `nix build` / `nh os switch` — the compositor process must be
restarted, usually by **relogin**).

**Current train:** KWin / Plasma **6.7.x** (e.g. 6.7.3).  
**Sessions:** Plasma Wayland **or** KWin + Noctalia (`kwin-noctalia`).

```sh
# Quick sanity (on NixOS after switch)
readlink -f /run/current-system/sw/bin/kwin_wayland   # expect …-kwin-6.7.…
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
- [ ] Focus moves columns without permanently losing windows (off-viewport hide OK)
- [ ] `Meta+Shift+C` / `Meta+Shift+V` center / cycle column width
- [ ] `Meta+Shift+[` / `Meta+Shift+]` consume / expel (UX may still be rough)

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
