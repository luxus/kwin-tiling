/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <vector>

// Pure interactive-move bookkeeping used by StackColumn and layout engines.
// StackColumn::cancelMove must call shouldDestroySourceLeaf — not a parallel
// reimplementation. No KWin/Qt (tests/movestate_test.cpp).

namespace KWin::movestate
{

// --- multi-column source index ---------------------------------------------

inline int afterWindowRemoved(int moveSourceCol, int removedCol, bool columnEmptied, bool removedIsSourceWindow)
{
    if (moveSourceCol < 0) {
        return -1;
    }
    if (removedIsSourceWindow) {
        return -1;
    }
    if (removedCol < 0) {
        return moveSourceCol;
    }
    if (columnEmptied && removedCol == moveSourceCol) {
        return -1;
    }
    if (columnEmptied && moveSourceCol > removedCol) {
        return moveSourceCol - 1;
    }
    return moveSourceCol;
}

inline bool moveIsOpen(int moveSourceCol)
{
    return moveSourceCol >= 0;
}

/**
 * Whether removeWindow should run cancelMove (destroy empty source holder).
 * @p windowAlreadyUnmanaged / !windowFoundInLayout: KWin untile-for-drag left
 * an empty leaf that findWindow cannot see.
 */
inline bool shouldCancelMoveOnRemove(bool moveOpen, bool windowFoundInLayout, int removeCol, int sourceCol,
                                     bool windowAlreadyUnmanaged)
{
    if (!moveOpen) {
        return false;
    }
    if (windowAlreadyUnmanaged || !windowFoundInLayout) {
        return true;
    }
    return removeCol == sourceCol;
}

/** Classification for ScrollingLayoutEngine::removeWindow branch selection. */
enum class RemovePath {
    NoMoveNormal,      // no open move → normal remove or prune
    CancelSource,      // cancel source leaf (maybe done after)
    SiblingOtherColumn, // keep move open, shift source index
};

inline RemovePath classifyRemove(bool moveOpen, bool windowFound, int removeCol, int sourceCol)
{
    if (!moveOpen) {
        return RemovePath::NoMoveNormal;
    }
    if (!windowFound || removeCol == sourceCol) {
        return RemovePath::CancelSource;
    }
    return RemovePath::SiblingOtherColumn;
}

// --- source-leaf destroy decision (shared by StackColumn + pure leaf model) --

/**
 * Canonical rule for cancelMove: destroy the source leaf when it is empty
 * (untile-for-drag ghost) or still holds the dragged window.
 */
inline bool shouldDestroySourceLeaf(bool leafEmpty, bool leafHoldsDragged)
{
    return leafEmpty || leafHoldsDragged;
}

// --- pure leaf-list model (tests + algorithm twin of StackColumn) ------------

struct LeafSlot {
    int id = -1;
    bool empty = false;
    bool holdsDragged = false;
};

/**
 * Pure cancelMove: if source leaf is in @p leaves and shouldDestroySourceLeaf,
 * erase it. Always clears @p sourceLeafId. Returns true if a leaf was removed.
 */
inline bool cancelMoveLeaf(std::vector<LeafSlot> &leaves, int &sourceLeafId)
{
    if (sourceLeafId < 0) {
        return false;
    }
    const int src = sourceLeafId;
    sourceLeafId = -1;

    for (auto it = leaves.begin(); it != leaves.end(); ++it) {
        if (it->id != src) {
            continue;
        }
        if (shouldDestroySourceLeaf(it->empty, it->holdsDragged)) {
            leaves.erase(it);
            return true;
        }
        return false;
    }
    return false;
}

/** Anti-pattern under test: only clear pointer → empty leaf remains (phantom). */
inline void clearMoveLeafOnly(int &sourceLeafId)
{
    sourceLeafId = -1;
}

inline bool hasEmptyLeaf(const std::vector<LeafSlot> &leaves)
{
    for (const LeafSlot &l : leaves) {
        if (l.empty) {
            return true;
        }
    }
    return false;
}

} // namespace KWin::movestate
