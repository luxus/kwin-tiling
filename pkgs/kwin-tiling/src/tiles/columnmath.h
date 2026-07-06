/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <utility>
#include <vector>

// Pure layout arithmetic shared by every tiling engine's vertical stack.
//
// Deliberately free of any KWin / Qt types so it can be unit-tested standalone
// (see tests/columnmath_test.cpp) — the geometry distribution and the
// interactive-resize inverse are exactly the kind of off-by-one / divide-by-zero
// arithmetic that used to be copy-pasted into all three engines.

namespace KWin::columnmath
{

// Minimum / maximum a per-window height weight may take. A weight of 1.0 is an
// equal share; the bounds stop a window from collapsing to nothing or starving
// every sibling.
inline constexpr double kMinWeight = 0.2;
inline constexpr double kMaxWeight = 5.0;

inline double clampWeight(double w)
{
    return std::clamp(w, kMinWeight, kMaxWeight);
}

// Distribute `weights` along a 0..1 axis. Returns one (offset, size) pair per
// weight: offsets are cumulative from 0, sizes are the weight's fraction of the
// total and sum to 1.0. Zero/negative total falls back to an equal split.
inline std::vector<std::pair<double, double>> distribute(const std::vector<double> &weights)
{
    std::vector<std::pair<double, double>> out;
    out.reserve(weights.size());

    double total = 0.0;
    for (const double w : weights) {
        total += w;
    }
    // All weights non-positive (never happens with real, clamped weights, but
    // keep the primitive robust): fall back to an equal split rather than
    // collapsing every leaf to zero height.
    const bool equal = total <= 0.0;
    const double equalSize = weights.empty() ? 0.0 : 1.0 / static_cast<double>(weights.size());

    double offset = 0.0;
    for (const double w : weights) {
        const double size = equal ? equalSize : w / total;
        out.emplace_back(offset, size);
        offset += size;
    }
    return out;
}

// Inverse of distribute() for one leaf: the user interactively resized a leaf to
// occupy `frac` of its column; given the summed weights of the OTHER leaves,
// return the weight that lands it at `frac`. Mirrors the formula that used to
// live (identically) in every engine's endResizeWindow(). The 0.1 / 10.0
// sentinels are intentionally outside [kMinWeight, kMaxWeight] so the caller's
// clampWeight() pins them to the min/max.
inline double weightForFraction(double frac, double otherWeightSum)
{
    frac = std::clamp(frac, 0.05, 0.95);
    if (otherWeightSum > 0.0 && frac > 0.05 && frac < 0.95) {
        return (frac * otherWeightSum) / (1.0 - frac);
    }
    return (frac >= 0.95) ? 10.0 : 0.1;
}

} // namespace KWin::columnmath
