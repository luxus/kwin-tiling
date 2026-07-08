/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for movestate.h — column indices, remove paths,
    shouldDestroySourceLeaf (shared with StackColumn::cancelMove), cancel vs
    clear phantom. Run via tests/run.sh or:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/movestate_test \
            pkgs/kwin-tiling/tests/movestate_test.cpp && /tmp/movestate_test
*/

#include "../src/tiles/movestate.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace KWin::movestate;

int main()
{
    // --- column index bookkeeping ---
    assert(afterWindowRemoved(-1, 0, true, false) == -1);
    assert(!moveIsOpen(-1));
    assert(moveIsOpen(0));
    assert(afterWindowRemoved(2, 2, true, true) == -1);
    assert(afterWindowRemoved(2, 0, true, false) == 1);
    assert(afterWindowRemoved(1, 2, true, false) == 1);
    assert(afterWindowRemoved(1, 1, true, false) == -1);

    // --- classifyRemove (Scrolling removeWindow branches) ---
    assert(classifyRemove(false, true, 0, 0) == RemovePath::NoMoveNormal);
    assert(classifyRemove(true, false, -1, 2) == RemovePath::CancelSource);
    assert(classifyRemove(true, true, 2, 2) == RemovePath::CancelSource);
    assert(classifyRemove(true, true, 0, 2) == RemovePath::SiblingOtherColumn);

    // shouldCancelMoveOnRemove aligns with CancelSource classification.
    assert(shouldCancelMoveOnRemove(true, false, -1, 2, true));
    assert(shouldCancelMoveOnRemove(true, true, 2, 2, false));
    assert(!shouldCancelMoveOnRemove(true, true, 0, 2, false));
    assert(!shouldCancelMoveOnRemove(false, true, 0, 0, false));

    // --- shouldDestroySourceLeaf: shipped rule used by StackColumn ---
    assert(shouldDestroySourceLeaf(true, false));  // empty ghost
    assert(shouldDestroySourceLeaf(false, true)); // still holds drag
    assert(shouldDestroySourceLeaf(true, true));
    assert(!shouldDestroySourceLeaf(false, false)); // unrelated leaf content

    // --- pure cancelMoveLeaf uses the same destroy rule ---
    {
        std::vector<LeafSlot> leaves = {
            {10, false, false},
            {20, true, false}, // empty source
        };
        int sourceId = 20;

        // Anti-pattern: clear only → phantom empty leaf remains.
        int cleared = sourceId;
        clearMoveLeafOnly(cleared);
        assert(cleared == -1);
        assert(hasEmptyLeaf(leaves));

        // Correct: cancel destroys empty source via shouldDestroySourceLeaf.
        sourceId = 20;
        assert(cancelMoveLeaf(leaves, sourceId));
        assert(sourceId == -1);
        assert(!hasEmptyLeaf(leaves));
        assert(leaves.size() == 1 && leaves[0].id == 10);
    }

    {
        std::vector<LeafSlot> leaves = {{1, false, true}};
        int sourceId = 1;
        assert(cancelMoveLeaf(leaves, sourceId));
        assert(leaves.empty());
    }

    {
        // Non-empty leaf that does not hold drag → do not destroy.
        std::vector<LeafSlot> leaves = {{3, false, false}};
        int sourceId = 3;
        assert(!cancelMoveLeaf(leaves, sourceId));
        assert(sourceId == -1); // source pointer still cleared
        assert(leaves.size() == 1);
    }

    // Sibling other column: index shifts, move stays open.
    {
        int moveCol = 2;
        assert(classifyRemove(true, true, 0, moveCol) == RemovePath::SiblingOtherColumn);
        moveCol = afterWindowRemoved(moveCol, 0, true, false);
        assert(moveCol == 1);
        assert(moveIsOpen(moveCol));
    }

    std::puts("movestate_test: OK");
    return 0;
}
