/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for the pure column arithmetic. Not part of the kwin
    build (lives outside src/) — run it directly during development, it needs no
    KWin/Qt:

        g++ -std=c++20 -O2 -Wall -o /tmp/columnmath_test \
            pkgs/kwin-tiling/tests/columnmath_test.cpp && /tmp/columnmath_test
*/

#include "../src/tiles/columnmath.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace KWin::columnmath;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    // Equal weights -> equal, contiguous slices summing to 1.
    {
        const auto d = distribute({1.0, 1.0, 1.0, 1.0});
        assert(d.size() == 4);
        double acc = 0.0;
        for (const auto &[off, size] : d) {
            assert(approx(off, acc));
            assert(approx(size, 0.25));
            acc += size;
        }
        assert(approx(acc, 1.0));
    }

    // Proportional split: weights 1:3 -> 0.25 / 0.75, offsets 0 / 0.25.
    {
        const auto d = distribute({1.0, 3.0});
        assert(approx(d[0].first, 0.0) && approx(d[0].second, 0.25));
        assert(approx(d[1].first, 0.25) && approx(d[1].second, 0.75));
    }

    // Single leaf fills the whole axis (no special-casing needed by callers).
    {
        const auto d = distribute({1.0});
        assert(d.size() == 1 && approx(d[0].first, 0.0) && approx(d[0].second, 1.0));
    }

    // Degenerate totals fall back to an equal split instead of dividing by zero.
    {
        const auto d = distribute({0.0, 0.0});
        assert(approx(d[0].second, 0.5) && approx(d[1].second, 0.5));
        assert(distribute({}).empty());
    }

    // Resize inverse round-trips: pick a weight, render it to a fraction, invert,
    // and the recovered weight reproduces the same fraction.
    {
        const double w = 2.0;       // dragged leaf
        const double others = 3.0;  // sum of the rest (e.g. three 1.0 siblings)
        const double frac = w / (w + others); // = 0.4, what distribute() would show
        const double recovered = weightForFraction(frac, others);
        assert(approx(recovered, w, 1e-6));
        // and rendering the recovered weight gives the same fraction back
        assert(approx(recovered / (recovered + others), frac, 1e-6));
    }

    // Extremes clamp to sentinels that clampWeight() then pins to the bounds.
    {
        assert(clampWeight(weightForFraction(0.99, 4.0)) == kMaxWeight); // ->10.0 -> 5.0
        assert(clampWeight(weightForFraction(0.0, 4.0)) == kMinWeight);  // ->0.1 -> 0.2
        assert(weightForFraction(0.5, 0.0) == 0.1); // no siblings -> min sentinel
    }

    // clampWeight bounds.
    assert(clampWeight(99.0) == kMaxWeight);
    assert(clampWeight(0.0) == kMinWeight);
    assert(clampWeight(1.0) == 1.0);

    std::puts("columnmath: all checks passed");
    return 0;
}
