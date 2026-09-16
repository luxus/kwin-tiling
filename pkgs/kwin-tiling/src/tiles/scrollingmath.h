/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <vector>

// Pure Scrolling arithmetic. No KWin/Qt types — unit-tested standalone
// (tests/scrollingmath_test.cpp). Shared by several Scrolling PRs; keep every
// API here so rebase does not overwrite a sibling:
//   #51 / W0-2  placeColumns, fitScrollOffset, visibility
//   #55 / W2-1  expandToAvailableWidth
//   #45 / W1-4  drop-insert (isLowerHalf, consumeInsertIndex, stripInsertIndex)
//
// Distinct from Columns InsertAbove/Below (#32): drop-insert is the Scrolling
// strip (viewport-relative X + in-column Y half), not a shared drop-zone policy.

namespace KWin::scrollingmath
{

inline constexpr double kViewport = 1.0;
inline constexpr double kEps = 1e-9;

// --- viewport placement (#51 / issue #41) -----------------------------------

struct ColumnRect
{
    double x = 0.0;     // viewport-relative; may be < 0 or x+width > 1
    double width = 0.0; // stored Column::width — never clipped to the viewport
};

enum class Visibility {
    FullyVisible, // entirely inside [0, 1]
    Peeking,      // overlaps the viewport but not fully inside it
    Offscreen,    // no overlap with [0, 1]
};

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
    const int n = std::min(std::max(index, 0), static_cast<int>(widths.size()));
    for (int i = 0; i < n; ++i) {
        left += widths[static_cast<size_t>(i)];
    }
    return left;
}

// Fit-scroll (niri center-focused-column "never"): if the strip fits, centre
// it; otherwise move the viewport only far enough that the active column is
// fully visible. An already-visible active column must not move the camera.
inline double fitScrollOffset(const std::vector<double> &widths, int activeIndex, double currentOffset)
{
    if (widths.empty()) {
        return 0.0;
    }
    const double total = totalWidth(widths);
    if (total <= kViewport) {
        return (total - kViewport) / 2.0;
    }

    const int ac = std::clamp(activeIndex, 0, static_cast<int>(widths.size()) - 1);
    const double left = columnLeft(widths, ac);
    const double right = left + widths[static_cast<size_t>(ac)];

    double offset = currentOffset;
    if (left < offset) {
        offset = left;
    } else if (right > offset + kViewport) {
        offset = right - kViewport;
    }
    return std::clamp(offset, 0.0, total - kViewport);
}

// Place every column at (cumulative strip X - scrollOffset) with its stored
// width unchanged. Does **not** intersect with [0, 1].
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
    if (right <= 0.0 || left >= kViewport) {
        return Visibility::Offscreen;
    }
    if (left >= 0.0 && right <= kViewport) {
        return Visibility::FullyVisible;
    }
    return Visibility::Peeking;
}

// Visible sliver of a column after a [0,1] clamp. W0-2 forbids using this as
// the placed width: peeking neighbours must keep Column::width.
inline double clippedViewportWidth(const ColumnRect &r)
{
    const double left = std::max(r.x, 0.0);
    const double right = std::min(r.x + r.width, kViewport);
    return std::max(0.0, right - left);
}

// --- expand-column-to-available-width (#55 / issue #47) ---------------------

enum class ExpandAction {
    None,
    ToggleFullWidth,
    Grow,
};

struct ExpandPlan {
    ExpandAction action = ExpandAction::None;
    double newWidth = 0.0; // Grow: new width of the active column
    double scrollOffset = 0.0; // Grow: pin the leftmost fully visible column
};

inline bool fullyVisible(double left, double width, double scrollOffset)
{
    return left + kEps >= scrollOffset && left + width <= scrollOffset + kViewport + kEps;
}

// Centered strip when the columns fit the viewport — matches
// ScrollingLayoutEngine::scrollActiveIntoView for total <= 1.
inline double centeredScrollOffset(double total)
{
    return (total - kViewport) / 2.0;
}

inline ExpandPlan expandToAvailableWidth(const std::vector<double> &widths, int active, double scrollOffset)
{
    ExpandPlan plan;
    if (widths.empty() || active < 0 || active >= static_cast<int>(widths.size())) {
        return plan;
    }

    std::vector<double> lefts(widths.size(), 0.0);
    double x = 0.0;
    for (size_t i = 0; i < widths.size(); ++i) {
        lefts[i] = x;
        x += widths[i];
    }

    double widthTaken = 0.0;
    double leftmostLeft = 0.0;
    bool haveLeftmost = false;
    bool activeFullyVisible = false;
    bool countedOther = false;

    for (int i = 0; i < static_cast<int>(widths.size()); ++i) {
        const double left = lefts[static_cast<size_t>(i)];
        const double width = widths[static_cast<size_t>(i)];
        if (left + kEps < scrollOffset) {
            continue; // starts off-screen to the left
        }
        if (!haveLeftmost) {
            leftmostLeft = left;
            haveLeftmost = true;
        }
        if (scrollOffset + kViewport + kEps < left + width) {
            break; // this and everything to the right peek or sit off-screen
        }
        if (i == active) {
            activeFullyVisible = true;
        } else {
            countedOther = true;
        }
        widthTaken += width;
    }

    if (!activeFullyVisible) {
        return plan;
    }

    // Only the focused column is fully on-screen (maybe the only column):
    // toggle full width so a second invoke restores the previous size.
    if (!countedOther) {
        plan.action = ExpandAction::ToggleFullWidth;
        return plan;
    }

    const double available = kViewport - widthTaken;
    if (available <= kEps) {
        return plan;
    }

    plan.action = ExpandAction::Grow;
    plan.newWidth = std::min(kViewport, widths[static_cast<size_t>(active)] + available);
    plan.scrollOffset = leftmostLeft;
    return plan;
}

// Mutate only the active slot on Grow. Neighbours are left untouched.
inline void applyGrow(std::vector<double> &widths, int active, const ExpandPlan &plan)
{
    if (plan.action != ExpandAction::Grow || active < 0 || active >= static_cast<int>(widths.size())) {
        return;
    }
    widths[static_cast<size_t>(active)] = plan.newWidth;
}

// --- drop-insert (#45 / W1-4) -----------------------------------------------

// True when the cursor is in the lower half of a target tile (height <= 0
// treats the drop as below, i.e. append after that leaf).
inline bool isLowerHalf(double cursorY, double top, double height)
{
    if (height <= 0.0) {
        return true;
    }
    return (cursorY - top) >= 0.5 * height;
}

// Leaf index at which to consume a dropped window into the target's column.
// Upper half → insert at the target (above it); lower half → after it.
inline int consumeInsertIndex(int targetLeafIndex, bool lowerHalf)
{
    if (targetLeafIndex < 0) {
        return 0;
    }
    return lowerHalf ? targetLeafIndex + 1 : targetLeafIndex;
}

// Column index at which to insert a *new* column on an empty-space drop.
// @p relX is the cursor's viewport-relative X (0 = left of the work area).
// @p widths are each column's width in view-width fractions; @p scrollOffset
// is the engine's viewport left in the same units (may be negative when the
// strip is centred). Inserts after every column whose midpoint sits to the
// left of the cursor, so a drop on the gap between two columns lands between
// them.
inline int stripInsertIndex(double relX, const std::vector<double> &widths, double scrollOffset)
{
    if (widths.empty()) {
        return 0;
    }
    int index = 0;
    double stripX = 0.0;
    for (const double w : widths) {
        const double viewMid = (stripX - scrollOffset) + w / 2.0;
        if (relX >= viewMid) {
            ++index;
        }
        stripX += w;
    }
    return std::clamp(index, 0, static_cast<int>(widths.size()));
}

} // namespace KWin::scrollingmath
