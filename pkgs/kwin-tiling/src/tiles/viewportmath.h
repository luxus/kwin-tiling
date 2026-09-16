/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// Pure Scrolling viewport policy: never / always / on-overflow / pair-center
// scroll offset. No KWin / Qt types — unit-tested standalone
// (tests/viewportmath_test.cpp). Mirrors niri's
// compute_new_view_offset_for_column (center-focused-column) plus Direktor /
// Karousel pair-center. Coordinates are view-width fractions: the viewport is
// [offset, offset+1).

namespace KWin::viewportmath
{

enum class CenterFocusedColumn {
    Never = 0,
    Always = 1,
    OnOverflow = 2,
    PairCenter = 3, // Karousel / Direktor: always pair-peek when n > 2
};

inline std::string normalizeModeName(std::string_view name)
{
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c == '-' || c == '_' || c == ' ') {
            continue;
        }
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
        out.push_back(c);
    }
    return out;
}

/** Parse kcfg / kwinrc CenterFocusedColumn. Unknown names fall back to Never. */
inline CenterFocusedColumn parseCenterFocusedColumn(std::string_view name)
{
    const std::string n = normalizeModeName(name);
    if (n == "always") {
        return CenterFocusedColumn::Always;
    }
    if (n == "onoverflow") {
        return CenterFocusedColumn::OnOverflow;
    }
    if (n == "paircenter" || n == "centerpairs" || n == "karousel") {
        return CenterFocusedColumn::PairCenter;
    }
    return CenterFocusedColumn::Never;
}

inline const char *centerFocusedColumnToString(CenterFocusedColumn mode)
{
    switch (mode) {
    case CenterFocusedColumn::Always:
        return "always";
    case CenterFocusedColumn::OnOverflow:
        return "on-overflow";
    case CenterFocusedColumn::PairCenter:
        return "pair-center";
    case CenterFocusedColumn::Never:
        break;
    }
    return "never";
}

inline double totalWidth(const std::vector<double> &widths)
{
    double total = 0.0;
    for (const double w : widths) {
        total += w;
    }
    return total;
}

inline double columnLeft(const std::vector<double> &widths, int index)
{
    double left = 0.0;
    const int n = static_cast<int>(widths.size());
    const int lim = std::clamp(index, 0, n);
    for (int i = 0; i < lim; ++i) {
        left += widths[static_cast<size_t>(i)];
    }
    return left;
}

/** Viewport-relative column rect. x may be < 0 or x+width > 1; width is never clipped. */
struct ColumnRect {
    double x = 0.0;
    double width = 0.0;
};

enum class Visibility {
    FullyVisible, // entirely inside [0, 1]
    Peeking,      // overlaps the viewport but not fully inside it
    Offscreen,    // no overlap with [0, 1]
};

/**
 * Place every column at (cumulative strip X - scrollOffset) with its stored
 * width unchanged. Does **not** intersect with [0, 1] — peeking neighbours
 * keep Column::width (W0-2 / #41). Path A overflow is what makes that
 * geometry legal on KWin tiles; this header does not copy overflow/pin.
 */
inline std::vector<ColumnRect> placeColumns(const std::vector<double> &widths, double scrollOffset)
{
    std::vector<ColumnRect> out;
    out.reserve(widths.size());
    double x = 0.0;
    for (const double w : widths) {
        out.push_back({x - scrollOffset, w});
        x += w;
    }
    return out;
}

inline Visibility visibility(const ColumnRect &r)
{
    const double left = r.x;
    const double right = r.x + r.width;
    if (right <= 0.0 || left >= 1.0) {
        return Visibility::Offscreen;
    }
    if (left >= 0.0 && right <= 1.0) {
        return Visibility::FullyVisible;
    }
    return Visibility::Peeking;
}

/** Visible sliver after a [0,1] clamp. W0-2 forbids using this as placed width. */
inline double clippedViewportWidth(const ColumnRect &r)
{
    const double left = std::max(r.x, 0.0);
    const double right = std::min(r.x + r.width, 1.0);
    return std::max(0.0, right - left);
}

/**
 * Hide fully off-viewport columns (Path A coexistence). Peeking columns stay
 * shown at full width. Monocle hide is reflowZoomed, not this.
 */
inline bool hideForOffscreen(const ColumnRect &r)
{
    return visibility(r) == Visibility::Offscreen;
}

/**
 * Fit-scroll (niri "never"): if the strip fits the view, center the whole
 * strip; otherwise scroll just enough to bring [left, left+width) into
 * [currentOffset, currentOffset+1), leaving the camera still when the
 * column is already fully visible. Clamped to the strip when total > 1.
 */
inline double fitScrollOffset(double left, double width, double currentOffset, double total)
{
    if (total <= 1.0) {
        return (total - 1.0) / 2.0;
    }
    const double right = left + width;
    double offset = currentOffset;
    if (left < currentOffset) {
        offset = left;
    } else if (right > currentOffset + 1.0) {
        offset = right - 1.0;
    }
    return std::clamp(offset, 0.0, total - 1.0);
}

/**
 * Center the column in the view. Columns as wide as (or wider than) the view
 * left-align. Not clamped to the strip: centering the first/last column can
 * show empty space and park neighbours past the edge (Path A overflow tiles
 * keep peeking width; fully off-viewport columns stay hidden).
 */
inline double centerScrollOffset(double left, double width)
{
    if (width >= 1.0) {
        return left;
    }
    return left - (1.0 - width) / 2.0;
}

/**
 * niri on-overflow: center only when the focused column and its neighbour
 * toward the previously focused column do not both fit in the view.
 * Missing/same prev, or a single column, is not overflow (fit).
 */
inline bool neighborOverflows(const std::vector<double> &widths, int activeIndex, int prevIndex)
{
    const int n = static_cast<int>(widths.size());
    if (n < 2 || prevIndex < 0 || activeIndex < 0 || activeIndex >= n || prevIndex == activeIndex) {
        return false;
    }
    const int sourceIndex = (prevIndex > activeIndex) ? std::min(activeIndex + 1, n - 1)
                                                       : std::max(activeIndex - 1, 0);
    if (sourceIndex == activeIndex) {
        return false;
    }
    return widths[static_cast<size_t>(sourceIndex)] + widths[static_cast<size_t>(activeIndex)] > 1.0;
}

/**
 * Direktor / Karousel pair-center (n > 2): keep a dead-zone so one neighbour
 * of @p defaultWidth always has room beside the active column — not only when
 * the actual neighbour overflows. Viewport-relative active left stays in
 * [minX, maxX] with minX = (1 - width - defaultWidth) / 2. If the pair cannot
 * fit (minX < 0), fall back to fit (active fully visible, wide columns
 * left-align). Does not clamp to the strip (same as Always).
 */
inline double pairCenterScrollOffset(double left, double width, double currentOffset,
                                     double defaultWidth)
{
    double minX = (1.0 - width - defaultWidth) / 2.0;
    double maxX = 1.0 - width - minX;
    if (minX < 0.0) {
        minX = 0.0;
        maxX = std::max(0.0, 1.0 - width);
    }
    const double viewLeft = left - currentOffset;
    if (viewLeft < minX) {
        return left - minX;
    }
    if (viewLeft > maxX) {
        return left - maxX;
    }
    return currentOffset;
}

inline double scrollOffsetForFocus(const std::vector<double> &widths, int activeIndex, int prevIndex,
                                   double currentOffset, CenterFocusedColumn mode,
                                   double defaultWidth = 0.5)
{
    if (widths.empty()) {
        return 0.0;
    }
    const int n = static_cast<int>(widths.size());
    const int ac = std::clamp(activeIndex, 0, n - 1);
    const double left = columnLeft(widths, ac);
    const double width = widths[static_cast<size_t>(ac)];
    const double total = totalWidth(widths);

    if (mode == CenterFocusedColumn::PairCenter) {
        // Pair-peek only when there is a strip to peek into. 1–2 columns fit
        // like Never (including centering a strip that is narrower than the
        // view). Always pair-peek for n > 2 — not only on overflow.
        if (n <= 2) {
            return fitScrollOffset(left, width, currentOffset, total);
        }
        return pairCenterScrollOffset(left, width, currentOffset, defaultWidth);
    }

    const bool center = (mode == CenterFocusedColumn::Always)
        || (mode == CenterFocusedColumn::OnOverflow && neighborOverflows(widths, ac, prevIndex));
    if (center) {
        return centerScrollOffset(left, width);
    }
    return fitScrollOffset(left, width, currentOffset, total);
}

} // namespace KWin::viewportmath
