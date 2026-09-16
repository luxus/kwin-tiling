/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <utility>
#include <vector>

// Pure scrolling-strip move rules (issue #42 / niri W1-1).
//
// A strip is a left-to-right list of columns; each column is top-to-bottom
// window ids. No KWin/Qt types — tested in scrollingmove_test.cpp.
//
// Directional Meta+Alt semantics in Scrolling:
//   Up/Down    → moveWindowInColumn  (niri move-window-up/down)
//   Left/Right → moveColumn          (niri move-column-left/right)
// Consume/expel stays Meta+Shift+[ / ] and is not what Left/Right does.

namespace KWin::scrollingmove
{

enum class Direction {
    Left,
    Right,
    Up,
    Down,
};

// One step along an axis: sign of delta only, clamped to [0, count).
inline int stepIndex(int idx, int delta, int count)
{
    if (idx < 0 || count <= 0 || delta == 0) {
        return idx;
    }
    return std::clamp(idx + (delta > 0 ? 1 : -1), 0, count - 1);
}

// Column and leaf of windowId, or {-1, -1} if missing.
inline std::pair<int, int> locate(const std::vector<std::vector<int>> &columns, int windowId)
{
    for (int c = 0; c < int(columns.size()); ++c) {
        const auto &col = columns[size_t(c)];
        for (int l = 0; l < int(col.size()); ++l) {
            if (col[size_t(l)] == windowId) {
                return {c, l};
            }
        }
    }
    return {-1, -1};
}

inline int columnIndexOf(const std::vector<std::vector<int>> &columns, int windowId)
{
    return locate(columns, windowId).first;
}

// niri move-window-up/down: swap with the adjacent leaf in the same column.
// Column index is unchanged. Returns false if missing, delta is 0, or already
// at that edge of the column.
inline bool moveWindowInColumn(std::vector<std::vector<int>> &columns, int windowId, int delta)
{
    const auto [c, l] = locate(columns, windowId);
    if (c < 0 || delta == 0) {
        return false;
    }
    auto &col = columns[size_t(c)];
    const int target = stepIndex(l, delta, int(col.size()));
    if (target == l) {
        return false;
    }
    std::swap(col[size_t(l)], col[size_t(target)]);
    return true;
}

// niri move-column-left/right: slide the window's whole column one step along
// the strip. Windows stay stacked together; nothing is consumed or expelled.
inline bool moveColumn(std::vector<std::vector<int>> &columns, int windowId, int delta)
{
    const int c = columnIndexOf(columns, windowId);
    if (c < 0 || delta == 0) {
        return false;
    }
    const int target = stepIndex(c, delta, int(columns.size()));
    if (target == c) {
        return false;
    }
    std::swap(columns[size_t(c)], columns[size_t(target)]);
    return true;
}

// Meta+Alt dispatcher: Up/Down in-column, Left/Right whole column.
inline bool moveInDirection(std::vector<std::vector<int>> &columns, int windowId, Direction direction)
{
    switch (direction) {
    case Direction::Up:
        return moveWindowInColumn(columns, windowId, -1);
    case Direction::Down:
        return moveWindowInColumn(columns, windowId, +1);
    case Direction::Left:
        return moveColumn(columns, windowId, -1);
    case Direction::Right:
        return moveColumn(columns, windowId, +1);
    }
    return false;
}

} // namespace KWin::scrollingmove
