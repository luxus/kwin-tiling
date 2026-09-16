/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for scrollingmath.h — Scrolling drop-insert
    (consume into a column vs new-column strip index). Run via tests/run.sh:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/scrollingmath_test \
            pkgs/kwin-tiling/tests/scrollingmath_test.cpp && /tmp/scrollingmath_test
*/

#include "../src/tiles/scrollingmath.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace KWin::scrollingmath;

int main()
{
    // --- in-column Y half: consume index ---
    // Upper half of the first (only) leaf → insert at 0 (above).
    assert(!isLowerHalf(10.0, 0.0, 100.0));
    assert(consumeInsertIndex(0, false) == 0);

    // Lower half of a single-window column → insert at 1 → 2-high column.
    assert(isLowerHalf(50.0, 0.0, 100.0));
    assert(isLowerHalf(75.0, 0.0, 100.0));
    assert(consumeInsertIndex(0, true) == 1);

    // Midpoint counts as lower (append after the target).
    assert(isLowerHalf(50.0, 0.0, 100.0));

    // Third leaf, upper vs lower.
    assert(consumeInsertIndex(2, false) == 2);
    assert(consumeInsertIndex(2, true) == 3);

    // Degenerate geometry: treat as below (append).
    assert(isLowerHalf(0.0, 0.0, 0.0));
    assert(consumeInsertIndex(-1, true) == 0);

    // --- empty-space strip index from cursor X ---
    // Two 0.5 columns, no scroll: midpoints at 0.25 and 0.75.
    const std::vector<double> two = {0.5, 0.5};
    assert(stripInsertIndex(0.10, two, 0.0) == 0); // left of first
    assert(stripInsertIndex(0.24, two, 0.0) == 0); // left half of first
    // Gap between columns (0.5) and right half of first → new column at 1.
    assert(stripInsertIndex(0.40, two, 0.0) == 1);
    assert(stripInsertIndex(0.50, two, 0.0) == 1);
    assert(stripInsertIndex(0.60, two, 0.0) == 1); // left half of second
    assert(stripInsertIndex(0.90, two, 0.0) == 2); // right of last

    // Empty strip → index 0.
    assert(stripInsertIndex(0.5, {}, 0.0) == 0);

    // Single 0.5 column centred: scrollOffset = (0.5 - 1) / 2 = -0.25.
    // View left = 0.25, midpoint = 0.50.
    const std::vector<double> one = {0.5};
    const double centered = (0.5 - 1.0) / 2.0;
    assert(stripInsertIndex(0.10, one, centered) == 0); // empty left of strip
    assert(stripInsertIndex(0.50, one, centered) == 1); // midpoint and right
    assert(stripInsertIndex(0.90, one, centered) == 1); // empty right of strip

    // Scrolled viewport: offset 0.2, same two 0.5 columns.
    // View mids: (-0.2 + 0.25) = 0.05 and (0.3 + 0.25) = 0.55.
    assert(stripInsertIndex(0.00, two, 0.2) == 0);
    assert(stripInsertIndex(0.40, two, 0.2) == 1);
    assert(stripInsertIndex(0.80, two, 0.2) == 2);

    std::puts("scrollingmath: all checks passed");
    return 0;
}
