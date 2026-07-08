/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for masterstackmath.h — run with:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/masterstackmath_test \
            pkgs/kwin-tiling/tests/masterstackmath_test.cpp && /tmp/masterstackmath_test
*/

#include "../src/tiles/masterstackmath.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace KWin::masterstackmath;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    // Master on left (default): stack windows sit to the right of master.
    {
        const auto rects = layoutRects(3, 1, 0.5, false);
        assert(rects.size() == 3);
        assert(approx(rects[0].x, 0.0) && approx(rects[0].w, 0.5)); // master left
        assert(approx(rects[1].x, 0.5)); // stack right
        // Focus right from master → first stack window
        assert(windowInDirection(rects, 0, Direction::Right) == 1);
        // Focus left from stack → master (index 0)
        assert(windowInDirection(rects, 1, Direction::Left) == 0);
    }

    // Master on right (flipMaster): stack is on the LEFT visually.
    // Left of a stack window must NOT be "always index 0" (master); master is right.
    {
        const auto rects = layoutRects(3, 1, 0.5, true);
        assert(approx(rects[0].x, 0.5) && approx(rects[0].w, 0.5)); // master RIGHT
        assert(approx(rects[1].x, 0.0)); // stack LEFT
        // From stack (1): Left has no neighbour (already left-most column)
        assert(windowInDirection(rects, 1, Direction::Left) == -1);
        // From stack (1): Right goes to master (0)
        assert(windowInDirection(rects, 1, Direction::Right) == 0);
        // From master (0): Left goes to a stack window, not nullptr
        const int leftOfMaster = windowInDirection(rects, 0, Direction::Left);
        assert(leftOfMaster == 1 || leftOfMaster == 2);
        // From master (0): Right has no neighbour
        assert(windowInDirection(rects, 0, Direction::Right) == -1);
    }

    // Multi-master + flip: two masters on the right, stack on the left.
    {
        const auto rects = layoutRects(4, 2, 0.5, true);
        // indices 0,1 masters (right), 2,3 stack (left)
        assert(approx(rects[0].x, 0.5));
        assert(approx(rects[2].x, 0.0));
        // Vertical within master column
        assert(windowInDirection(rects, 0, Direction::Down) == 1);
        assert(windowInDirection(rects, 1, Direction::Up) == 0);
        // Vertical within stack column
        assert(windowInDirection(rects, 2, Direction::Down) == 3);
        // Left from stack is still edge
        assert(windowInDirection(rects, 2, Direction::Left) == -1);
        // Right from stack lands on a master
        const int r = windowInDirection(rects, 2, Direction::Right);
        assert(r == 0 || r == 1);
    }

    // Cross-output entry: enter from left (moving Right) → left-most window
    {
        const auto rects = layoutRects(3, 1, 0.5, true); // master right, stack left
        assert(entryWindowIndex(rects, Direction::Right) == 1
               || entryWindowIndex(rects, Direction::Right) == 2);
        // Enter from right (moving Left) → master on the right
        assert(entryWindowIndex(rects, Direction::Left) == 0);
    }

    // Edge ratios still produce usable geometry for focus.
    {
        const auto narrow = layoutRects(2, 1, 0.1, false);
        assert(approx(narrow[0].w, 0.1) || approx(narrow[0].w, 0.1));
        assert(windowInDirection(narrow, 0, Direction::Right) == 1);
        const auto wide = layoutRects(2, 1, 0.9, true);
        assert(windowInDirection(wide, 0, Direction::Left) == 1);
    }

    // Multi-master column: vertical focus within the master run (unique x).
    {
        const auto rects = layoutRects(4, 2, 0.5, false);
        // masters 0..1 left, stack 2..3 right — Down from master 0 is master 1
        assert(windowInDirection(rects, 0, Direction::Down) == 1);
        assert(windowInDirection(rects, 1, Direction::Up) == 0);
        // Stack vertical
        assert(windowInDirection(rects, 2, Direction::Down) == 3);
        assert(windowInDirection(rects, 3, Direction::Up) == 2);
    }

    std::puts("masterstackmath_test: OK");
    return 0;
}
