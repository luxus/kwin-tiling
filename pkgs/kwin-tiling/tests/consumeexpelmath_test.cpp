/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for consumeexpelmath.h — niri consume-or-expel
    matrix (solo vs stacked × left vs right) plus first/last-column no-ops.

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/consumeexpelmath_test \
            pkgs/kwin-tiling/tests/consumeexpelmath_test.cpp && /tmp/consumeexpelmath_test
*/

#include "../src/tiles/consumeexpelmath.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace KWin::consumeexpelmath;
using Strip = std::vector<std::vector<int>>;

static void assertStrip(const Strip &got, const Strip &want)
{
    assert(got == want);
}

int main()
{
    // --- plan(): documented matrix on a 2-column strip (solo | stacked) ---
    {
        // col 0 solo (1 tile), col 1 stacked (2 tiles)
        const Plan soloLeft = plan(2, 0, 1, Direction::Left);
        assert(soloLeft.kind == Kind::NoOp); // first column

        const Plan soloRight = plan(2, 0, 1, Direction::Right);
        assert(soloRight.kind == Kind::Merge && soloRight.targetColumn == 1);

        const Plan stackedLeft = plan(2, 1, 2, Direction::Left);
        assert(stackedLeft.kind == Kind::Expel && stackedLeft.targetColumn == 1);

        const Plan stackedRight = plan(2, 1, 2, Direction::Right);
        assert(stackedRight.kind == Kind::Expel && stackedRight.targetColumn == 2);
    }

    // Last-column solo → right is no-op; left merges.
    {
        const Plan lastRight = plan(2, 1, 1, Direction::Right);
        assert(lastRight.kind == Kind::NoOp);
        const Plan lastLeft = plan(2, 1, 1, Direction::Left);
        assert(lastLeft.kind == Kind::Merge && lastLeft.targetColumn == 0);
    }

    // Single column, single window: both sides no-op (first and last).
    {
        assert(plan(1, 0, 1, Direction::Left).kind == Kind::NoOp);
        assert(plan(1, 0, 1, Direction::Right).kind == Kind::NoOp);
    }

    // Single stacked column: expel either side is valid.
    {
        const Plan left = plan(1, 0, 3, Direction::Left);
        assert(left.kind == Kind::Expel && left.targetColumn == 0);
        const Plan right = plan(1, 0, 3, Direction::Right);
        assert(right.kind == Kind::Expel && right.targetColumn == 1);
    }

    // Degenerate inputs never crash and never invent work.
    {
        assert(plan(0, 0, 1, Direction::Left).kind == Kind::NoOp);
        assert(plan(2, -1, 1, Direction::Right).kind == Kind::NoOp);
        assert(plan(2, 2, 1, Direction::Left).kind == Kind::NoOp);
        assert(plan(2, 0, 0, Direction::Right).kind == Kind::NoOp);
    }

    // mergeDestAfterRemove: right neighbour shifts; left neighbour does not.
    {
        assert(mergeDestAfterRemove(0, 1) == 0); // merge right, source removed
        assert(mergeDestAfterRemove(1, 0) == 0); // merge left, dest unchanged
    }

    // --- apply(): solo vs stacked × left vs right on {{1},{2,3}} ---
    {
        // Solo × left (first column): no-op.
        Strip a{{1}, {2, 3}};
        assert(!apply(a, 0, 0, Direction::Left));
        assertStrip(a, {{1}, {2, 3}});

        // Solo × right: merge 1 onto the neighbour.
        Strip b{{1}, {2, 3}};
        assert(apply(b, 0, 0, Direction::Right));
        assertStrip(b, {{2, 3, 1}});

        // Stacked × left (focus 2): expel into a new column on the left of {3}.
        Strip c{{1}, {2, 3}};
        assert(apply(c, 1, 0, Direction::Left));
        assertStrip(c, {{1}, {2}, {3}});

        // Stacked × right (focus 2): expel into a new column on the right of {3}.
        Strip d{{1}, {2, 3}};
        assert(apply(d, 1, 0, Direction::Right));
        assertStrip(d, {{1}, {3}, {2}});
    }

    // Last-column solo merge left; last-column solo merge right is no-op.
    {
        Strip left{{1, 2}, {3}};
        assert(apply(left, 1, 0, Direction::Left));
        assertStrip(left, {{1, 2, 3}});

        Strip right{{1, 2}, {3}};
        assert(!apply(right, 1, 0, Direction::Right));
        assertStrip(right, {{1, 2}, {3}});
    }

    // Middle tile of a stack: expel keeps siblings together.
    {
        Strip left{{1}, {2, 3, 4}};
        assert(apply(left, 1, 1, Direction::Left));
        assertStrip(left, {{1}, {3}, {2, 4}});

        Strip right{{1}, {2, 3, 4}};
        assert(apply(right, 1, 1, Direction::Right));
        assertStrip(right, {{1}, {2, 4}, {3}});
    }

    // Invalid focus: no crash, unchanged.
    {
        Strip s{{1}, {2}};
        assert(!apply(s, 9, 0, Direction::Left));
        assert(!apply(s, 0, 9, Direction::Right));
        assertStrip(s, {{1}, {2}});
    }

    std::puts("consumeexpelmath_test: OK");
    return 0;
}
