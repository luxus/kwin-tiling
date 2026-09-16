/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>

// Pure drop-zone policy: classify a pointer hit on a tiled window as Swap
// vs InsertAbove / InsertBelow, and compute the insert index after the
// dragged window is removed from its source column.
//
// No KWin/Qt — unit-tested in tests/insertpolicy_test.cpp. Engines compose
// this with StackColumn instead of swapping on every drop.

namespace KWin::insertpolicy
{

enum class DropZone {
    Swap,
    InsertAbove,
    InsertBelow,
};

/** Top/bottom fraction of a target rect that maps to insert (middle is Swap). */
inline constexpr double kEdgeBand = 0.25;

inline DropZone classifyRelativeY(double relY)
{
    relY = std::clamp(relY, 0.0, 1.0);
    if (relY < kEdgeBand) {
        return DropZone::InsertAbove;
    }
    if (relY > 1.0 - kEdgeBand) {
        return DropZone::InsertBelow;
    }
    return DropZone::Swap;
}

inline DropZone classifyPoint(double px, double py, double x, double y, double w, double h)
{
    if (h <= 0.0) {
        return DropZone::Swap;
    }
    (void)px;
    (void)w;
    (void)x;
    return classifyRelativeY((py - y) / h);
}

/**
 * Where to insert after removing the source window from its column.
 *
 * @p srcCount includes a ghost (empty) source leaf left by untile-for-drag.
 * @p dropSourceColumn is true when that column will vanish (srcCount <= 1).
 * Same-column removal shifts @p tgtRow down when the source sat above the
 * target; a vanished source column left of the target shifts @p tgtCol down.
 */
struct InsertPos {
    int column = 0;
    int row = 0;
    bool dropSourceColumn = false;
};

inline InsertPos insertPos(int srcCol, int srcRow, int srcCount,
                           int tgtCol, int tgtRow, DropZone zone)
{
    InsertPos p;
    p.dropSourceColumn = (srcCount <= 1);

    int tCol = tgtCol;
    int tRow = tgtRow;
    if (p.dropSourceColumn && srcCol < tgtCol) {
        tCol -= 1;
    }
    if (!p.dropSourceColumn && srcCol == tgtCol && srcRow < tgtRow) {
        tRow -= 1;
    }
    if (tCol < 0) {
        tCol = 0;
    }
    if (tRow < 0) {
        tRow = 0;
    }
    p.column = tCol;
    p.row = (zone == DropZone::InsertAbove) ? tRow : tRow + 1;
    return p;
}

/** Row to insert at when dropping onto a target that is still in place (no prior remove). */
inline int insertRowAtTarget(int targetRow, DropZone zone)
{
    if (targetRow < 0) {
        return 0;
    }
    return (zone == DropZone::InsertAbove) ? targetRow : targetRow + 1;
}

} // namespace KWin::insertpolicy
