/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <utility>
#include <vector>

// Pure Scrolling consume-into-column / expel-from-column (niri W1-3).
// No KWin / Qt types — tested in scrollingcolumn_test.cpp.
//
// consume-window-into-column: first tile of the next column appends to the
// focused column (bottom). The source column is dropped if emptied.
// expel-window-from-column: last (bottom) tile of the focused column becomes a
// new column immediately to the right.
//
// Principle 1: remaining columns keep their widths. The expelled column copies
// the source width so neither side is resized as a side effect.

namespace KWin::scrollingcolumn
{

struct Column {
    std::vector<int> windows; // top → bottom
    double width = 0.5;
};

struct Layout {
    std::vector<Column> columns;
    int active = 0; // focused column index
};

struct ConsumeOp {
    bool apply = false;
    int destCol = -1;
    int sourceCol = -1;
    int sourceLeaf = 0; // first of next
};

struct ExpelOp {
    bool apply = false;
    int sourceCol = -1;
    int sourceLeaf = -1; // last of focused
};

inline ConsumeOp planConsumeIntoColumn(int activeCol, int columnCount)
{
    ConsumeOp op;
    if (columnCount < 2 || activeCol < 0 || activeCol >= columnCount - 1) {
        return op;
    }
    op.apply = true;
    op.destCol = activeCol;
    op.sourceCol = activeCol + 1;
    op.sourceLeaf = 0;
    return op;
}

inline ExpelOp planExpelFromColumn(int activeCol, int columnCount, int sourceCount)
{
    ExpelOp op;
    if (activeCol < 0 || activeCol >= columnCount || sourceCount <= 1) {
        return op;
    }
    op.apply = true;
    op.sourceCol = activeCol;
    op.sourceLeaf = sourceCount - 1;
    return op;
}

// niri consume-window-into-column. Returns false on no-op.
inline bool consumeIntoColumn(Layout &layout)
{
    const auto op = planConsumeIntoColumn(layout.active, int(layout.columns.size()));
    if (!op.apply) {
        return false;
    }
    Column &src = layout.columns[size_t(op.sourceCol)];
    if (op.sourceLeaf < 0 || op.sourceLeaf >= int(src.windows.size())) {
        return false;
    }
    const int win = src.windows[size_t(op.sourceLeaf)];
    src.windows.erase(src.windows.begin() + op.sourceLeaf);
    layout.columns[size_t(op.destCol)].windows.push_back(win);
    if (src.windows.empty()) {
        layout.columns.erase(layout.columns.begin() + op.sourceCol);
    }
    return true;
}

// niri expel-window-from-column. Returns false on no-op.
inline bool expelFromColumn(Layout &layout)
{
    if (layout.active < 0 || layout.active >= int(layout.columns.size())) {
        return false;
    }
    Column &src = layout.columns[size_t(layout.active)];
    const auto op = planExpelFromColumn(layout.active, int(layout.columns.size()), int(src.windows.size()));
    if (!op.apply) {
        return false;
    }
    const int win = src.windows[size_t(op.sourceLeaf)];
    src.windows.erase(src.windows.begin() + op.sourceLeaf);
    Column neu;
    neu.width = src.width; // copy; do not change remaining column widths
    neu.windows.push_back(win);
    layout.columns.insert(layout.columns.begin() + op.sourceCol + 1, std::move(neu));
    return true;
}

} // namespace KWin::scrollingcolumn
