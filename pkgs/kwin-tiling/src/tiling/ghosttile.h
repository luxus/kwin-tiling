/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Pure minimize/maximize leave-rejoin decisions (KineticWE c22d5c2 / issue #30).
// TilingController applies the side effects; this header is the unit-tested
// policy (tests/ghosttile_test.cpp). Mode stays Tiled through leave so restore
// re-joins. Fullscreen is not handled here: it keeps tile membership.

namespace KWin::ghosttile
{

enum class Action {
    Ignore, // not tiled; floating maximize/minimize is none of our business
    Vacate, // leave the layout (ghost-tile); siblings reflow
    Rejoin, // restored; add back unless the other leave-state is still active
};

inline Action classifyMinimize(bool tiled, bool minimized)
{
    if (!tiled) {
        return Action::Ignore;
    }
    return minimized ? Action::Vacate : Action::Rejoin;
}

inline Action classifyMaximize(bool tiled, bool maximized)
{
    if (!tiled) {
        return Action::Ignore;
    }
    return maximized ? Action::Vacate : Action::Rejoin;
}

/** Map-time / retile: already minimized or maximized must not take a slot. */
inline bool shouldTakeTileOnAdd(bool minimized, bool maximized)
{
    return !minimized && !maximized;
}

/**
 * Unminimize/unmaximize rejoin. Skip if the window is still minimized or
 * still maximized — the other state's leave is still in effect (no ghost).
 */
inline bool shouldRejoin(bool tiled, bool minimized, bool maximized)
{
    return tiled && !minimized && !maximized;
}

} // namespace KWin::ghosttile
