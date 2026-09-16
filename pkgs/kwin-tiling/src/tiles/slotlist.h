/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "columnmath.h"

#include <algorithm>
#include <utility>
#include <vector>

// Pure ordered-slot + weight bookkeeping used by StackColumn.
// Slots are opaque ids; this module never knows about CustomTile/Window.
// Swap-target clamp is columnmath::movedIndex (same helper as moveByDelta).
// Tested in tests/slotlist_test.cpp.

namespace KWin::slotlist
{

inline constexpr double kDefaultWeight = 1.0;

struct Slot {
    int id = -1;
    double weight = kDefaultWeight;
};

// A slot removed from one list so it can be inserted into another, carrying
// its weight (Scrolling consume/expel, Centered/Scrolling cross-column swap).
struct Detached {
    int id = -1;
    double weight = kDefaultWeight;
    bool isValid() const
    {
        return id >= 0;
    }
};

// Insert position: append when `at` is out of range (StackColumn::insertWindow
// / attachLeaf).
inline int insertIndex(int at, int count)
{
    return (at < 0 || at > count) ? count : at;
}

// Neighbour at ±1, or -1 at the ends / for an invalid `from`.
inline int neighborIndex(int from, bool next, int count)
{
    if (from < 0 || from >= count) {
        return -1;
    }
    const int n = from + (next ? 1 : -1);
    return (n < 0 || n >= count) ? -1 : n;
}

// Clamp-and-swap destination; no-op when equal to `index`.
inline int swapTarget(int index, int delta, int count)
{
    return columnmath::movedIndex(count, index, delta);
}

inline bool swapByDelta(std::vector<Slot> &slots, int index, int delta)
{
    const int n = static_cast<int>(slots.size());
    const int target = swapTarget(index, delta, n);
    if (target == index) {
        return false;
    }
    std::swap(slots[static_cast<size_t>(index)], slots[static_cast<size_t>(target)]);
    return true;
}

inline Detached detachAt(std::vector<Slot> &slots, int index)
{
    Detached detached;
    if (index < 0 || index >= static_cast<int>(slots.size())) {
        return detached;
    }
    detached.id = slots[static_cast<size_t>(index)].id;
    detached.weight = slots[static_cast<size_t>(index)].weight;
    slots.erase(slots.begin() + index);
    return detached;
}

inline void attachAt(std::vector<Slot> &slots, const Detached &detached, int at = -1)
{
    if (!detached.isValid()) {
        return;
    }
    const int idx = insertIndex(at, static_cast<int>(slots.size()));
    slots.insert(slots.begin() + idx, Slot{detached.id, detached.weight});
}

// Remove at `from` in `fromList`, insert at `to` in `toList`, carrying weight.
inline bool transfer(std::vector<Slot> &fromList, int from, std::vector<Slot> &toList, int to)
{
    Detached detached = detachAt(fromList, from);
    if (!detached.isValid()) {
        return false;
    }
    attachAt(toList, detached, to);
    return true;
}

template<typename Pred>
inline bool pruneIf(std::vector<Slot> &slots, Pred pred)
{
    const auto before = slots.size();
    slots.erase(std::remove_if(slots.begin(), slots.end(), pred), slots.end());
    return slots.size() != before;
}

} // namespace KWin::slotlist
