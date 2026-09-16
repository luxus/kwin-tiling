/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for ghosttile.h (minimize/maximize leave-rejoin).
*/

#include "../src/tiling/ghosttile.h"

#include <cassert>
#include <cstdio>

using namespace KWin::ghosttile;

int main()
{
    // Floating maximize/minimize is none of tiling's business.
    assert(classifyMinimize(false, true) == Action::Ignore);
    assert(classifyMinimize(false, false) == Action::Ignore);
    assert(classifyMaximize(false, true) == Action::Ignore);
    assert(classifyMaximize(false, false) == Action::Ignore);

    // Tiled minimize leaves; restore wants rejoin.
    assert(classifyMinimize(true, true) == Action::Vacate);
    assert(classifyMinimize(true, false) == Action::Rejoin);

    // Tiled maximize leaves (ghost-tile, same as minimize); unmaximize rejoins.
    assert(classifyMaximize(true, true) == Action::Vacate);
    assert(classifyMaximize(true, false) == Action::Rejoin);

    // Map-time / retile: created already minimized or maximized → no slot.
    assert(!shouldTakeTileOnAdd(true, false));
    assert(!shouldTakeTileOnAdd(false, true));
    assert(!shouldTakeTileOnAdd(true, true));
    assert(shouldTakeTileOnAdd(false, false));

    // Rejoin only when tiled and neither leave-state is still active.
    assert(shouldRejoin(true, false, false));
    assert(!shouldRejoin(false, false, false)); // floating
    assert(!shouldRejoin(true, true, false)); // still minimized
    assert(!shouldRejoin(true, false, true)); // still maximized
    assert(!shouldRejoin(true, true, true)); // both
    // Unmaximize while minimized must not create a ghost tile.
    assert(classifyMaximize(true, false) == Action::Rejoin);
    assert(!shouldRejoin(true, true, false));
    // Unminimize while maximized must not create a ghost tile.
    assert(classifyMinimize(true, false) == Action::Rejoin);
    assert(!shouldRejoin(true, false, true));

    std::puts("ghosttile_test: OK");
    return 0;
}
