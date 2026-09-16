/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for columnlayoutmath.h — run with:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/columnlayoutmath_test \
            pkgs/kwin-tiling/tests/columnlayoutmath_test.cpp && /tmp/columnlayoutmath_test
*/

#include "../src/tiles/columnlayoutmath.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace KWin::columnlayoutmath;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

static double sum(const std::vector<double> &w)
{
    double s = 0.0;
    for (double x : w) {
        s += x;
    }
    return s;
}

int main()
{
    assert(clampMaxColumns(0) == kMinMaxColumns);
    assert(clampMaxColumns(3) == 3);
    assert(clampMaxColumns(9) == kMaxMaxColumns);
    assert(kDefaultMaxColumns == 3);

    // Grow columns until the cap; then stack in the focused column.
    {
        const AddTarget a0 = addTarget(0, 3, -1, -1);
        assert(a0.openNewColumn && a0.column == 0);

        const AddTarget a1 = addTarget(1, 3, 0, 0);
        assert(a1.openNewColumn && a1.column == 1);

        const AddTarget a2 = addTarget(2, 3, 1, 0);
        assert(a2.openNewColumn && a2.column == 2);

        const AddTarget a3 = addTarget(3, 3, 1, 0);
        assert(!a3.openNewColumn && a3.column == 1);

        // No focus: fewest (leftmost on ties).
        const AddTarget a4 = addTarget(3, 3, -1, 2);
        assert(!a4.openNewColumn && a4.column == 2);

        // Stale focus falls back to fewest, then 0.
        const AddTarget a5 = addTarget(3, 3, 9, -1);
        assert(!a5.openNewColumn && a5.column == 0);
    }

    {
        assert(fewestColumn({}) == -1);
        assert(fewestColumn({2, 1, 1}) == 1); // leftmost of the two 1s
        assert(fewestColumn({3, 3, 3}) == 0);
        assert(fewestColumn({4, 2, 1}) == 2);
    }

    {
        const auto one = equalWidths(1);
        assert(one.size() == 1 && approx(one[0], 1.0));

        const auto two = equalWidths(2);
        assert(two.size() == 2);
        assert(approx(sum(two), 1.0));
        assert(approx(two[0], 0.5) && approx(two[1], 0.5));

        const auto three = equalWidths(3);
        assert(three.size() == 3);
        assert(approx(sum(three), 1.0));
        assert(equalWidths(0).empty());
    }

    {
        auto w = normalizeWidths({0.2, 0.3, 0.5});
        assert(approx(sum(w), 1.0));
        assert(approx(w[0], 0.2) && approx(w[1], 0.3) && approx(w[2], 0.5));

        auto starved = normalizeWidths({0.01, 0.01, 0.01});
        // After scale they are equal; last stays in range.
        assert(approx(sum(starved), 1.0));
        for (double x : starved) {
            assert(x >= kMinColWidth - 1e-9);
        }

        auto one = normalizeWidths({0.4});
        assert(one.size() == 1 && approx(one[0], 1.0));

        auto zero = normalizeWidths({0.0, 0.0});
        assert(approx(sum(zero), 1.0));
    }

    {
        double left = 0.5;
        double right = 0.5;
        assert(transferWidth(left, right, 0.1));
        assert(approx(left, 0.6) && approx(right, 0.4));
        assert(!transferWidth(left, right, 0.5)); // would starve right
        assert(approx(left, 0.6) && approx(right, 0.4));
        assert(transferWidth(left, right, -0.1));
        assert(approx(left, 0.5) && approx(right, 0.5));
    }

    {
        const auto w = equalWidths(3);
        assert(columnAtX(w, 0.0) == 0);
        assert(columnAtX(w, 0.2) == 0);
        assert(columnAtX(w, 0.5) == 1);
        assert(columnAtX(w, 0.9) == 2);
        assert(columnAtX(w, 1.0) == 2);
        assert(columnAtX({}, 0.5) == -1);
    }

    {
        assert(consumeTargetColumn(0, 1) == -1);
        assert(consumeTargetColumn(0, 3) == 1); // prefer right
        assert(consumeTargetColumn(2, 3) == 1); // last → left
        assert(consumeTargetColumn(1, 3) == 2);

        assert(expelInsertAt(0, 2, 3, 2) == 1); // not last → right
        assert(expelInsertAt(1, 2, 3, 2) == 1); // last → left of it (same insert index)
        assert(expelInsertAt(0, 3, 3, 2) == -1); // at cap
        assert(expelInsertAt(0, 2, 3, 1) == -1); // already alone
    }

    {
        const auto cols = columnRects(equalWidths(2));
        assert(cols.size() == 2);
        assert(approx(cols[0].x, 0.0) && approx(cols[0].w, 0.5));
        assert(approx(cols[1].x, 0.5) && approx(cols[1].w, 0.5));
        const auto rows = stackRects(cols[0], 2);
        assert(rows.size() == 2);
        assert(approx(rows[0].h, 0.5) && approx(rows[1].y, 0.5));
        assert(approx(rows[0].x, 0.0) && approx(rows[0].w, 0.5));
    }

    std::puts("columnlayoutmath_test: OK");
    return 0;
}
