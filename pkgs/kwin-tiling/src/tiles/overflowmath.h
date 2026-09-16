/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>

// Pure Path A overflow arithmetic (issue #40 / luxusAi#128).
// No KWin / Qt types — unit-tested standalone (tests/overflowmath_test.cpp).
//
// Goal: a Scrolling column may keep its stored width while hanging past the
// output edge, stay assigned to its TileManager output, and neither paint nor
// take input on the neighbour. Instant jump-scroll is fine; do not interpolate
// scrollOffset here.

namespace KWin::overflowmath
{

struct Rect
{
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;

    double left() const
    {
        return x;
    }
    double right() const
    {
        return x + w;
    }
    double top() const
    {
        return y;
    }
    double bottom() const
    {
        return y + h;
    }
};

inline Rect intersect(const Rect &a, const Rect &b)
{
    const double left = std::max(a.left(), b.left());
    const double top = std::max(a.top(), b.top());
    const double right = std::min(a.right(), b.right());
    const double bottom = std::min(a.bottom(), b.bottom());
    if (right <= left || bottom <= top) {
        return {};
    }
    return {left, top, right - left, bottom - top};
}

// CustomTile::setRelativeGeometry stock clamp. Overflow leaves skip the
// [0,1] intersect (and the "right/bottom > 1 → do nothing" early return).
inline Rect customTileGeometry(const Rect &requested, bool allowOverflow)
{
    if (allowOverflow) {
        return requested;
    }
    return intersect(requested, {0.0, 0.0, 1.0, 1.0});
}

// Tile::windowGeometry() stock clamp. Overflow tiles skip the output
// intersect so frameGeometry keeps the full absolute width/height.
inline Rect windowGeometry(const Rect &absolute, const Rect &output, bool allowOverflow)
{
    if (allowOverflow) {
        return absolute;
    }
    return intersect(absolute, output);
}

// WaylandWindow::updateGeometry / setMoveResizeGeometry: overflow windows
// stay on the TileManager output instead of outputAt(center).
inline bool pinToManagerOutput(bool allowOverflow)
{
    return allowOverflow;
}

inline int pinnedOutputId(int managerOutputId, int centerOutputId, bool allowOverflow)
{
    return allowOverflow ? managerOutputId : centerOutputId;
}

// WorkspaceScene::createStackingOrder: hide overflow windows from any view
// that is not their pinned output, so they do not paint on the neighbour.
inline bool paintOnView(bool allowOverflow, bool viewIsPinnedOutput)
{
    return !allowOverflow || viewIsPinnedOutput;
}

// Window::hitTest / isOnOutput: overflow windows only receive input and
// report occupancy on the pinned output.
inline bool hitOnPinnedOutput(bool allowOverflow, bool pointOnPinnedOutput)
{
    return !allowOverflow || pointOnPinnedOutput;
}

inline bool isOnOutput(bool allowOverflow, bool isPinnedOutput, bool geometryIntersects)
{
    return allowOverflow ? isPinnedOutput : geometryIntersects;
}

} // namespace KWin::overflowmath
