/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Combined self-check for scrollingmath.h. Covers every API shared by
    Scrolling PRs so a rebase cannot drop a sibling's tests:

      #51 / W0-2  placeColumns + fitScrollOffset (peek at full width)
      #55 / W2-1  expandToAvailableWidth
      #45 / W1-4  drop-insert (consume index + strip index)

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/scrollingmath_test \
            pkgs/kwin-tiling/tests/scrollingmath_test.cpp && /tmp/scrollingmath_test
*/

#include "../src/tiles/scrollingmath.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace KWin::scrollingmath;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    // ========== #51 / W0-2: placeColumns + fitScrollOffset ==========

    // Empty / fit-the-viewport: centre the strip (offset may be negative).
    {
        assert(approx(fitScrollOffset({}, 0, 0.0), 0.0));
        const std::vector<double> two{0.4, 0.4};
        assert(approx(fitScrollOffset(two, 0, 0.0), (0.8 - 1.0) / 2.0));
        const auto placed = placeColumns(two, fitScrollOffset(two, 0, 0.0));
        assert(placed.size() == 2);
        assert(approx(placed[0].width, 0.4) && approx(placed[1].width, 0.4));
    }

    // Fit-scroll does not move when the active column is already fully visible.
    {
        const std::vector<double> widths(5, 1.0 / 3.0);
        const double offset = fitScrollOffset(widths, 0, 0.0);
        assert(approx(offset, 0.0));
        assert(approx(fitScrollOffset(widths, 1, offset), offset));
        assert(approx(fitScrollOffset(widths, 2, offset), offset));
    }

    // Acceptance (#41): default width 1/3, 5-column strip. Focusing along the
    // strip never changes an unfocused column's width; neighbours that overlap
    // the viewport peek at full size (not the visible sliver).
    {
        const std::vector<double> widths(5, 1.0 / 3.0);
        const std::vector<double> original = widths;
        double offset = 0.0;
        bool sawPeek = false;

        for (int active = 0; active < 5; ++active) {
            offset = fitScrollOffset(widths, active, offset);
            const auto placed = placeColumns(widths, offset);
            assert(placed.size() == 5);

            assert(visibility(placed[static_cast<size_t>(active)]) == Visibility::FullyVisible);

            for (int c = 0; c < 5; ++c) {
                const ColumnRect &r = placed[static_cast<size_t>(c)];
                assert(approx(r.width, original[static_cast<size_t>(c)]));
                assert(approx(r.width, 1.0 / 3.0));

                const Visibility v = visibility(r);
                if (v == Visibility::Peeking) {
                    sawPeek = true;
                    const double sliver = clippedViewportWidth(r);
                    assert(sliver > 0.0);
                    assert(sliver < r.width - 1e-9);
                    // Placed width stays the stored column width, not the sliver.
                    assert(approx(r.width, 1.0 / 3.0));
                }
            }
        }
        assert(sawPeek);
        // Original widths were never mutated by placement / fit-scroll.
        for (int c = 0; c < 5; ++c) {
            assert(approx(widths[static_cast<size_t>(c)], original[static_cast<size_t>(c)]));
        }
    }

    // Explicit peek: a 1/3 column hanging 40% off the left edge keeps width 1/3.
    {
        const ColumnRect peek{-0.4 * (1.0 / 3.0), 1.0 / 3.0};
        assert(visibility(peek) == Visibility::Peeking);
        assert(approx(peek.width, 1.0 / 3.0));
        assert(clippedViewportWidth(peek) < peek.width);
        const auto placed = placeColumns({1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0}, 0.4 / 3.0);
        assert(approx(placed[0].x, -0.4 / 3.0));
        assert(approx(placed[0].width, 1.0 / 3.0));
        assert(visibility(placed[0]) == Visibility::Peeking);
    }

    // Fully off-viewport columns are still placed at full width (not hidden,
    // not collapsed). Hide-for-offscreen is gone; monocle hide is not math.
    {
        const std::vector<double> widths(5, 1.0 / 3.0);
        const double offset = fitScrollOffset(widths, 0, 0.0);
        const auto placed = placeColumns(widths, offset);
        assert(visibility(placed[4]) == Visibility::Offscreen);
        assert(approx(placed[4].width, 1.0 / 3.0));
        assert(approx(clippedViewportWidth(placed[4]), 0.0));
    }

    // placeColumns is a pure translation: x_i = stripX_i - scrollOffset.
    {
        const std::vector<double> widths{0.5, 0.5, 0.5};
        const auto placed = placeColumns(widths, 0.25);
        assert(approx(placed[0].x, -0.25) && approx(placed[0].width, 0.5));
        assert(approx(placed[1].x, 0.25) && approx(placed[1].width, 0.5));
        assert(approx(placed[2].x, 0.75) && approx(placed[2].width, 0.5));
        assert(visibility(placed[0]) == Visibility::Peeking);
        assert(visibility(placed[1]) == Visibility::FullyVisible);
        assert(visibility(placed[2]) == Visibility::Peeking);
    }

    // ========== #55 / W2-1: expandToAvailableWidth ==========

    const double third = 1.0 / 3.0;

    // Issue #47 / W2-1: two 1/3 columns with empty space. Focused grows to the
    // visible remainder (2/3); neighbour stays 1/3. Default 0.5 then 1/3 presets.
    {
        std::vector<double> atDefault = {0.5, 0.5};
        const auto noRoom = expandToAvailableWidth(atDefault, 0, 0.0);
        assert(noRoom.action == ExpandAction::None);

        std::vector<double> thirds = {third, third};
        const double scroll = centeredScrollOffset(third + third);
        const auto focusedLeft = expandToAvailableWidth(thirds, 0, scroll);
        assert(focusedLeft.action == ExpandAction::Grow);
        assert(approx(focusedLeft.newWidth, 2.0 / 3.0));
        applyGrow(thirds, 0, focusedLeft);
        assert(approx(thirds[0], 2.0 / 3.0));
        assert(approx(thirds[1], third)); // neighbour unchanged
    }
    {
        std::vector<double> thirds = {third, third};
        const double scroll = centeredScrollOffset(third + third);
        const auto focusedRight = expandToAvailableWidth(thirds, 1, scroll);
        assert(focusedRight.action == ExpandAction::Grow);
        assert(approx(focusedRight.newWidth, 2.0 / 3.0));
        applyGrow(thirds, 1, focusedRight);
        assert(approx(thirds[0], third)); // neighbour unchanged
        assert(approx(thirds[1], 2.0 / 3.0));
    }

    // Alone (single column, or the only fully visible one) → toggle full width.
    {
        const std::vector<double> alone = {0.5};
        const auto plan = expandToAvailableWidth(alone, 0, centeredScrollOffset(0.5));
        assert(plan.action == ExpandAction::ToggleFullWidth);
    }
    {
        const std::vector<double> alreadyFull = {1.0};
        const auto plan = expandToAvailableWidth(alreadyFull, 0, 0.0);
        assert(plan.action == ExpandAction::ToggleFullWidth);
    }

    // Two half-width columns already fill the view: nowhere to expand.
    {
        const std::vector<double> filled = {0.5, 0.5};
        const auto plan = expandToAvailableWidth(filled, 1, 0.0);
        assert(plan.action == ExpandAction::None);
    }

    // Peeking third column: leftover (including the peek) goes to the focused
    // column; the other fully visible neighbour is unchanged.
    {
        std::vector<double> cols = {0.4, 0.4, 0.4};
        const auto plan = expandToAvailableWidth(cols, 0, 0.0);
        assert(plan.action == ExpandAction::Grow);
        assert(approx(plan.newWidth, 0.6));
        applyGrow(cols, 0, plan);
        assert(approx(cols[0], 0.6));
        assert(approx(cols[1], 0.4));
        assert(approx(cols[2], 0.4));
    }

    // Active column not fully visible → no-op.
    {
        const std::vector<double> cols = {0.4, 0.4, 0.4};
        const auto plan = expandToAvailableWidth(cols, 2, 0.0);
        assert(plan.action == ExpandAction::None);
    }

    // Scroll so the right pair is fully visible; grow the rightmost. Leftmost
    // fully-visible neighbour keeps its width; view pins to that neighbour.
    {
        std::vector<double> cols = {0.4, 0.4, 0.4};
        const auto plan = expandToAvailableWidth(cols, 2, 0.2);
        assert(plan.action == ExpandAction::Grow);
        assert(approx(plan.newWidth, 0.6));
        assert(approx(plan.scrollOffset, 0.4));
        applyGrow(cols, 2, plan);
        assert(approx(cols[0], 0.4));
        assert(approx(cols[1], 0.4));
        assert(approx(cols[2], 0.6));
    }

    // Empty / out-of-range active → no-op.
    {
        assert(expandToAvailableWidth({}, 0, 0.0).action == ExpandAction::None);
        const std::vector<double> one = {0.5};
        assert(expandToAvailableWidth(one, -1, 0.0).action == ExpandAction::None);
        assert(expandToAvailableWidth(one, 3, 0.0).action == ExpandAction::None);
    }

    // ========== #45 / W1-4: drop-insert ==========

    // --- in-column Y half: consume index ---
    // Upper half of the first (only) leaf → insert at 0 (above).
    assert(!isLowerHalf(10.0, 0.0, 100.0));
    assert(consumeInsertIndex(0, false) == 0);

    // Lower half of a single-window column → insert at 1 → 2-high column.
    assert(isLowerHalf(50.0, 0.0, 100.0));
    assert(isLowerHalf(75.0, 0.0, 100.0));
    assert(consumeInsertIndex(0, true) == 1);

    // Midpoint counts as lower (append after the target).
    assert(isLowerHalf(50.0, 0.0, 100.0));

    // Third leaf, upper vs lower.
    assert(consumeInsertIndex(2, false) == 2);
    assert(consumeInsertIndex(2, true) == 3);

    // Degenerate geometry: treat as below (append).
    assert(isLowerHalf(0.0, 0.0, 0.0));
    assert(consumeInsertIndex(-1, true) == 0);

    // --- empty-space strip index from cursor X ---
    // Two 0.5 columns, no scroll: midpoints at 0.25 and 0.75.
    const std::vector<double> two = {0.5, 0.5};
    assert(stripInsertIndex(0.10, two, 0.0) == 0); // left of first
    assert(stripInsertIndex(0.24, two, 0.0) == 0); // left half of first
    // Gap between columns (0.5) and right half of first → new column at 1.
    assert(stripInsertIndex(0.40, two, 0.0) == 1);
    assert(stripInsertIndex(0.50, two, 0.0) == 1);
    assert(stripInsertIndex(0.60, two, 0.0) == 1); // left half of second
    assert(stripInsertIndex(0.90, two, 0.0) == 2); // right of last

    // Empty strip → index 0.
    assert(stripInsertIndex(0.5, {}, 0.0) == 0);

    // Single 0.5 column centred: scrollOffset = (0.5 - 1) / 2 = -0.25.
    // View left = 0.25, midpoint = 0.50.
    const std::vector<double> one = {0.5};
    const double centered = (0.5 - 1.0) / 2.0;
    assert(stripInsertIndex(0.10, one, centered) == 0); // empty left of strip
    assert(stripInsertIndex(0.50, one, centered) == 1); // midpoint and right
    assert(stripInsertIndex(0.90, one, centered) == 1); // empty right of strip

    // Scrolled viewport: offset 0.2, same two 0.5 columns.
    // View mids: (-0.2 + 0.25) = 0.05 and (0.3 + 0.25) = 0.55.
    assert(stripInsertIndex(0.00, two, 0.2) == 0);
    assert(stripInsertIndex(0.40, two, 0.2) == 1);
    assert(stripInsertIndex(0.80, two, 0.2) == 2);

    std::puts("scrollingmath: all checks passed");
    return 0;
}
