/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for columnwidthpresets.h. Issue #48 proof:
    presets 0.25, 0.5 and cycle twice. Also reverse cycle and parse forms.

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/columnwidthpresets_test \
            pkgs/kwin-tiling/tests/columnwidthpresets_test.cpp \
            && /tmp/columnwidthpresets_test
*/

#include "../src/tiles/columnwidthpresets.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

using namespace KWin::columnwidthpresets;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    // Historical default set.
    {
        const auto d = defaults();
        assert(d.size() == 4);
        assert(approx(d[0], 1.0 / 3.0));
        assert(approx(d[1], 0.5));
        assert(approx(d[2], 2.0 / 3.0));
        assert(approx(d[3], 1.0));
    }

    // Parse decimals, fractions, percents, and integer percents.
    {
        const auto p = parse({"1/3", "50%", "0.666667", "1"});
        assert(p.size() == 4);
        assert(approx(p[0], 1.0 / 3.0, 1e-6));
        assert(approx(p[1], 0.5));
        assert(approx(p[2], 2.0 / 3.0, 1e-4));
        assert(approx(p[3], 1.0));
        assert(approx(parse({"25"})[0], 0.25));
        assert(approx(parse({"1/4"})[0], 0.25));
    }

    // Comma-separated single token (kwinrc one-liner).
    {
        const auto p = parse({"0.25, 0.5"});
        assert(p.size() == 2);
        assert(approx(p[0], 0.25) && approx(p[1], 0.5));
    }

    // Invalid / empty falls back to defaults; junk in a mixed list is skipped.
    {
        const auto d = defaults();
        const auto empty = parse({});
        assert(empty.size() == d.size() && approx(empty[0], d[0]));
        const auto junk = parse({"nope", "also-bad"});
        assert(junk.size() == d.size());
        const auto mixed = parse({"nope", "0.25", "0.5"});
        assert(mixed.size() == 2);
        assert(approx(mixed[0], 0.25) && approx(mixed[1], 0.5));
    }

    // Duplicates collapse; values clamp to [0.1, 1.0]; output is sorted.
    {
        const auto p = parse({"0.5", "0.25", "0.5", "2.0", "0.01"});
        assert(p.size() == 3);
        assert(approx(p[0], 0.1)); // 0.01 clamped
        assert(approx(p[1], 0.25));
        assert(approx(p[2], 0.5));
    }

    // --- Issue #48 proof: presets 0.25, 0.5 and cycle twice ---
    {
        const auto p = parse({"0.25", "0.5"});
        assert(p.size() == 2);

        // Starting on the narrow preset: 0.25 → 0.5 → 0.25.
        double w = 0.25;
        w = cycle(w, p, +1);
        assert(approx(w, 0.5));
        w = cycle(w, p, +1);
        assert(approx(w, 0.25));

        // Starting on the default column width (0.5): wrap to 0.25, then 0.5.
        w = 0.5;
        w = cycle(w, p, +1);
        assert(approx(w, 0.25));
        w = cycle(w, p, +1);
        assert(approx(w, 0.5));

        // Reverse from 0.5: 0.25, then wrap to 0.5.
        w = 0.5;
        w = cycle(w, p, -1);
        assert(approx(w, 0.25));
        w = cycle(w, p, -1);
        assert(approx(w, 0.5));

        // Only configured values are visited.
        std::set<int> seen;
        w = 0.25;
        for (int i = 0; i < 8; ++i) {
            w = cycle(w, p, +1);
            seen.insert(int(std::lround(w * 100)));
        }
        assert(seen.size() == 2);
        assert(seen.count(25) && seen.count(50));
    }

    // Default list: next-larger wrap, reverse next-smaller wrap.
    {
        const auto p = defaults();
        assert(approx(cycle(0.5, p, +1), 2.0 / 3.0));
        assert(approx(cycle(2.0 / 3.0, p, +1), 1.0));
        assert(approx(cycle(1.0, p, +1), 1.0 / 3.0));
        assert(approx(cycle(1.0 / 3.0, p, +1), 0.5));

        assert(approx(cycle(0.5, p, -1), 1.0 / 3.0));
        assert(approx(cycle(1.0 / 3.0, p, -1), 1.0));
        assert(approx(cycle(1.0, p, -1), 2.0 / 3.0));

        // Off-preset (user-resized) jumps to the next configured value.
        assert(approx(cycle(0.4, p, +1), 0.5));
        assert(approx(cycle(0.4, p, -1), 1.0 / 3.0));
    }

    // Single preset: cycle and reverse stay put.
    {
        const auto p = parse({"0.5"});
        assert(p.size() == 1);
        assert(approx(cycle(0.5, p, +1), 0.5));
        assert(approx(cycle(0.25, p, +1), 0.5));
        assert(approx(cycle(1.0, p, -1), 0.5));
    }

    // Empty preset list uses defaults.
    {
        const std::vector<double> empty;
        assert(approx(cycle(0.5, empty, +1), 2.0 / 3.0));
    }

    std::puts("columnwidthpresets: all checks passed");
    return 0;
}
