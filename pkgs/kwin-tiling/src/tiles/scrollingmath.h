/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <vector>

// Pure Scrolling drop-insert arithmetic. No KWin/Qt types — unit-tested
// standalone (tests/scrollingmath_test.cpp).
//
// Distinct from Columns InsertAbove/Below (#32): this is the Scrolling strip
// (viewport-relative X + in-column Y half), not a shared drop-zone policy.

namespace KWin::scrollingmath
{

// True when the cursor is in the lower half of a target tile (height <= 0
// treats the drop as below, i.e. append after that leaf).
inline bool isLowerHalf(double cursorY, double top, double height)
{
    if (height <= 0.0) {
        return true;
    }
    return (cursorY - top) >= 0.5 * height;
}

// Leaf index at which to consume a dropped window into the target's column.
// Upper half → insert at the target (above it); lower half → after it.
inline int consumeInsertIndex(int targetLeafIndex, bool lowerHalf)
{
    if (targetLeafIndex < 0) {
        return 0;
    }
    return lowerHalf ? targetLeafIndex + 1 : targetLeafIndex;
}

// Column index at which to insert a *new* column on an empty-space drop.
// @p relX is the cursor's viewport-relative X (0 = left of the work area).
// @p widths are each column's width in view-width fractions; @p scrollOffset
// is the engine's viewport left in the same units (may be negative when the
// strip is centred). Inserts after every column whose midpoint sits to the
// left of the cursor, so a drop on the gap between two columns lands between
// them.
inline int stripInsertIndex(double relX, const std::vector<double> &widths, double scrollOffset)
{
    if (widths.empty()) {
        return 0;
    }
    int index = 0;
    double stripX = 0.0;
    for (const double w : widths) {
        const double viewMid = (stripX - scrollOffset) + w / 2.0;
        if (relX >= viewMid) {
            ++index;
        }
        stripX += w;
    }
    return std::clamp(index, 0, static_cast<int>(widths.size()));
}

} // namespace KWin::scrollingmath
