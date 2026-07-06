/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <cmath>
#include <limits>
#include <vector>

// Pure nearest-centroid directional search over a set of rects. No KWin/Qt
// types — unit-tested standalone (tests/directionmath_test.cpp).

namespace KWin::directionmath
{

struct Rect
{
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

enum class Direction {
    Left,
    Right,
    Up,
    Down,
};

inline int nearestInDirection(const std::vector<Rect> &rects, int from, Direction direction)
{
    if (from < 0 || from >= static_cast<int>(rects.size())) {
        return -1;
    }
    const Rect &cur = rects[static_cast<size_t>(from)];
    const double cx = cur.x + cur.w / 2.0;
    const double cy = cur.y + cur.h / 2.0;

    int best = -1;
    double bestDist = std::numeric_limits<double>::infinity();
    for (int i = 0; i < static_cast<int>(rects.size()); ++i) {
        if (i == from) {
            continue;
        }
        const Rect &t = rects[static_cast<size_t>(i)];
        const double tx = t.x + t.w / 2.0;
        const double ty = t.y + t.h / 2.0;
        double dist = -1.0;
        switch (direction) {
        case Direction::Left:
            if (tx < cx) {
                dist = (cx - tx) + std::abs(ty - cy) * 0.5;
            }
            break;
        case Direction::Right:
            if (tx > cx) {
                dist = (tx - cx) + std::abs(ty - cy) * 0.5;
            }
            break;
        case Direction::Up:
            if (ty < cy) {
                dist = (cy - ty) + std::abs(tx - cx) * 0.5;
            }
            break;
        case Direction::Down:
            if (ty > cy) {
                dist = (ty - cy) + std::abs(tx - cx) * 0.5;
            }
            break;
        }
        if (dist >= 0.0 && dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

} // namespace KWin::directionmath