/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for viewportmath.h (Scrolling center-focused-column).
    No KWin/Qt:

        g++ -std=c++20 -O2 -Wall -o /tmp/viewportmath_test \
            pkgs/kwin-tiling/tests/viewportmath_test.cpp && /tmp/viewportmath_test
*/

#include "../src/tiles/viewportmath.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace KWin::viewportmath;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    // Parse: kcfg strings, aliases, unknown → never.
    assert(parseCenterFocusedColumn("never") == CenterFocusedColumn::Never);
    assert(parseCenterFocusedColumn("Never") == CenterFocusedColumn::Never);
    assert(parseCenterFocusedColumn("always") == CenterFocusedColumn::Always);
    assert(parseCenterFocusedColumn("ALWAYS") == CenterFocusedColumn::Always);
    assert(parseCenterFocusedColumn("on-overflow") == CenterFocusedColumn::OnOverflow);
    assert(parseCenterFocusedColumn("on_overflow") == CenterFocusedColumn::OnOverflow);
    assert(parseCenterFocusedColumn("onOverflow") == CenterFocusedColumn::OnOverflow);
    assert(parseCenterFocusedColumn("") == CenterFocusedColumn::Never);
    assert(parseCenterFocusedColumn("nope") == CenterFocusedColumn::Never);
    assert(std::string(centerFocusedColumnToString(CenterFocusedColumn::Never)) == "never");
    assert(std::string(centerFocusedColumnToString(CenterFocusedColumn::Always)) == "always");
    assert(std::string(centerFocusedColumnToString(CenterFocusedColumn::OnOverflow)) == "on-overflow");

    // Four 0.5 columns: strip total 2.0. Column 1 sits at left=0.5.
    const std::vector<double> halfs{0.5, 0.5, 0.5, 0.5};
    assert(approx(totalWidth(halfs), 2.0));
    assert(approx(columnLeft(halfs, 0), 0.0));
    assert(approx(columnLeft(halfs, 1), 0.5));
    assert(approx(columnLeft(halfs, 3), 1.5));

    // never: already fully visible → do not move.
    {
        const double cur = 0.0; // viewport [0, 1); col 0 [0, 0.5) is visible
        assert(approx(scrollOffsetForFocus(halfs, 0, -1, cur, CenterFocusedColumn::Never), 0.0));
        // col 1 [0.5, 1.0) also fully visible at offset 0
        assert(approx(scrollOffsetForFocus(halfs, 1, 0, cur, CenterFocusedColumn::Never), 0.0));
    }

    // never: column 3 [1.5, 2.0) is off the right of offset 0 → snap so right=1.0+offset
    {
        const double next = scrollOffsetForFocus(halfs, 3, 2, 0.0, CenterFocusedColumn::Never);
        assert(approx(next, 1.0)); // right 2.0 → offset 1.0
    }

    // never: column 0 off the left of offset 1.0 → snap to left
    {
        const double next = scrollOffsetForFocus(halfs, 0, 1, 1.0, CenterFocusedColumn::Never);
        assert(approx(next, 0.0));
    }

    // never: whole strip fits → center the strip, not the active column.
    {
        const std::vector<double> small{0.4, 0.4}; // total 0.8
        const double centeredStrip = (0.8 - 1.0) / 2.0; // -0.1
        assert(approx(scrollOffsetForFocus(small, 0, -1, 0.0, CenterFocusedColumn::Never), centeredStrip));
        assert(approx(scrollOffsetForFocus(small, 1, 0, 0.0, CenterFocusedColumn::Never), centeredStrip));
    }

    // always: focusing any column recenters it.
    {
        // col 1 width 0.5 at 0.5 → offset = 0.5 - 0.25 = 0.25
        assert(approx(scrollOffsetForFocus(halfs, 1, 0, 0.0, CenterFocusedColumn::Always), 0.25));
        // first column: negative offset so it sits in the middle (empty left peek)
        assert(approx(scrollOffsetForFocus(halfs, 0, -1, 0.0, CenterFocusedColumn::Always), -0.25));
        // last column: offset past total-1 so it sits in the middle
        assert(approx(scrollOffsetForFocus(halfs, 3, 2, 0.0, CenterFocusedColumn::Always), 1.25));
    }

    // always: wide columns left-align (not centered).
    {
        const std::vector<double> wide{1.2, 0.5};
        assert(approx(centerScrollOffset(0.0, 1.2), 0.0));
        assert(approx(scrollOffsetForFocus(wide, 0, -1, 0.0, CenterFocusedColumn::Always), 0.0));
        // second column at 1.2, width 0.5 → 1.2 - 0.25 = 0.95
        assert(approx(scrollOffsetForFocus(wide, 1, 0, 0.0, CenterFocusedColumn::Always), 0.95));
    }

    // on-overflow: two 0.5 columns fit together → fit, not center.
    {
        const std::vector<double> pair{0.5, 0.5};
        // from col 0 to col 1: neighbour widths 0.5+0.5 = 1.0, not overflow
        const double next = scrollOffsetForFocus(pair, 1, 0, 0.0, CenterFocusedColumn::OnOverflow);
        assert(approx(next, 0.0)); // already visible; fit leaves camera
        assert(!neighborOverflows(pair, 1, 0));
    }

    // on-overflow: 0.6+0.6 do not fit → center the target.
    {
        const std::vector<double> fat{0.6, 0.6, 0.6};
        assert(neighborOverflows(fat, 1, 0));
        // col 1 at 0.6, width 0.6 → center offset = 0.6 - 0.2 = 0.4
        assert(approx(scrollOffsetForFocus(fat, 1, 0, 0.0, CenterFocusedColumn::OnOverflow), 0.4));
        // same column / no prev → fit (no center)
        assert(!neighborOverflows(fat, 1, 1));
        assert(!neighborOverflows(fat, 0, -1));
        const double fitSame = scrollOffsetForFocus(fat, 0, -1, 0.0, CenterFocusedColumn::OnOverflow);
        assert(approx(fitSame, fitScrollOffset(0.0, 0.6, 0.0, 1.8)));
    }

    // on-overflow: jump 0→3 uses neighbour 2, not column 0.
    {
        const std::vector<double> mixed{0.9, 0.2, 0.2, 0.9};
        // neighbour of 3 toward 0 is 2; 0.2+0.9 = 1.1 overflow → center
        assert(neighborOverflows(mixed, 3, 0));
        assert(approx(scrollOffsetForFocus(mixed, 3, 0, 0.0, CenterFocusedColumn::OnOverflow),
                      centerScrollOffset(columnLeft(mixed, 3), 0.9)));
        // 0.2+0.2 neighbour from 1→2 would not overflow
        assert(!neighborOverflows(mixed, 2, 1));
    }

    // empty strip
    assert(approx(scrollOffsetForFocus({}, 0, -1, 0.0, CenterFocusedColumn::Always), 0.0));

    std::puts("viewportmath: all checks passed");
    return 0;
}
