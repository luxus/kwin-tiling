/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for tilingconfig.h (enabled-kinds parse, layout
    precedence, smart-gap suppression).
*/

#include "../src/tiling/tilingconfig.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace KWin::tilingconfig;

int main()
{
    // Case-insensitive parse; unknown names skipped.
    assert(parseLayoutKind("MasterStack") == LayoutKind::MasterStack);
    assert(parseLayoutKind("masterstack") == LayoutKind::MasterStack);
    assert(parseLayoutKind("STACKED") == LayoutKind::Stacked);
    assert(parseLayoutKind("Scrolling") == LayoutKind::Scrolling);
    assert(parseLayoutKind("Centered") == LayoutKind::Centered);
    assert(parseLayoutKind("Grid") == LayoutKind::Grid);
    assert(!parseLayoutKind(""));
    assert(!parseLayoutKind("NotALayout"));

    // Enabled list: order preserved, unknown dropped, empty → fallback.
    const std::vector<LayoutKind> enabled = parseEnabledKinds(
        {"Stacked", "bogus", "Scrolling", "MasterStack"}, LayoutKind::Centered);
    assert(enabled.size() == 3);
    assert(enabled[0] == LayoutKind::Stacked);
    assert(enabled[1] == LayoutKind::Scrolling);
    assert(enabled[2] == LayoutKind::MasterStack);

    const std::vector<LayoutKind> emptyFallback = parseEnabledKinds({}, LayoutKind::Centered);
    assert(emptyFallback.size() == 1);
    assert(emptyFallback[0] == LayoutKind::Centered);
    const std::vector<LayoutKind> unknownFallback = parseEnabledKinds({"nope"}, LayoutKind::Grid);
    assert(unknownFallback.size() == 1);
    assert(unknownFallback[0] == LayoutKind::Grid);

    assert(isLayoutEnabled(LayoutKind::Stacked, enabled));
    assert(!isLayoutEnabled(LayoutKind::Grid, enabled));

    // Config default: DesktopOutput wins when it differs from global.
    const std::vector<LayoutKind> all = parseEnabledKinds(
        {"MasterStack", "Stacked", "Scrolling", "Centered", "Grid"}, LayoutKind::MasterStack);
    assert(resolveConfigDefault(LayoutKind::MasterStack, all, LayoutKind::Stacked, LayoutKind::Scrolling)
           == LayoutKind::Stacked);
    // DesktopOutput equal to global still lets the per-output override apply
    // (matches KConfig hasKey + "kind == globalDefault" in the controller).
    assert(resolveConfigDefault(LayoutKind::MasterStack, all, LayoutKind::MasterStack, LayoutKind::Scrolling)
           == LayoutKind::Scrolling);
    // No DesktopOutput: per-output override applies.
    assert(resolveConfigDefault(LayoutKind::MasterStack, all, std::nullopt, LayoutKind::Scrolling)
           == LayoutKind::Scrolling);
    // Neither override: global default.
    assert(resolveConfigDefault(LayoutKind::MasterStack, all, std::nullopt, std::nullopt)
           == LayoutKind::MasterStack);
    // Disabled chosen kind → first enabled.
    const std::vector<LayoutKind> noGrid = parseEnabledKinds({"Stacked", "Scrolling"}, LayoutKind::Stacked);
    assert(resolveConfigDefault(LayoutKind::Grid, noGrid, std::nullopt, std::nullopt) == LayoutKind::Stacked);
    assert(resolveConfigDefault(LayoutKind::MasterStack, noGrid, LayoutKind::Grid, std::nullopt)
           == LayoutKind::Stacked);

    // Remembered Cycle/Switch wins while still enabled.
    LayoutKindInputs in;
    in.globalDefault = LayoutKind::MasterStack;
    in.enabled = all;
    in.remembered = LayoutKind::Grid;
    in.desktopOutput = LayoutKind::Stacked;
    in.outputDefault = LayoutKind::Scrolling;
    assert(layoutKindFor(in) == LayoutKind::Grid);

    // Remembered but disabled → fall through to DesktopOutput.
    in.remembered = LayoutKind::Grid;
    in.enabled = noGrid;
    in.desktopOutput = LayoutKind::Scrolling;
    assert(layoutKindFor(in) == LayoutKind::Scrolling);

    // No remembered: same as resolveConfigDefault.
    in.remembered = std::nullopt;
    in.enabled = all;
    in.desktopOutput = LayoutKind::Centered;
    assert(layoutKindFor(in) == LayoutKind::Centered);

    // Gap merge: missing override keys keep defaults.
    const GapSettings defaults{8.0, 4, 4, 2, 2};
    GapOverride partial;
    partial.gapLeft = 10;
    const GapSettings merged = mergeGaps(defaults, partial);
    assert(merged.gapBetween == 8.0);
    assert(merged.gapLeft == 10);
    assert(merged.gapRight == 4);
    assert(merged.gapTop == 2);
    assert(merged.gapBottom == 2);

    GapOverride full;
    full.gapBetween = 12.0;
    full.gapLeft = 1;
    full.gapRight = 2;
    full.gapTop = 3;
    full.gapBottom = 4;
    const GapSettings allOver = mergeGaps(defaults, full);
    assert(allOver.gapBetween == 12.0);
    assert(allOver.gapLeft == 1);
    assert(allOver.gapRight == 2);
    assert(allOver.gapTop == 3);
    assert(allOver.gapBottom == 4);

    // Smart gaps: user toggle or ≤1 window.
    assert(shouldSuppressGaps(true, 5));
    assert(shouldSuppressGaps(false, 0));
    assert(shouldSuppressGaps(false, 1));
    assert(!shouldSuppressGaps(false, 2));

    std::puts("tilingconfig_test: OK");
    return 0;
}
