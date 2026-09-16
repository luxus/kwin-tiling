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
    assert(parseCenterFocusedColumn("pair-center") == CenterFocusedColumn::PairCenter);
    assert(parseCenterFocusedColumn("pair_center") == CenterFocusedColumn::PairCenter);
    assert(parseCenterFocusedColumn("pairCenter") == CenterFocusedColumn::PairCenter);
    assert(parseCenterFocusedColumn("center-pairs") == CenterFocusedColumn::PairCenter);
    assert(parseCenterFocusedColumn("karousel") == CenterFocusedColumn::PairCenter);
    assert(parseCenterFocusedColumn("") == CenterFocusedColumn::Never);
    assert(parseCenterFocusedColumn("nope") == CenterFocusedColumn::Never);
    assert(std::string(centerFocusedColumnToString(CenterFocusedColumn::Never)) == "never");
    assert(std::string(centerFocusedColumnToString(CenterFocusedColumn::Always)) == "always");
    assert(std::string(centerFocusedColumnToString(CenterFocusedColumn::OnOverflow)) == "on-overflow");
    assert(std::string(centerFocusedColumnToString(CenterFocusedColumn::PairCenter)) == "pair-center");

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
    assert(approx(scrollOffsetForFocus({}, 0, -1, 0.0, CenterFocusedColumn::PairCenter), 0.0));

    // pair-center: n ≤ 2 is fit (Never), not pair-peek.
    {
        const std::vector<double> pair{0.5, 0.5};
        assert(approx(scrollOffsetForFocus(pair, 1, 0, 0.0, CenterFocusedColumn::PairCenter), 0.0));
        const std::vector<double> small{0.4, 0.4}; // total 0.8 → center the strip
        const double centeredStrip = (0.8 - 1.0) / 2.0;
        assert(approx(scrollOffsetForFocus(small, 0, -1, 0.0, CenterFocusedColumn::PairCenter),
                      centeredStrip));
        assert(approx(scrollOffsetForFocus(small, 0, -1, 0.0, CenterFocusedColumn::Never),
                      centeredStrip));
    }

    // pair-center: n > 2 always leaves room for one default-width neighbour,
    // even when the focused column is already fully visible (not only on overflow).
    {
        const std::vector<double> thirds(5, 1.0 / 3.0);
        const double d = 1.0 / 3.0;
        // leftover = 1/3 → minX = 1/6, maxX = 1/2
        assert(approx(pairCenterScrollOffset(0.0, d, 0.0, d), -1.0 / 6.0));
        assert(approx(scrollOffsetForFocus(thirds, 0, -1, 0.0, CenterFocusedColumn::PairCenter, d),
                      -1.0 / 6.0));
        // col 1 at 1/3 is already in [1/6, 1/2] at offset 0 → do not move
        assert(approx(scrollOffsetForFocus(thirds, 1, 0, 0.0, CenterFocusedColumn::PairCenter, d),
                      0.0));
        // col 2 at 2/3 > maxX 1/2 → offset = 2/3 - 1/2 = 1/6
        assert(approx(scrollOffsetForFocus(thirds, 2, 1, 0.0, CenterFocusedColumn::PairCenter, d),
                      1.0 / 6.0));
        // on-overflow would leave offset 0 (1/3+1/3 fit; col 2 already visible)
        assert(!neighborOverflows(thirds, 2, 1));
        assert(approx(scrollOffsetForFocus(thirds, 2, 1, 0.0, CenterFocusedColumn::OnOverflow), 0.0));
        // never would also leave col 2 (fully visible at [2/3, 1])
        assert(approx(scrollOffsetForFocus(thirds, 2, 1, 0.0, CenterFocusedColumn::Never), 0.0));
        // always centers col 2: 2/3 - (1 - 1/3)/2 = 2/3 - 1/3 = 1/3
        assert(approx(scrollOffsetForFocus(thirds, 2, 1, 0.0, CenterFocusedColumn::Always), 1.0 / 3.0));
    }

    // pair-center: four 0.5 columns, default 0.5 → leftover 0, minX=0, maxX=0.5
    {
        // col 2 at 1.0, viewLeft 1.0 > 0.5 → offset = 1.0 - 0.5 = 0.5 (pair-peek left)
        assert(approx(scrollOffsetForFocus(halfs, 2, 1, 0.0, CenterFocusedColumn::PairCenter, 0.5),
                      0.5));
        // col 1 already at maxX → stay
        assert(approx(scrollOffsetForFocus(halfs, 1, 0, 0.0, CenterFocusedColumn::PairCenter, 0.5),
                      0.0));
        // already in the dead zone after scrolling: keep camera
        assert(approx(scrollOffsetForFocus(halfs, 2, 1, 0.5, CenterFocusedColumn::PairCenter, 0.5),
                      0.5));
    }

    // pair-center: pair does not fit (minX < 0) → fit; wide columns left-align.
    {
        const std::vector<double> wide{1.2, 0.5, 0.5};
        assert(approx(pairCenterScrollOffset(0.0, 1.2, 0.0, 0.5), 0.0));
        assert(approx(scrollOffsetForFocus(wide, 0, -1, 0.0, CenterFocusedColumn::PairCenter, 0.5),
                      0.0));
        // 0.6+0.6 default: leftover negative → fit dead-zone, not center
        const std::vector<double> fat{0.6, 0.6, 0.6};
        assert(approx(scrollOffsetForFocus(fat, 0, -1, 0.0, CenterFocusedColumn::PairCenter, 0.6),
                      0.0));
        // col 1: viewLeft 0.6 > maxX 0.4 → offset 0.2 (fit), not center 0.4
        assert(approx(scrollOffsetForFocus(fat, 1, 0, 0.0, CenterFocusedColumn::PairCenter, 0.6),
                      0.2));
        assert(approx(scrollOffsetForFocus(fat, 1, 0, 0.0, CenterFocusedColumn::OnOverflow), 0.4));
    }

    // pair-center: defaultWidth is the reserved neighbour, not the actual one.
    {
        const std::vector<double> mixed{0.9, 0.2, 0.2, 0.9};
        const double d = 0.5;
        // n=4, col 1 width 0.2 at 0.9; leftover = 1-0.2-0.5 = 0.3; minX=0.15; maxX=0.65
        // viewLeft at offset 0 is 0.9 > 0.65 → offset = 0.9 - 0.65 = 0.25
        assert(approx(pairCenterScrollOffset(0.9, 0.2, 0.0, d), 0.25));
        assert(approx(scrollOffsetForFocus(mixed, 1, 0, 0.0, CenterFocusedColumn::PairCenter, d),
                      0.25));
    }

    // placeColumns is a pure translation: x_i = stripX_i - scrollOffset.
    {
        const std::vector<double> widths{0.5, 0.5, 0.5};
        const auto placed = placeColumns(widths, 0.25);
        assert(placed.size() == 3);
        assert(approx(placed[0].x, -0.25) && approx(placed[0].width, 0.5));
        assert(approx(placed[1].x, 0.25) && approx(placed[1].width, 0.5));
        assert(approx(placed[2].x, 0.75) && approx(placed[2].width, 0.5));
        assert(visibility(placed[0]) == Visibility::Peeking);
        assert(visibility(placed[1]) == Visibility::FullyVisible);
        assert(visibility(placed[2]) == Visibility::Peeking);
        assert(!hideForOffscreen(placed[0]));
        assert(!hideForOffscreen(placed[1]));
        assert(!hideForOffscreen(placed[2]));
    }

    // Acceptance (#41): default width 1/3, 5-column strip. Focusing along the
    // strip never changes an unfocused column's width. Peeking neighbours keep
    // full size (not the visible sliver). Fully off-viewport stay hidden.
    {
        const std::vector<double> widths(5, 1.0 / 3.0);
        const std::vector<double> original = widths;
        double offset = 0.0;

        for (int active = 0; active < 5; ++active) {
            offset = scrollOffsetForFocus(widths, active, active - 1, offset, CenterFocusedColumn::Never);
            const auto placed = placeColumns(widths, offset);
            assert(placed.size() == 5);
            assert(visibility(placed[static_cast<size_t>(active)]) == Visibility::FullyVisible);
            assert(!hideForOffscreen(placed[static_cast<size_t>(active)]));

            for (int c = 0; c < 5; ++c) {
                const ColumnRect &r = placed[static_cast<size_t>(c)];
                assert(approx(r.width, original[static_cast<size_t>(c)]));
                assert(approx(r.width, 1.0 / 3.0));

                const Visibility v = visibility(r);
                assert(hideForOffscreen(r) == (v == Visibility::Offscreen));
                if (v == Visibility::Peeking) {
                    const double sliver = clippedViewportWidth(r);
                    assert(sliver > 0.0);
                    assert(sliver < r.width - 1e-9);
                    assert(!hideForOffscreen(r));
                }
                if (v == Visibility::Offscreen) {
                    assert(approx(clippedViewportWidth(r), 0.0));
                    assert(hideForOffscreen(r));
                }
            }
        }
        for (int c = 0; c < 5; ++c) {
            assert(approx(widths[static_cast<size_t>(c)], original[static_cast<size_t>(c)]));
        }
    }

    // Coexistence with Path A overflow: a 1/3 column hanging ~40% past the
    // left edge keeps stored width (not the sliver) and is *not* hidden.
    // Fully off-viewport columns on the same strip *are* hidden. Overflow
    // clamp/pin lives in overflowmath — this header does not copy it.
    {
        const std::vector<double> widths(5, 1.0 / 3.0);
        const double offset = 0.4 / 3.0;
        const auto placed = placeColumns(widths, offset);
        assert(placed.size() == 5);

        assert(visibility(placed[0]) == Visibility::Peeking);
        assert(approx(placed[0].x, -0.4 / 3.0));
        assert(approx(placed[0].width, 1.0 / 3.0));
        assert(!hideForOffscreen(placed[0]));
        const double sliver = clippedViewportWidth(placed[0]);
        assert(sliver > 0.0 && sliver < placed[0].width - 1e-9);

        assert(visibility(placed[4]) == Visibility::Offscreen);
        assert(approx(placed[4].width, 1.0 / 3.0));
        assert(hideForOffscreen(placed[4]));
        assert(approx(clippedViewportWidth(placed[4]), 0.0));

        bool sawPeek = false;
        bool sawHide = false;
        for (const ColumnRect &r : placed) {
            assert(approx(r.width, 1.0 / 3.0));
            if (visibility(r) == Visibility::Peeking) {
                sawPeek = true;
                assert(!hideForOffscreen(r));
            }
            if (hideForOffscreen(r)) {
                sawHide = true;
                assert(visibility(r) == Visibility::Offscreen);
            }
        }
        assert(sawPeek);
        assert(sawHide);
    }

    // Empty placeColumns.
    assert(placeColumns({}, 0.0).empty());

    // pair-center placement: peeking neighbour keeps stored width (not the
    // sliver) and is not hidden. Fully off-viewport stay hidden. Path A
    // overflow/pin is not copied here.
    {
        const std::vector<double> widths(5, 1.0 / 3.0);
        const double d = 1.0 / 3.0;
        const double offset = scrollOffsetForFocus(widths, 2, 1, 0.0, CenterFocusedColumn::PairCenter, d);
        assert(approx(offset, 1.0 / 6.0));
        const auto placed = placeColumns(widths, offset);
        assert(placed.size() == 5);
        assert(visibility(placed[2]) == Visibility::FullyVisible);
        assert(visibility(placed[1]) == Visibility::FullyVisible);
        assert(visibility(placed[0]) == Visibility::Peeking);
        assert(approx(placed[0].width, 1.0 / 3.0));
        assert(!hideForOffscreen(placed[0]));
        const double sliver = clippedViewportWidth(placed[0]);
        assert(sliver > 0.0 && sliver < placed[0].width - 1e-9);
        assert(visibility(placed[4]) == Visibility::Offscreen);
        assert(approx(placed[4].width, 1.0 / 3.0));
        assert(hideForOffscreen(placed[4]));
        for (const ColumnRect &r : placed) {
            assert(approx(r.width, 1.0 / 3.0));
        }
    }

    std::puts("viewportmath: all checks passed");
    return 0;
}
