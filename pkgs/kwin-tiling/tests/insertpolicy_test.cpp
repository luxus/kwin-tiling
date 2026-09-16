/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for insertpolicy.h — run with:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/insertpolicy_test \
            pkgs/kwin-tiling/tests/insertpolicy_test.cpp && /tmp/insertpolicy_test
*/

#include "../src/tiles/insertpolicy.h"

#include <cassert>
#include <cstdio>

using namespace KWin::insertpolicy;

int main()
{
    assert(classifyRelativeY(0.0) == DropZone::InsertAbove);
    assert(classifyRelativeY(0.24) == DropZone::InsertAbove);
    assert(classifyRelativeY(kEdgeBand) == DropZone::Swap);
    assert(classifyRelativeY(0.5) == DropZone::Swap);
    assert(classifyRelativeY(0.75) == DropZone::Swap);
    assert(classifyRelativeY(0.76) == DropZone::InsertBelow);
    assert(classifyRelativeY(1.0) == DropZone::InsertBelow);
    assert(classifyRelativeY(-1.0) == DropZone::InsertAbove);
    assert(classifyRelativeY(2.0) == DropZone::InsertBelow);

    // Point in the top band of a 100-tall window at y=10.
    assert(classifyPoint(50, 20, 0, 10, 80, 100) == DropZone::InsertAbove);
    // Middle.
    assert(classifyPoint(50, 60, 0, 10, 80, 100) == DropZone::Swap);
    // Bottom band (y=10+100=110; 0.76*100=76 → py=86).
    assert(classifyPoint(50, 90, 0, 10, 80, 100) == DropZone::InsertBelow);
    assert(classifyPoint(0, 0, 0, 0, 10, 0) == DropZone::Swap);

    // Same column, source above target, InsertBelow: removing source shifts
    // the target up, then insert after it.
    {
        const InsertPos p = insertPos(/*src*/ 0, 0, 3, /*tgt*/ 0, 2, DropZone::InsertBelow);
        assert(!p.dropSourceColumn);
        assert(p.column == 0);
        assert(p.row == 2); // target row 2 → 1 after remove, then +1
    }

    // Same column, source below target, InsertAbove: target row unchanged.
    {
        const InsertPos p = insertPos(0, 2, 3, 0, 0, DropZone::InsertAbove);
        assert(!p.dropSourceColumn);
        assert(p.column == 0);
        assert(p.row == 0);
    }

    // Cross-column insert below; source column survives.
    {
        const InsertPos p = insertPos(0, 1, 2, 1, 0, DropZone::InsertBelow);
        assert(!p.dropSourceColumn);
        assert(p.column == 1);
        assert(p.row == 1);
    }

    // Source was alone: its column vanishes, target column index shifts left.
    {
        const InsertPos p = insertPos(0, 0, 1, 2, 1, DropZone::InsertAbove);
        assert(p.dropSourceColumn);
        assert(p.column == 1);
        assert(p.row == 1);
    }

    // Source alone to the right of target: target column index unchanged.
    {
        const InsertPos p = insertPos(2, 0, 1, 0, 0, DropZone::InsertBelow);
        assert(p.dropSourceColumn);
        assert(p.column == 0);
        assert(p.row == 1);
    }

    // Drop onto a still-present target (dropWindow, no prior remove).
    assert(insertRowAtTarget(2, DropZone::InsertAbove) == 2);
    assert(insertRowAtTarget(2, DropZone::InsertBelow) == 3);
    assert(insertRowAtTarget(-1, DropZone::InsertAbove) == 0);

    std::puts("insertpolicy_test: OK");
    return 0;
}
