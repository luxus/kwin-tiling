/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for overflowmath.h — run with:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/overflowmath_test \
            pkgs/kwin-tiling/tests/overflowmath_test.cpp && /tmp/overflowmath_test
*/

#include "../src/tiles/overflowmath.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace KWin::overflowmath;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    // Stock path still clamps a peek to the visible sliver.
    {
        const Rect peek{-0.133, 0.0, 1.0 / 3.0, 1.0};
        const Rect clamped = customTileGeometry(peek, false);
        assert(approx(clamped.x, 0.0));
        assert(clamped.w < peek.w - 1e-9);
    }

    // Path A: CustomTile keeps requested overflow geometry (no [0,1] clamp).
    {
        const Rect peek{0.8, 0.0, 1.0 / 3.0, 1.0}; // ~40% of a 1/3 column past the right edge
        assert(approx(peek.right(), 0.8 + 1.0 / 3.0));
        const Rect kept = customTileGeometry(peek, true);
        assert(approx(kept.x, peek.x) && approx(kept.w, peek.w));
        assert(kept.right() > 1.0);
    }

    // Acceptance (#40): 1/3-width column, ~40% past a 1920px output edge,
    // keeps full width and does not migrate.
    {
        constexpr double outputW = 1920.0;
        constexpr double outputH = 1080.0;
        const Rect output{0.0, 0.0, outputW, outputH};
        const Rect neighbour{outputW, 0.0, outputW, outputH};

        const double colW = outputW / 3.0;          // 640
        const double past = 0.4 * colW;             // 256 px (~40% of the column)
        const Rect absolute{outputW - (colW - past), 0.0, colW, outputH}; // x = 1536

        assert(approx(absolute.w, colW));
        assert(absolute.right() > output.right());
        assert(approx(absolute.right() - output.right(), past));

        const Rect clamped = windowGeometry(absolute, output, false);
        assert(approx(clamped.w, outputW - absolute.x)); // sliver 384
        assert(clamped.w < colW - 1e-9);

        const Rect overflowed = windowGeometry(absolute, output, true);
        assert(approx(overflowed.w, colW));
        assert(approx(overflowed.x, absolute.x));

        const double centerX = overflowed.x + overflowed.w / 2.0;
        const int home = 0;
        const int neighbourId = 1;
        const int centerOutput = (centerX >= outputW) ? neighbourId : home;
        assert(centerOutput == home); // 40% of 1/3 still has its centre on home
        assert(pinnedOutputId(home, centerOutput, true) == home);
        assert(pinnedOutputId(home, neighbourId, true) == home); // even if centre crossed

        assert(isOnOutput(true, true, true));
        assert(!isOnOutput(true, false, true)); // geometry hits neighbour, still not "on" it
        assert(isOnOutput(false, false, true)); // stock: intersect wins

        assert(paintOnView(true, true));
        assert(!paintOnView(true, false));
        assert(paintOnView(false, false)); // stock paints the overlapping sliver

        const bool pointOnNeighbour = neighbour.x + 10.0 >= overflowed.x
            && neighbour.x + 10.0 < overflowed.right();
        assert(pointOnNeighbour);
        assert(!hitOnPinnedOutput(true, false));
        assert(hitOnPinnedOutput(true, true));
        assert(hitOnPinnedOutput(false, false));
    }

    // Fully off-output column (W0-2 / #41) still keeps width; pin prevents migrate.
    {
        const Rect output{0.0, 0.0, 1920.0, 1080.0};
        const Rect absolute{1920.0 + 100.0, 0.0, 640.0, 1080.0};
        const Rect overflowed = windowGeometry(absolute, output, true);
        assert(approx(overflowed.w, 640.0));
        assert(pinnedOutputId(0, 1, true) == 0);
        assert(!paintOnView(true, false));
    }

    // Floating parent intersect would also clamp; overflow skips it.
    {
        const Rect parent{0.0, 0.0, 1.0, 1.0};
        const Rect leaf{-0.2, 0.0, 0.5, 1.0};
        const Rect stock = intersect(leaf, parent);
        assert(approx(stock.x, 0.0) && approx(stock.w, 0.3));
        const Rect overflowed = customTileGeometry(leaf, true);
        assert(approx(overflowed.x, -0.2) && approx(overflowed.w, 0.5));
    }

    std::puts("overflowmath_test: OK");
    return 0;
}
