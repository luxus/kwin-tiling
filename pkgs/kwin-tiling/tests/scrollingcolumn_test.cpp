/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Pure Scrolling consume-into-column / expel-from-column (issue #44 / W1-3).
    Proof: two columns of one window each; consume into column → one stacked
    column; remaining widths unchanged.

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/scrollingcolumn_test \
            pkgs/kwin-tiling/tests/scrollingcolumn_test.cpp && /tmp/scrollingcolumn_test
*/

#include "../src/tiles/scrollingcolumn.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace KWin::scrollingcolumn;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    // --- Proof: two columns of one window each; consume → one stacked column ---
    {
        Layout layout;
        layout.columns = {{{10}, 0.4}, {{20}, 0.6}};
        layout.active = 0;
        assert(consumeIntoColumn(layout));
        assert(layout.columns.size() == 1);
        assert((layout.columns[0].windows == std::vector<int>{10, 20}));
        assert(approx(layout.columns[0].width, 0.4)); // dest width unchanged
    }

    // Source column with leftover tiles stays; its width is untouched.
    {
        Layout layout;
        layout.columns = {{{1}, 0.3}, {{2, 3, 4}, 0.5}, {{5}, 0.7}};
        layout.active = 0;
        const double w0 = layout.columns[0].width;
        const double w1 = layout.columns[1].width;
        const double w2 = layout.columns[2].width;
        assert(consumeIntoColumn(layout));
        assert(layout.columns.size() == 3);
        assert((layout.columns[0].windows == std::vector<int>{1, 2}));
        assert((layout.columns[1].windows == std::vector<int>{3, 4}));
        assert((layout.columns[2].windows == std::vector<int>{5}));
        assert(approx(layout.columns[0].width, w0));
        assert(approx(layout.columns[1].width, w1));
        assert(approx(layout.columns[2].width, w2));
    }

    // Last column / single column: consume is a no-op.
    {
        Layout one;
        one.columns = {{{1}, 0.5}};
        one.active = 0;
        assert(!consumeIntoColumn(one));
        assert(one.columns.size() == 1);

        Layout last;
        last.columns = {{{1}, 0.4}, {{2}, 0.6}};
        last.active = 1;
        assert(!consumeIntoColumn(last));
        assert(last.columns.size() == 2);
        assert(approx(last.columns[0].width, 0.4));
        assert(approx(last.columns[1].width, 0.6));
    }

    // Expel last tile to the right; remaining source width unchanged.
    {
        Layout layout;
        layout.columns = {{{10, 20}, 0.4}, {{30}, 0.7}};
        layout.active = 0;
        assert(expelFromColumn(layout));
        assert(layout.columns.size() == 3);
        assert((layout.columns[0].windows == std::vector<int>{10}));
        assert((layout.columns[1].windows == std::vector<int>{20}));
        assert((layout.columns[2].windows == std::vector<int>{30}));
        assert(approx(layout.columns[0].width, 0.4));
        assert(approx(layout.columns[1].width, 0.4)); // copy, not resize
        assert(approx(layout.columns[2].width, 0.7));
    }

    // Solo column: expel is a no-op (already its own column).
    {
        Layout layout;
        layout.columns = {{{10}, 0.5}, {{20}, 0.5}};
        layout.active = 0;
        assert(!expelFromColumn(layout));
        assert(layout.columns.size() == 2);
    }

    // Consume then expel round-trips two windows; dest width is preserved.
    {
        Layout layout;
        layout.columns = {{{10}, 0.4}, {{20}, 0.6}};
        layout.active = 0;
        assert(consumeIntoColumn(layout));
        assert(expelFromColumn(layout));
        assert(layout.columns.size() == 2);
        assert((layout.columns[0].windows == std::vector<int>{10}));
        assert((layout.columns[1].windows == std::vector<int>{20}));
        assert(approx(layout.columns[0].width, 0.4));
        assert(approx(layout.columns[1].width, 0.4));
    }

    std::puts("scrollingcolumn_test: OK");
    return 0;
}
