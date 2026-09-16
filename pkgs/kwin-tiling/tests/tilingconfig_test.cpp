/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for tilingconfig.h (enabled-kinds parse, layout
    precedence, per-output sizing clamp, per-desktop sizing overlays,
    smart-gap suppression).
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

    // --- per-output sizing: override-if-present, else default, then clamp ---
    const OutputSizing global{0.5, 1, 0.5};
    const OutputSizing resolvedDefault = resolveOutputSizing(global);
    assert(resolvedDefault.masterRatio == 0.5);
    assert(resolvedDefault.masterCount == 1);
    assert(resolvedDefault.defaultColumnWidth == 0.5);

    OutputSizingOverride ratioOnly;
    ratioOnly.masterRatio = 0.7;
    const OutputSizing ratioMerged = resolveOutputSizing(global, ratioOnly);
    assert(ratioMerged.masterRatio == 0.7);
    assert(ratioMerged.masterCount == 1);
    assert(ratioMerged.defaultColumnWidth == 0.5);

    OutputSizingOverride allOverSize;
    allOverSize.masterRatio = 0.25;
    allOverSize.masterCount = 3;
    allOverSize.defaultColumnWidth = 0.8;
    const OutputSizing fullSize = resolveOutputSizing(global, allOverSize);
    assert(fullSize.masterRatio == 0.25);
    assert(fullSize.masterCount == 3);
    assert(fullSize.defaultColumnWidth == 0.8);

    // Out-of-range values clamp on both the global default and the override.
    assert(clampMasterRatio(0.05) == kMinMasterRatio);
    assert(clampMasterRatio(0.99) == kMaxMasterRatio);
    assert(clampMasterRatio(0.5) == 0.5);
    assert(clampMasterCount(0) == kMinMasterCount);
    assert(clampMasterCount(-3) == kMinMasterCount);
    assert(clampMasterCount(4) == 4);
    assert(clampColumnWidth(0.0) == kMinColumnWidth);
    assert(clampColumnWidth(1.5) == kMaxColumnWidth);

    const OutputSizing wildGlobal{99.0, 0, -1.0};
    const OutputSizing clampedGlobal = resolveOutputSizing(wildGlobal);
    assert(clampedGlobal.masterRatio == kMaxMasterRatio);
    assert(clampedGlobal.masterCount == kMinMasterCount);
    assert(clampedGlobal.defaultColumnWidth == kMinColumnWidth);

    OutputSizingOverride wildOver;
    wildOver.masterRatio = 0.01;
    wildOver.masterCount = -8;
    wildOver.defaultColumnWidth = 2.0;
    const OutputSizing clampedOver = resolveOutputSizing(global, wildOver);
    assert(clampedOver.masterRatio == kMinMasterRatio);
    assert(clampedOver.masterCount == kMinMasterCount);
    assert(clampedOver.defaultColumnWidth == kMaxColumnWidth);

    // Per-desktop overlay: missing keys keep the previous layer.
    OutputSizingOverride outputSizing;
    outputSizing.masterRatio = 0.6;
    outputSizing.masterCount = 2;
    OutputSizingOverride desktopSizing;
    desktopSizing.masterCount = 3;
    const OutputSizing resolved = resolveSizing(global, outputSizing, desktopSizing);
    assert(resolved.masterRatio == 0.6); // output, desktop did not set it
    assert(resolved.masterCount == 3); // desktop wins
    assert(resolved.defaultColumnWidth == 0.5);

    // Desktop overlay wins over output for the same key.
    desktopSizing.masterRatio = 0.7;
    const OutputSizing desktopWins = resolveSizing(global, outputSizing, desktopSizing);
    assert(desktopWins.masterRatio == 0.7);
    assert(desktopWins.masterCount == 3);

    // Empty overlays: global unchanged (and still clamped).
    const OutputSizing noOverride = resolveSizing(global, {}, OutputSizingOverride{});
    assert(noOverride.masterRatio == 0.5);
    assert(noOverride.masterCount == 1);
    assert(noOverride.defaultColumnWidth == 0.5);
    assert(sizingOverrideEmpty(OutputSizingOverride{}));
    assert(!sizingOverrideEmpty(outputSizing));

    // Desktop overlay clamps out-of-range values.
    OutputSizingOverride wildDesktop;
    wildDesktop.masterRatio = 0.05;
    wildDesktop.masterCount = 0;
    wildDesktop.defaultColumnWidth = 1.5;
    const OutputSizing clampedDesktop = resolveSizing(global, {}, wildDesktop);
    assert(clampedDesktop.masterRatio == kMinMasterRatio);
    assert(clampedDesktop.masterCount == kMinMasterCount);
    assert(clampedDesktop.defaultColumnWidth == kMaxColumnWidth);
    wildDesktop.masterRatio = 1.5;
    assert(resolveSizing(global, {}, wildDesktop).masterRatio == kMaxMasterRatio);

    // Live writes: (output, desktop) → DesktopOutput so each desktop keeps its own.
    assert(sizingWriteTarget(true, true, true) == SizingWriteTarget::DesktopOutput);
    assert(sizingWriteTarget(true, false, true) == SizingWriteTarget::DesktopOutput);
    assert(sizingWriteTarget(true, true, false) == SizingWriteTarget::Output);
    assert(sizingWriteTarget(true, false, false) == SizingWriteTarget::Global);
    assert(sizingWriteTarget(false, false, true) == SizingWriteTarget::Global);
    assert(sizingWriteTarget(false, false, false) == SizingWriteTarget::Global);

    std::puts("tilingconfig_test: OK");
    return 0;
}
