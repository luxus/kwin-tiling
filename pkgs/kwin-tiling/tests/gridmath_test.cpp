/*
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for gridmath.h — run with:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/gridmath_test \
            pkgs/kwin-tiling/tests/gridmath_test.cpp && /tmp/gridmath_test
*/

#include "../src/tiles/gridmath.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace KWin::gridmath;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

static void assertTilesUnitSquare(const std::vector<Rect> &rects)
{
    for (const Rect &r : rects) {
        assert(r.x >= -1e-9 && r.y >= -1e-9);
        assert(r.x + r.w <= 1.0 + 1e-9);
        assert(r.y + r.h <= 1.0 + 1e-9);
        assert(r.w > 0.0 && r.h > 0.0);
    }
}

int main()
{
    {
        const GridShape s = targetShape(1);
        assert(s.rows == 1 && s.cols == 1 && s.capacity == 1 && s.prevCapacity == 0);
    }
    {
        const GridShape s = targetShape(8);
        assert(s.rows == 3 && s.cols == 3 && s.capacity == 9 && s.prevCapacity == 6);
    }
    {
        const GridShape s = targetShape(13);
        assert(s.rows == 4 && s.cols == 4 && s.capacity == 16 && s.prevCapacity == 12);
    }
    {
        const GridShape s = targetShape(25);
        assert(s.rows == 5 && s.cols == 5 && s.capacity == 25 && s.prevCapacity == 20);
    }

    for (int n = 1; n <= 12; ++n) {
        const GridShape shape = targetShape(n);
        const auto rects = cellRects(n, shape);
        assert(static_cast<int>(rects.size()) == n);
        assertTilesUnitSquare(rects);
    }

    {
        const GridShape shape = targetShape(8);
        const auto rects = cellRects(8, shape);
        assert(approx(rects[0].x, 0.0));
        assert(approx(rects[0].y, 0.0));
        assert(approx(rects[0].w, 1.0 / 3.0));
        assert(approx(rects[0].h, 2.0 / 3.0));
    }

    {
        const auto rects = cellRects(1, targetShape(1));
        assert(rects.size() == 1);
        assert(approx(rects[0].w, 1.0) && approx(rects[0].h, 1.0));
    }

    std::puts("gridmath_test: OK");
    return 0;
}