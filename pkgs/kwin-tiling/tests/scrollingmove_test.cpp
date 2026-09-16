/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for scrollingmove.h — three-column fixture for
    issue #42 (niri W1-1). Run via tests/run.sh or:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/scrollingmove_test \
            pkgs/kwin-tiling/tests/scrollingmove_test.cpp && /tmp/scrollingmove_test
*/

#include "../src/tiles/scrollingmove.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace KWin::scrollingmove;

int main()
{
    // Three-column fixture: [A] [B,C] [D] — B stacked above C in the middle.
    const std::vector<std::vector<int>> fixture{{1}, {2, 3}, {4}};

    // --- Meta+Alt+Down: swap inside the column, column index unchanged ---
    {
        auto cols = fixture;
        assert(columnIndexOf(cols, 2) == 1);
        assert(moveWindowInColumn(cols, 2, +1));
        assert((cols == std::vector<std::vector<int>>{{1}, {3, 2}, {4}}));
        assert(columnIndexOf(cols, 2) == 1);
        assert(columnIndexOf(cols, 3) == 1);
        assert(columnIndexOf(cols, 1) == 0);
        assert(columnIndexOf(cols, 4) == 2);
        // Neighbour columns untouched (not a column reorder).
        assert((cols[0] == std::vector<int>{1}));
        assert((cols[2] == std::vector<int>{4}));
    }

    // Dispatcher Down matches moveWindowInColumn.
    {
        auto cols = fixture;
        assert(moveInDirection(cols, 2, Direction::Down));
        assert((cols == std::vector<std::vector<int>>{{1}, {3, 2}, {4}}));
        assert(columnIndexOf(cols, 2) == 1);
    }

    // Up restores; already-at-edge is a no-op (not a column move).
    {
        auto cols = fixture;
        assert(moveInDirection(cols, 2, Direction::Up) == false);
        assert(cols == fixture);
        assert(moveInDirection(cols, 3, Direction::Down) == false);
        assert(cols == fixture);
        assert(moveInDirection(cols, 3, Direction::Up));
        assert((cols == std::vector<std::vector<int>>{{1}, {3, 2}, {4}}));
        assert(columnIndexOf(cols, 3) == 1);
    }

    // --- Left/Right: slide the whole column (not consume-or-expel) ---
    {
        auto cols = fixture;
        assert(moveInDirection(cols, 2, Direction::Right));
        // Middle stacked column slides past D; B and C stay together.
        assert((cols == std::vector<std::vector<int>>{{1}, {4}, {2, 3}}));
        assert(columnIndexOf(cols, 2) == 2);
        assert(columnIndexOf(cols, 3) == 2);
        assert(int(cols.size()) == 3); // not expelled into a 4th column
        assert(int(cols[2].size()) == 2); // not consumed into D
    }

    {
        auto cols = fixture;
        assert(moveInDirection(cols, 2, Direction::Left));
        assert((cols == std::vector<std::vector<int>>{{2, 3}, {1}, {4}}));
        assert(columnIndexOf(cols, 2) == 0);
        assert(int(cols[0].size()) == 2); // not merged into A
        assert(int(cols.size()) == 3);
    }

    // Left/Right at the strip edge is a no-op (does not consume).
    {
        auto cols = fixture;
        assert(moveInDirection(cols, 1, Direction::Left) == false);
        assert(cols == fixture);
        assert(moveInDirection(cols, 4, Direction::Right) == false);
        assert(cols == fixture);
    }

    // Missing window / zero delta.
    {
        auto cols = fixture;
        assert(moveWindowInColumn(cols, 99, +1) == false);
        assert(moveColumn(cols, 99, +1) == false);
        assert(moveWindowInColumn(cols, 2, 0) == false);
        assert(cols == fixture);
    }

    // stepIndex: sign only, clamped.
    assert(stepIndex(1, +5, 3) == 2);
    assert(stepIndex(1, -5, 3) == 0);
    assert(stepIndex(0, -1, 3) == 0);
    assert(stepIndex(2, +1, 3) == 2);
    assert(stepIndex(1, 0, 3) == 1);

    std::puts("scrollingmove_test: OK");
    return 0;
}
