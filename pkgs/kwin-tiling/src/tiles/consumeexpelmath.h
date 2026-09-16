/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <vector>

// niri consume-or-expel-window-left/right decision matrix, free of KWin/Qt so
// the solo×stacked × left×right cases (and first/last-column no-ops) can be
// unit-tested without linking the compositor (tests/consumeexpelmath_test.cpp).
//
//   |            | Left                              | Right                             |
//   | Solo (1)   | merge into left neighbour         | merge into right neighbour        |
//   | Stacked    | expel into a new column on the left | expel into a new column on the right |
//
// First column + left, last column + right: no-op (no crash). Existing
// consume/expel (always merge-left / always split-right) are separate actions.

namespace KWin::consumeexpelmath
{

enum class Direction {
    Left,
    Right,
};

enum class Kind {
    NoOp,
    Merge, // append the window onto an existing neighbour column
    Expel, // insert a new one-window column on that side
};

struct Plan {
    Kind kind = Kind::NoOp;
    // Merge: neighbour column index in the *current* strip (before removal).
    // Expel: insert index for the new column (after the window is detached from
    // its source, which stays in place).
    int targetColumn = -1;
};

inline Plan plan(int columnCount, int columnIndex, int tilesInColumn, Direction direction)
{
    if (columnCount <= 0 || columnIndex < 0 || columnIndex >= columnCount || tilesInColumn <= 0) {
        return {};
    }
    const bool left = direction == Direction::Left;
    if (tilesInColumn == 1) {
        if (left) {
            if (columnIndex == 0) {
                return {};
            }
            return {Kind::Merge, columnIndex - 1};
        }
        if (columnIndex + 1 == columnCount) {
            return {};
        }
        return {Kind::Merge, columnIndex + 1};
    }
    // Stacked: new column immediately on that side of the remaining tiles.
    return {Kind::Expel, left ? columnIndex : columnIndex + 1};
}

// After a solo merge the emptied source column is removed. Neighbours to its
// right shift left by one; the left neighbour's index is unchanged.
inline int mergeDestAfterRemove(int sourceColumn, int destColumn)
{
    if (sourceColumn < destColumn) {
        return destColumn - 1;
    }
    return destColumn;
}

// Apply the plan to a strip of columns (each a list of window ids). Returns
// false on NoOp / invalid focus; the strip is unchanged then.
inline bool apply(std::vector<std::vector<int>> &columns, int columnIndex, int leafIndex, Direction direction)
{
    if (columnIndex < 0 || columnIndex >= static_cast<int>(columns.size())) {
        return false;
    }
    auto &source = columns[static_cast<size_t>(columnIndex)];
    if (leafIndex < 0 || leafIndex >= static_cast<int>(source.size())) {
        return false;
    }
    const Plan p = plan(static_cast<int>(columns.size()), columnIndex, static_cast<int>(source.size()), direction);
    if (p.kind == Kind::NoOp) {
        return false;
    }
    const int window = source[static_cast<size_t>(leafIndex)];
    source.erase(source.begin() + leafIndex);
    if (p.kind == Kind::Merge) {
        const bool emptied = source.empty();
        int dest = p.targetColumn;
        if (emptied) {
            columns.erase(columns.begin() + columnIndex);
            dest = mergeDestAfterRemove(columnIndex, p.targetColumn);
        }
        columns[static_cast<size_t>(dest)].push_back(window);
        return true;
    }
    columns.insert(columns.begin() + p.targetColumn, std::vector<int>{window});
    return true;
}

} // namespace KWin::consumeexpelmath
