/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Pure config-resolution helpers for TilingController.
// Layout-kind parsing, enabled-list fallback, remembered-vs-default
// precedence, per-output sizing clamp, per-desktop sizing overlays, and
// smart-gap suppression — unit-tested without Qt/KWin
// (tests/tilingconfig_test.cpp). TilingController loads KConfig once in
// reconfigure() and consults the cache on the window add/remove/migrate path.

namespace KWin::tilingconfig
{

// Keep in sync with LayoutEngine::LayoutKind.
enum class LayoutKind {
    MasterStack = 0,
    Stacked = 1,
    Scrolling = 2,
    Centered = 3,
    Grid = 4,
    Columns = 5,
};

inline bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

inline std::optional<LayoutKind> parseLayoutKind(std::string_view name)
{
    if (iequals(name, "MasterStack")) {
        return LayoutKind::MasterStack;
    }
    if (iequals(name, "Stacked")) {
        return LayoutKind::Stacked;
    }
    if (iequals(name, "Scrolling")) {
        return LayoutKind::Scrolling;
    }
    if (iequals(name, "Centered")) {
        return LayoutKind::Centered;
    }
    if (iequals(name, "Grid")) {
        return LayoutKind::Grid;
    }
    if (iequals(name, "Columns")) {
        return LayoutKind::Columns;
    }
    return std::nullopt;
}

/**
 * Parse EnabledLayouts. Unknown names are skipped. If nothing known remains,
 * fall back to @p fallback so the controller always has at least one kind.
 * Order is preserved (cycleLayout uses it).
 */
inline std::vector<LayoutKind> parseEnabledKinds(const std::vector<std::string> &names, LayoutKind fallback)
{
    std::vector<LayoutKind> result;
    result.reserve(names.size());
    for (const std::string &name : names) {
        if (const std::optional<LayoutKind> kind = parseLayoutKind(name)) {
            result.push_back(*kind);
        }
    }
    if (result.empty()) {
        result.push_back(fallback);
    }
    return result;
}

inline bool isLayoutEnabled(LayoutKind kind, const std::vector<LayoutKind> &enabled)
{
    for (LayoutKind k : enabled) {
        if (k == kind) {
            return true;
        }
    }
    return false;
}

/**
 * Config default for an (output, desktop) pair, without remembered
 * Cycle/Switch memory.
 *
 * DesktopOutput DefaultLayout wins when present. If the result is still the
 * global default, a per-output DefaultLayout (if any) is applied. If the
 * chosen kind is not enabled, the first enabled kind is used.
 */
inline LayoutKind resolveConfigDefault(LayoutKind globalDefault, const std::vector<LayoutKind> &enabled,
                                         std::optional<LayoutKind> desktopOutput, std::optional<LayoutKind> outputDefault)
{
    LayoutKind kind = globalDefault;
    if (desktopOutput) {
        kind = *desktopOutput;
    }
    if (kind == globalDefault && outputDefault) {
        kind = *outputDefault;
    }
    if (!isLayoutEnabled(kind, enabled) && !enabled.empty()) {
        kind = enabled.front();
    }
    return kind;
}

struct LayoutKindInputs {
    LayoutKind globalDefault = LayoutKind::MasterStack;
    std::vector<LayoutKind> enabled;
    std::optional<LayoutKind> remembered; // [Tiling][DesktopLayouts]
    std::optional<LayoutKind> desktopOutput; // [Tiling][DesktopOutput N:name]
    std::optional<LayoutKind> outputDefault; // [Tiling][Output name]
};

/**
 * Per-(output, desktop) layout: a remembered manual choice wins while it is
 * still enabled; otherwise the config default (see resolveConfigDefault).
 */
inline LayoutKind layoutKindFor(const LayoutKindInputs &in)
{
    if (in.remembered && isLayoutEnabled(*in.remembered, in.enabled)) {
        return *in.remembered;
    }
    return resolveConfigDefault(in.globalDefault, in.enabled, in.desktopOutput, in.outputDefault);
}

struct GapSettings {
    double gapBetween = 0.0;
    int gapLeft = 0;
    int gapRight = 0;
    int gapTop = 0;
    int gapBottom = 0;
};

/**
 * Merge a per-output override onto global defaults. Keys missing from the
 * override keep the default (same as KConfigGroup::readEntry with a default).
 */
struct GapOverride {
    std::optional<double> gapBetween;
    std::optional<int> gapLeft;
    std::optional<int> gapRight;
    std::optional<int> gapTop;
    std::optional<int> gapBottom;
};

inline GapSettings mergeGaps(const GapSettings &defaults, const GapOverride &override)
{
    GapSettings out = defaults;
    if (override.gapBetween) {
        out.gapBetween = *override.gapBetween;
    }
    if (override.gapLeft) {
        out.gapLeft = *override.gapLeft;
    }
    if (override.gapRight) {
        out.gapRight = *override.gapRight;
    }
    if (override.gapTop) {
        out.gapTop = *override.gapTop;
    }
    if (override.gapBottom) {
        out.gapBottom = *override.gapBottom;
    }
    return out;
}

/** Smart gaps: no indent/between for a single or empty layout, or user toggle. */
inline bool shouldSuppressGaps(bool gapsSuppressed, int windowCount)
{
    return gapsSuppressed || windowCount <= 1;
}

// --- per-output sizing (MasterRatio / MasterCount / DefaultColumnWidth) ---
// Same bounds TilingController and the KCM use. Override-if-present, else
// the global default, then clamp either way. (#7 Part A)

inline constexpr double kMinMasterRatio = 0.1;
inline constexpr double kMaxMasterRatio = 0.9;
inline constexpr double kMinColumnWidth = 0.1;
inline constexpr double kMaxColumnWidth = 1.0;
inline constexpr int kMinMasterCount = 1;
inline constexpr int kMinMaxColumns = 2;
inline constexpr int kMaxMaxColumns = 5;

inline double clampMasterRatio(double v)
{
    return std::clamp(v, kMinMasterRatio, kMaxMasterRatio);
}

inline int clampMasterCount(int v)
{
    return std::max(kMinMasterCount, v);
}

inline double clampColumnWidth(double v)
{
    return std::clamp(v, kMinColumnWidth, kMaxColumnWidth);
}

inline int clampMaxColumns(int v)
{
    return std::clamp(v, kMinMaxColumns, kMaxMaxColumns);
}

struct OutputSizing {
    double masterRatio = 0.5;
    int masterCount = 1;
    double defaultColumnWidth = 0.5;
};

struct OutputSizingOverride {
    std::optional<double> masterRatio;
    std::optional<int> masterCount;
    std::optional<double> defaultColumnWidth;
};

inline OutputSizing resolveOutputSizing(const OutputSizing &defaults, const OutputSizingOverride &override = {})
{
    OutputSizing out;
    out.masterRatio = clampMasterRatio(override.masterRatio.value_or(defaults.masterRatio));
    out.masterCount = clampMasterCount(override.masterCount.value_or(defaults.masterCount));
    out.defaultColumnWidth = clampColumnWidth(override.defaultColumnWidth.value_or(defaults.defaultColumnWidth));
    return out;
}

inline bool sizingOverrideEmpty(const OutputSizingOverride &o)
{
    return !o.masterRatio && !o.masterCount && !o.defaultColumnWidth;
}

/**
 * Resolve sizing: global defaults, then per-output, then per-(desktop, output).
 * The most specific present key wins. Built on resolveOutputSizing (#7 Part A).
 */
inline OutputSizing resolveSizing(const OutputSizing &global, const OutputSizingOverride &output,
                                  const OutputSizingOverride &desktop)
{
    return resolveOutputSizing(resolveOutputSizing(global, output), desktop);
}

/**
 * Where live MasterRatio / MasterCount writes go.
 *
 * A known (output, desktop) pair writes the DesktopOutput subgroup so each
 * virtual desktop keeps its own sizing. Otherwise an existing Output
 * subgroup if present, else the global [Tiling] group.
 */
enum class SizingWriteTarget {
    Global,
    Output,
    DesktopOutput,
};

inline SizingWriteTarget sizingWriteTarget(bool hasOutput, bool outputGroupExists, bool hasDesktop)
{
    if (hasOutput && hasDesktop) {
        return SizingWriteTarget::DesktopOutput;
    }
    if (hasOutput && outputGroupExists) {
        return SizingWriteTarget::Output;
    }
    return SizingWriteTarget::Global;
}

} // namespace KWin::tilingconfig
