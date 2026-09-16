/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for slotlist.h (StackColumn order + weight).
    No KWin/Qt:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/slotlist_test \
            pkgs/kwin-tiling/tests/slotlist_test.cpp && /tmp/slotlist_test
*/

#include "../src/tiles/slotlist.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace KWin::slotlist;

static std::vector<int> idsOf(const std::vector<Slot> &slots)
{
    std::vector<int> ids;
    ids.reserve(slots.size());
    for (const Slot &s : slots) {
        ids.push_back(s.id);
    }
    return ids;
}

static std::vector<double> weightsOf(const std::vector<Slot> &slots)
{
    std::vector<double> w;
    w.reserve(slots.size());
    for (const Slot &s : slots) {
        w.push_back(s.weight);
    }
    return w;
}

int main()
{
    // insertIndex: append when at is out of range; at == count is a valid append.
    assert(insertIndex(-1, 3) == 3);
    assert(insertIndex(0, 3) == 0);
    assert(insertIndex(3, 3) == 3);
    assert(insertIndex(4, 3) == 3);
    assert(insertIndex(1, 3) == 1);
    assert(insertIndex(-1, 0) == 0);
    assert(insertIndex(0, 0) == 0);

    // neighborIndex: ±1, none at the ends, invalid from → -1.
    assert(neighborIndex(1, true, 3) == 2);
    assert(neighborIndex(1, false, 3) == 0);
    assert(neighborIndex(0, false, 3) == -1);
    assert(neighborIndex(2, true, 3) == -1);
    assert(neighborIndex(0, true, 1) == -1);
    assert(neighborIndex(-1, true, 3) == -1);
    assert(neighborIndex(3, false, 3) == -1);
    assert(neighborIndex(0, true, 0) == -1);

    // swapTarget / swapByDelta: clamp, pairwise swap, no-op at ends.
    {
        std::vector<Slot> slots{{0, 1.0}, {1, 2.0}, {2, 3.0}};
        assert(swapTarget(0, 1, 3) == 1);
        assert(swapTarget(2, 1, 3) == 2); // clamped
        assert(swapTarget(0, -1, 3) == 0);
        assert(swapByDelta(slots, 0, 1));
        assert((idsOf(slots) == std::vector<int>{1, 0, 2}));
        assert((weightsOf(slots) == std::vector<double>{2.0, 1.0, 3.0}));
        assert(!swapByDelta(slots, 0, -5)); // already at front
        assert((idsOf(slots) == std::vector<int>{1, 0, 2}));
        assert(!swapByDelta(slots, 2, 9));
    }

    // detachAt / attachAt: weight travels with the slot.
    {
        std::vector<Slot> a{{10, 2.5}, {11, 1.0}, {12, 0.5}};
        const Detached d = detachAt(a, 1);
        assert(d.isValid());
        assert(d.id == 11 && d.weight == 1.0);
        assert((idsOf(a) == std::vector<int>{10, 12}));
        assert(!detachAt(a, 9).isValid());
        assert(!detachAt(a, -1).isValid());

        std::vector<Slot> b{{20, 1.0}};
        attachAt(b, d, 0);
        assert((idsOf(b) == std::vector<int>{11, 20}));
        assert(b[0].weight == 1.0);

        Detached invalid;
        attachAt(b, invalid, 0); // no-op
        assert(b.size() == 2);

        // append when at is out of range
        attachAt(b, Detached{99, 4.0}, -1);
        assert(b.back().id == 99 && b.back().weight == 4.0);
    }

    // transfer: consume/expel (append into neighbour column).
    {
        std::vector<Slot> left{{1, 2.0}, {2, 1.0}};
        std::vector<Slot> right{{3, 1.0}};
        assert(transfer(left, 0, right, -1));
        assert((idsOf(left) == std::vector<int>{2}));
        assert((idsOf(right) == std::vector<int>{3, 1}));
        assert(right.back().weight == 2.0);
        assert(!transfer(left, 5, right, 0));
    }

    // Cross-column drag-swap: detach both, re-insert at the original indices.
    {
        std::vector<Slot> src{{1, 2.0}, {2, 1.0}};
        std::vector<Slot> dst{{3, 3.0}, {4, 1.0}};
        const int srcIdx = 0;
        const int tgtIdx = 1;
        Detached moved = detachAt(src, srcIdx);
        Detached target = detachAt(dst, tgtIdx);
        attachAt(src, target, srcIdx);
        attachAt(dst, moved, tgtIdx);
        assert((idsOf(src) == std::vector<int>{4, 2}));
        assert((idsOf(dst) == std::vector<int>{3, 1}));
        assert(src[0].weight == 1.0);
        assert(dst[1].weight == 2.0);
    }

    // pruneIf: remove matching slots, report whether anything changed.
    {
        std::vector<Slot> slots{{1, 1.0}, {-1, 1.0}, {3, 1.0}, {-1, 0.5}};
        assert(pruneIf(slots, [](const Slot &s) {
            return s.id < 0;
        }));
        assert((idsOf(slots) == std::vector<int>{1, 3}));
        assert(!pruneIf(slots, [](const Slot &s) {
            return s.id < 0;
        }));
        assert(pruneIf(slots, [](const Slot &s) {
            return s.weight < 2.0;
        }));
        assert(slots.empty());
        assert(!pruneIf(slots, [](const Slot &) {
            return true;
        }));
    }

    std::puts("slotlist_test: OK");
    return 0;
}
