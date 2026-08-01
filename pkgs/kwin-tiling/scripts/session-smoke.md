# Session smoke checklist (~15 min)

Run after installing a fresh `kwin-tiling` build (e.g. `nix build .#kwin-tiling`
and switch the host module). Tick each item.

## Setup

- [ ] Log into Plasma Wayland (or KWin+Noctalia) with tiling enabled
- [ ] Open 3+ normal windows on one monitor (MasterStack layout)
- [ ] Note: Keep Above via window menu should still work for any window

## 1. Flip master + directional focus

- [ ] `Meta+Shift+F` (flip master) — master column moves to the other side
- [ ] `Meta+Left/Right/Up/Down` focus follows **geometry** (after flip, Left from
      stack does not jump to a master that is visually on the right)
- [ ] With 2+ masters (`Meta+Ctrl+.`), Up/Down move within the master column

## 2. Cross-monitor drag

- [ ] Drag a tiled window to another monitor and release
- [ ] Source monitor reflows with **no empty/phantom slot**
- [ ] Window is tiled on the destination (not floating unless rules say so)
- [ ] Drop on another tiled window swaps/inserts reasonably

## 3. Toggle tiling off/on (KCM or kwinrc)

- [ ] Disable tiling (*System Settings → Window Management → Tiling* or
      `[Tiling] Enabled=false` + reload)
- [ ] Windows leave the tile grid; decorations return if borderless-when-tiled
      was on
- [ ] Re-enable tiling — previously tiled windows return; **manual** Meta+W
      floats stay floating

## 4. Float + Keep Above

- [ ] `Meta+W` float a tiled window — sizes under cursor, not stuck fullscreen
- [ ] Window menu **Keep Above** still toggles independently (not forced by float)
- [ ] `Meta+W` again re-tiles

## 5. Quick regression

- [ ] `Meta+Shift+R` retile recovers a weird layout without crash
- [ ] Scrolling layout: open several windows, focus moves columns without
      disappearing windows (hidden off-viewport is OK)

## Result

| Date | Build/commit | Pass? | Notes |
|------|--------------|-------|-------|
|      |              |       |       |
