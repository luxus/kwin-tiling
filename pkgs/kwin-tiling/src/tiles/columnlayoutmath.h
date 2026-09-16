/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "columnmath.h"

#include <algorithm>
#include <utility>
#include <vector>

// Pure Columns-layout policy: how many equal-width columns to grow, where a
// new window lands, and how widths stay a partition of [0, 1].
//
// Distinct from Scrolling (viewport + independent column widths that need not
// sum to 1). No KWin/Qt — unit-tested in tests/columnlayoutmath_test.cpp.

namespace KWin::columnlayoutmath
{

inline constexpr int kMinMaxColumns = 2;
inline constexpr int kDefaultMaxColumns = 3;
inline constexpr int kMaxMaxColumns = 5;

inline constexpr double kMinColWidth = 0.08;
inline constexpr double kMaxColWidth = 0.85;

inline int clampMaxColumns(int n)
{
    return std::clamp(n, kMinMaxColumns, kMaxMaxColumns);
}

/** New window: grow a column until the cap, else stack in an existing one. */
struct AddTarget {
    bool openNewColumn = false;
    int column = 0; // used when !openNewColumn; also the append index when opening
};

/**
 * @p focusedColumn / @p fewestColumn are 0-based; pass -1 when unknown.
 * Ties for fewest are resolved by the caller (leftmost).
 */
inline AddTarget addTarget(int columnCount, int maxColumns, int focusedColumn, int fewestColumn)
{
    maxColumns = clampMaxColumns(maxColumns);
    if (columnCount < maxColumns) {
        return {true, std::max(columnCount, 0)};
    }
    int col = focusedColumn;
    if (col < 0 || col >= columnCount) {
        col = fewestColumn;
    }
    if (col < 0 || col >= columnCount) {
        col = 0;
    }
    return {false, col};
}

/** Leftmost column among those with the smallest window count. */
inline int fewestColumn(const std::vector<int> &counts)
{
    if (counts.empty()) {
        return -1;
    }
    int best = 0;
    int fewest = counts[0];
    for (int i = 1; i < int(counts.size()); ++i) {
        if (counts[static_cast<size_t>(i)] < fewest) {
            fewest = counts[static_cast<size_t>(i)];
            best = i;
        }
    }
    return best;
}

/** Equal widths summing to exactly 1.0 (last column absorbs remainder). */
inline std::vector<double> equalWidths(int n)
{
    std::vector<double> out;
    if (n <= 0) {
        return out;
    }
    if (n == 1) {
        return {1.0};
    }
    out.assign(static_cast<size_t>(n), 1.0 / static_cast<double>(n));
    double sum = 0.0;
    for (int i = 0; i < n - 1; ++i) {
        sum += out[static_cast<size_t>(i)];
    }
    out[static_cast<size_t>(n - 1)] = 1.0 - sum;
    return out;
}

/**
 * Scale widths so they sum to 1.0. Intermediate columns are clamped to
 * [kMinColWidth, kMaxColWidth]; the last absorbs the remainder. If that leaves
 * the last column out of range, fall back to equalWidths.
 */
inline std::vector<double> normalizeWidths(std::vector<double> widths)
{
    const int n = int(widths.size());
    if (n <= 0) {
        return widths;
    }
    if (n == 1) {
        widths[0] = 1.0;
        return widths;
    }
    double total = 0.0;
    for (double w : widths) {
        total += w;
    }
    if (total <= 0.0) {
        return equalWidths(n);
    }
    double sum = 0.0;
    for (int i = 0; i < n - 1; ++i) {
        const double clamped = std::clamp(widths[static_cast<size_t>(i)] / total, kMinColWidth, kMaxColWidth);
        widths[static_cast<size_t>(i)] = clamped;
        sum += clamped;
    }
    widths[static_cast<size_t>(n - 1)] = 1.0 - sum;
    if (widths[static_cast<size_t>(n - 1)] < kMinColWidth
        || widths[static_cast<size_t>(n - 1)] > kMaxColWidth) {
        return equalWidths(n);
    }
    return widths;
}

/**
 * Move @p delta of width from right to left (left += delta, right -= delta).
 * Returns false and leaves both unchanged if either would leave [kMin, kMax].
 */
inline bool transferWidth(double &left, double &right, double delta)
{
    const double newLeft = left + delta;
    const double newRight = right - delta;
    if (newLeft < kMinColWidth || newLeft > kMaxColWidth
        || newRight < kMinColWidth || newRight > kMaxColWidth) {
        return false;
    }
    left = newLeft;
    right = newRight;
    return true;
}

/** Column index whose [x, x+w) contains @p relX in [0, 1]; last if past the end. */
inline int columnAtX(const std::vector<double> &widths, double relX)
{
    if (widths.empty()) {
        return -1;
    }
    relX = std::clamp(relX, 0.0, 1.0);
    double acc = 0.0;
    for (int i = 0; i < int(widths.size()); ++i) {
        acc += widths[static_cast<size_t>(i)];
        if (relX <= acc || i == int(widths.size()) - 1) {
            return i;
        }
    }
    return int(widths.size()) - 1;
}

/**
 * Consume: prefer the column to the right, else the one to the left.
 * Returns -1 when there is nowhere to merge into.
 */
inline int consumeTargetColumn(int col, int columnCount)
{
    if (col < 0 || col >= columnCount || columnCount < 2) {
        return -1;
    }
    if (col + 1 < columnCount) {
        return col + 1;
    }
    return col - 1;
}

/**
 * Expel: index at which to insert a new single-window column. -1 when the
 * window is already alone, or the layout is at the column cap.
 * New column goes to the right of @p col, or to its left if @p col is last.
 */
inline int expelInsertAt(int col, int columnCount, int maxColumns, int windowsInColumn)
{
    maxColumns = clampMaxColumns(maxColumns);
    if (col < 0 || col >= columnCount || windowsInColumn <= 1 || columnCount >= maxColumns) {
        return -1;
    }
    return (col == columnCount - 1) ? col : col + 1;
}

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 1.0;
};

/** Horizontal strips for column widths (each full height). */
inline std::vector<Rect> columnRects(const std::vector<double> &widths)
{
    std::vector<Rect> out;
    out.reserve(widths.size());
    double x = 0.0;
    for (double w : widths) {
        out.push_back({x, 0.0, w, 1.0});
        x += w;
    }
    return out;
}

/** Vertical stack of @p rowCount equal-height cells inside a column rect. */
inline std::vector<Rect> stackRects(const Rect &column, int rowCount)
{
    std::vector<Rect> out;
    if (rowCount <= 0) {
        return out;
    }
    const auto dist = columnmath::distribute(std::vector<double>(static_cast<size_t>(rowCount), 1.0));
    out.reserve(dist.size());
    for (const auto &[yOff, h] : dist) {
        out.push_back({column.x, column.y + yOff * column.h, column.w, h * column.h});
    }
    return out;
}

} // namespace KWin::columnlayoutmath
