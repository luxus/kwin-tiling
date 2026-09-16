/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>

// Pure Path A overflow arithmetic (luxusAi#128 / kwin-tiling #40).
//
// Free of KWin / Qt types so nix flake check can lock the go/no-go:
// overflow tiles keep full width; they stay pinned to the TileManager
// output; neighbour views hide them (KWin paints by geometry otherwise).

namespace KWin::overflowmath
{

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;

    double left() const { return x; }
    double top() const { return y; }
    double right() const { return x + w; }
    double bottom() const { return y + h; }
    double centerX() const { return x + w * 0.5; }
    double centerY() const { return y + h * 0.5; }
};

inline Rect intersect(const Rect &a, const Rect &b)
{
    const double l = std::max(a.left(), b.left());
    const double t = std::max(a.top(), b.top());
    const double r = std::min(a.right(), b.right());
    const double btm = std::min(a.bottom(), b.bottom());
    if (r <= l || btm <= t) {
        return {l, t, 0.0, 0.0};
    }
    return {l, t, r - l, btm - t};
}

inline bool containsPoint(const Rect &r, double px, double py)
{
    return px >= r.left() && px < r.right() && py >= r.top() && py < r.bottom();
}

inline bool intersects(const Rect &a, const Rect &b)
{
    return a.left() < b.right() && b.left() < a.right() && a.top() < b.bottom() && b.top() < a.bottom();
}

// CustomTile::setRelativeGeometry [0,1] clamp. Overflow leaves skip it so a
// peeking column keeps Column::width instead of shrinking to the visible sliver.
inline Rect clampRelative(const Rect &geom, bool allowOverflow)
{
    if (allowOverflow) {
        return geom;
    }
    return intersect(geom, {0.0, 0.0, 1.0, 1.0});
}

// Floating-parent CustomTile also intersected the parent (the root is [0,1]).
inline Rect clampToParent(const Rect &geom, const Rect &parent, bool allowOverflow)
{
    if (allowOverflow) {
        return geom;
    }
    return intersect(geom, parent);
}

inline Rect absoluteFromRelative(const Rect &output, const Rect &relative)
{
    return {
        output.x + relative.x * output.w,
        output.y + relative.y * output.h,
        relative.w * output.w,
        relative.h * output.h,
    };
}

// Tile::windowGeometry: skip the output intersect when allowOverflow.
inline Rect windowGeometry(const Rect &absolute, const Rect &output, bool allowOverflow)
{
    if (allowOverflow) {
        return absolute;
    }
    return intersect(absolute, output);
}

// Pin tiled overflow windows to the TileManager output except during
// interactive move (the user may be dragging to another monitor).
inline bool shouldPinOutput(bool tiled, bool allowOverflow, bool interactiveMove)
{
    return tiled && allowOverflow && !interactiveMove;
}

inline int resolveOutputIndex(bool pin, int managerOutput, int centerOutput)
{
    return pin ? managerOutput : centerOutput;
}

// Which output contains a point (first match). -1 if none.
inline int outputIndexAt(const Rect *outputs, int n, double px, double py)
{
    for (int i = 0; i < n; ++i) {
        if (containsPoint(outputs[i], px, py)) {
            return i;
        }
    }
    return -1;
}

// SceneView::shouldHideWindow: overflow tiles are omitted from neighbour
// stacking. WorkspaceScene otherwise paints any item whose frame intersects
// the view viewport — that is the "appear on the right monitor" invariant.
inline bool hideOnView(bool pin, int managerOutput, int viewOutput)
{
    return pin && managerOutput != viewOutput;
}

// Hit-test: a point on the neighbour must not reach an overflow-tiled window.
inline bool hitTestOnOutput(bool pin, const Rect &frame, const Rect &pinnedOutput, double px, double py)
{
    if (pin && !containsPoint(pinnedOutput, px, py)) {
        return false;
    }
    return containsPoint(frame, px, py);
}

// isOnOutput: geometry intersection would report both monitors; pin restricts
// membership to the TileManager output.
inline bool isOnOutput(bool pin, int managerOutput, int queryOutput, const Rect &frame, const Rect &queryGeom)
{
    if (pin) {
        return managerOutput == queryOutput;
    }
    return intersects(frame, queryGeom);
}

} // namespace KWin::overflowmath
