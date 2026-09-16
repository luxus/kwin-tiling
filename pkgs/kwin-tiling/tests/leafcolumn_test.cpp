/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Pure StackColumn move lifecycle (begin → empty → cancel → destroy).
    Mirrors StackColumn::cancelMove via leafcolumn::Column + shouldDestroySourceLeaf.
*/

#include "../src/tiles/leafcolumn.h"

#include <cassert>
#include <cstdio>

using namespace KWin::leafcolumn;

int main()
{
    // --- Criterion path: beginMove → untile empty → cancelMove → destroyed ---
    {
        Column col;
        const int w1 = 10;
        const int w2 = 20;
        col.insertWindow(w1);
        col.insertWindow(w2);
        assert(col.count() == 2);

        col.beginMove(w1);
        assert(col.hasMoveSource());

        // KWin untile-for-drag leaves an empty source holder
        assert(col.emptySourceForDrag());
        assert(col.hasEmptyLeaf());
        assert(!col.containsWindow(w1));
        assert(col.count() == 2); // ghost still present

        // clearMoveOnly would leave the phantom:
        Column phantom = col;
        // re-setup phantom path
        Column bad;
        bad.insertWindow(w1);
        bad.insertWindow(w2);
        bad.beginMove(w1);
        bad.emptySourceForDrag();
        bad.clearMoveOnly();
        assert(bad.hasEmptyLeaf()); // PHANTOM
        assert(bad.destroyCount() == 0);

        // Correct: cancelMove destroys empty source
        const int destroysBefore = col.destroyCount();
        assert(col.cancelMove(w1));
        assert(col.destroyCount() == destroysBefore + 1);
        assert(!col.hasEmptyLeaf());
        assert(!col.hasMoveSource());
        assert(col.count() == 1);
        assert(col.containsWindow(w2));
    }

    // cancelMove while window still on leaf (no untile) also destroys
    {
        Column col;
        col.insertWindow(1);
        col.beginMove(1);
        assert(col.cancelMove(1));
        assert(col.count() == 0);
        assert(col.destroyCount() == 1);
    }

    // cancelMove with no open move is no-op
    {
        Column col;
        col.insertWindow(1);
        assert(!col.cancelMove(1));
        assert(col.count() == 1);
    }

    // removeWindow mid-drag clears move and destroys leaf
    {
        Column col;
        col.insertWindow(1);
        col.insertWindow(2);
        col.beginMove(1);
        assert(col.removeWindow(1));
        assert(!col.hasMoveSource());
        assert(col.count() == 1);
    }

    // pruneEmpty removes ghosts left without cancel
    {
        Column col;
        col.insertWindow(1);
        col.beginMove(1);
        col.emptySourceForDrag();
        col.clearMoveOnly(); // wrong path left ghost
        assert(col.hasEmptyLeaf());
        assert(col.pruneEmpty() == 1);
        assert(!col.hasEmptyLeaf());
        assert(col.count() == 0);
    }

    // Sibling remains after cancel of other window's drag
    {
        Column col;
        col.insertWindow(1);
        col.insertWindow(2);
        col.insertWindow(3);
        col.beginMove(2);
        col.emptySourceForDrag();
        assert(col.cancelMove(2));
        assert(col.count() == 2);
        assert(col.containsWindow(1));
        assert(col.containsWindow(3));
    }

    // --- issue #11: contains() misses mid-drag ghost; ownsGhostLeaf does not ---
    {
        Column col;
        col.insertWindow(1);
        col.insertWindow(2);
        col.beginMove(1);
        col.emptySourceForDrag();
        assert(!col.containsWindow(1)); // the naive guard would skip
        assert(col.ownsGhostLeaf(1));
        assert(!col.ownsGhostLeaf(2)); // sibling's close must not claim this ghost
        assert(col.shouldHandleRemove(1));
        assert(col.shouldHandleRemove(2)); // still in layout
        Column other;
        other.insertWindow(3);
        assert(!other.shouldHandleRemove(1)); // foreign engine: no reflow
        assert(!other.ownsGhostLeaf(1));
    }

    // cancelMove of a sibling must not destroy this window's ghost
    {
        Column col;
        col.insertWindow(1);
        col.insertWindow(2);
        col.beginMove(1);
        col.emptySourceForDrag();
        assert(!col.cancelMove(2));
        assert(col.hasEmptyLeaf());
        assert(col.hasMoveSource());
        assert(col.ownsGhostLeaf(1));
        assert(col.cancelMove(1));
        assert(!col.hasEmptyLeaf());
        assert(!col.hasMoveSource());
    }

    // shouldHandleRemove + cancel clears the ghost (engine removeWindow path)
    {
        Column col;
        col.insertWindow(1);
        col.beginMove(1);
        col.emptySourceForDrag();
        assert(col.shouldHandleRemove(1));
        assert(col.cancelMove(1));
        assert(!col.hasEmptyLeaf());
        assert(col.count() == 0);
        assert(col.destroyCount() == 1);
    }

    std::puts("leafcolumn_test: OK");
    return 0;
}
