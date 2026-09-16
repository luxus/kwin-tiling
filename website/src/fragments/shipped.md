- **Layouts** — MasterStack, Stacked, Scrolling, Centered, Grid, and Columns; pick a default per monitor, cycle at runtime
- **Gaps** — adjustable space between tiles and around screen edges; per-monitor overrides with reset
- **Window rules** — float or ignore windows by app class or title; utilities, dialogs, and transients auto-float
- **Keyboard** — focus in four directions, toggle float, promote to master, toggle master pin, focus last window, move within layout, move across monitors, adjust master width/count; scrolling consume/expel and niri consume-or-expel left/right
- **Mouse** — drag windows to swap or insert above/below; drag dividers to resize master ratio and per-window heights inside a column
- **Settings KCM** — enable layouts, gaps, master ratio/count, scrolling column width, center-focused-column mode, column-width presets, Columns max columns, borderless-when-tiled, per-monitor overrides, and per-desktop layout/sizing in *System Settings → Window Management → Tiling*; changes apply live
- **Autotile** — new windows tile automatically; moving between desktops or monitors reflows and follows focus
- **Persistence** — master ratio, master count, and layout choices survive restarts via `kwinrc`
- **Smart gaps** — gaps collapse to zero when only one window is on screen
- **Master pin** — lock a window to the master slot per output/desktop until unpinned
- **Focus last** — toggle back to the previously active window (`Meta+U`)
- **Borderless when tiled** — optional setting to hide window decorations while tiled
- **Resize axis gating** — split updates only apply to the axis actually dragged
- **Retile** — `Meta+Shift+R` rebuilds the active screen's layout when tiles and windows drift out of sync
- **Packaging** — vendored source + small hooks patch over stock KWin (6.8 beta / 6.7.90); ships as a Nix flake module/overlay
- **Tiling Reflow effect** — optional `kwin-effects-tiling-reflow` plugin; reads `WindowTilingReflowRole` when present
- **Testable pure core** — column/grid/master-stack/scrolling math, slotlist, overflow Path A, layout/sizing
  precedence, scrolling drop-insert, scrollingmove, viewportmath, engineindex,
  columnwidthpresets, scrollingcolumn, consume-or-expel matrix, move FSM, leaf
  cancel rules without linking KWin
- **Session smoke** — `pkgs/kwin-tiling/scripts/session-smoke.md` for post-relogin checks