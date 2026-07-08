/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "directionmath.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

// Pure master-stack layout geometry + directional focus over those rects.
// No KWin/Qt — unit-tested standalone (tests/masterstackmath_test.cpp).
// Mirrors MasterStackLayoutEngine::reflowMasterStack so focus tests can assert
// flip-master / multi-master behaviour without building the compositor.

namespace KWin::masterstackmath
{

using Rect = directionmath::Rect;
using Direction = directionmath::Direction;

/**
 * Relative (0..1) rects for a classic master-stack layout.
 * Window order is layout order: indices [0, masters) are masters, the rest stack.
 * When @p masterOnRight is true, masters occupy the right column.
 */
inline std::vector<Rect> layoutRects(int count, int masterCount, double masterRatio, bool masterOnRight)
{
    std::vector<Rect> out;
    if (count <= 0) {
        return out;
    }
    out.resize(static_cast<size_t>(count));
    if (count == 1) {
        out[0] = {0.0, 0.0, 1.0, 1.0};
        return out;
    }

    const int masters = std::min(std::max(masterCount, 1), count - 1);
    const double masterWidth = std::clamp(masterRatio, 0.1, 0.9);
    const double stackWidth = 1.0 - masterWidth;
    const double masterX = masterOnRight ? stackWidth : 0.0;
    const double stackX = masterOnRight ? 0.0 : masterWidth;

    auto fillColumn = [&](int first, int last, double x, double w) {
        const int n = last - first;
        if (n <= 0) {
            return;
        }
        const double h = 1.0 / static_cast<double>(n);
        for (int i = 0; i < n; ++i) {
            out[static_cast<size_t>(first + i)] = {x, h * i, w, h};
        }
    };

    fillColumn(0, masters, masterX, masterWidth);
    fillColumn(masters, count, stackX, stackWidth);
    return out;
}

/** Index of the window in @p direction from @p from, or -1 at the edge. */
inline int windowInDirection(const std::vector<Rect> &rects, int from, Direction direction)
{
    if (rects.empty()) {
        return -1;
    }
    if (from < 0 || from >= static_cast<int>(rects.size())) {
        return 0;
    }
    return directionmath::nearestInDirection(rects, from, direction);
}

/**
 * When entering an output from @p entryDirection (the direction focus moved),
 * pick the window nearest the shared edge. E.g. moving Right onto this output
 * enters from the left edge → prefer the left-most window.
 */
inline int entryWindowIndex(const std::vector<Rect> &rects, Direction entryDirection)
{
    if (rects.empty()) {
        return -1;
    }
    int best = 0;
    double bestScore = 0.0;
    bool first = true;
    for (int i = 0; i < static_cast<int>(rects.size()); ++i) {
        const Rect &r = rects[static_cast<size_t>(i)];
        const double cx = r.x + r.w / 2.0;
        const double cy = r.y + r.h / 2.0;
        double score = 0.0;
        switch (entryDirection) {
        case Direction::Right: // enter from left → smallest x
            score = -cx;
            break;
        case Direction::Left: // enter from right → largest x
            score = cx;
            break;
        case Direction::Down: // enter from top → smallest y
            score = -cy;
            break;
        case Direction::Up: // enter from bottom → largest y
            score = cy;
            break;
        }
        if (first || score > bestScore) {
            bestScore = score;
            best = i;
            first = false;
        }
    }
    return best;
}

} // namespace KWin::masterstackmath
