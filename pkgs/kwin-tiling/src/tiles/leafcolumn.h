/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "movestate.h"

#include <vector>

// Pure single-column leaf bookkeeping mirroring StackColumn's move lifecycle
// without KWin/Qt. Destroy decisions use movestate::shouldDestroySourceLeaf —
// the same helper StackColumn::cancelMove calls. Tested in leafcolumn_test.cpp:
//   beginMove → empty source (untile-for-drag) → cancelMove → leaf destroyed.

namespace KWin::leafcolumn
{

struct Leaf {
    int id = -1;
    int windowId = -1; // -1 when empty (ghost holder after untile)
};

/**
 * Ordered column of single-window leaves + open interactive-move source.
 * destroyCount increments when a leaf is removed via cancel/remove/prune.
 */
class Column
{
public:
    int insertWindow(int windowId, int at = -1)
    {
        if (windowId < 0) {
            return -1;
        }
        const int leafId = m_nextLeafId++;
        Leaf leaf{leafId, windowId};
        const int idx = (at < 0 || at > int(m_leaves.size())) ? int(m_leaves.size()) : at;
        m_leaves.insert(m_leaves.begin() + idx, leaf);
        return leafId;
    }

    bool containsWindow(int windowId) const { return indexOfWindow(windowId) >= 0; }

    int indexOfWindow(int windowId) const
    {
        for (int i = 0; i < int(m_leaves.size()); ++i) {
            if (m_leaves[size_t(i)].windowId == windowId) {
                return i;
            }
        }
        return -1;
    }

    int count() const { return int(m_leaves.size()); }
    int destroyCount() const { return m_destroyCount; }

    bool hasEmptyLeaf() const
    {
        for (const Leaf &l : m_leaves) {
            if (l.windowId < 0) {
                return true;
            }
        }
        return false;
    }

    bool hasMoveSource() const { return m_moveSourceLeafId >= 0; }
    int moveSourceLeafId() const { return m_moveSourceLeafId; }
    int moveWindowId() const { return m_moveWindowId; }

    /**
     * True when this column's open move source is @p windowId's leaf — empty
     * after untile-for-drag, or still holding that window. A contains() check
     * misses the empty-ghost case.
     */
    bool ownsGhostLeaf(int windowId) const
    {
        if (windowId < 0 || m_moveSourceLeafId < 0) {
            return false;
        }
        const bool moveWindowMatches = (m_moveWindowId < 0 || m_moveWindowId == windowId);
        for (const Leaf &l : m_leaves) {
            if (l.id != m_moveSourceLeafId) {
                continue;
            }
            return movestate::ownsGhostLeaf(true, moveWindowMatches, l.windowId < 0, l.windowId == windowId);
        }
        return false;
    }

    bool shouldHandleRemove(int windowId) const
    {
        return movestate::shouldHandleRemove(containsWindow(windowId), ownsGhostLeaf(windowId));
    }

    void beginMove(int windowId)
    {
        const int idx = indexOfWindow(windowId);
        if (idx < 0) {
            return;
        }
        m_moveSourceLeafId = m_leaves[size_t(idx)].id;
        m_moveWindowId = windowId;
    }

    /**
     * Simulate KWin untile-for-drag: source leaf stays in the column but no
     * longer holds the window (empty ghost holder).
     */
    bool emptySourceForDrag()
    {
        if (m_moveSourceLeafId < 0) {
            return false;
        }
        for (Leaf &l : m_leaves) {
            if (l.id == m_moveSourceLeafId) {
                l.windowId = -1;
                return true;
            }
        }
        return false;
    }

    /**
     * Same destroy decision as StackColumn::cancelMove:
     * shouldDestroySourceLeaf(empty, holds window argument).
     */
    bool cancelMove(int windowId)
    {
        if (m_moveSourceLeafId < 0) {
            return false;
        }
        // Sibling / unrelated remove must not steal this window's drag ghost.
        if (m_moveWindowId >= 0 && windowId >= 0 && m_moveWindowId != windowId) {
            return false;
        }
        const int src = m_moveSourceLeafId;
        m_moveSourceLeafId = -1;
        m_moveWindowId = -1;

        for (auto it = m_leaves.begin(); it != m_leaves.end(); ++it) {
            if (it->id != src) {
                continue;
            }
            const bool leafEmpty = it->windowId < 0;
            const bool leafHoldsDragged = (windowId >= 0 && it->windowId == windowId);
            if (!movestate::shouldDestroySourceLeaf(leafEmpty, leafHoldsDragged)) {
                return false;
            }
            m_leaves.erase(it);
            ++m_destroyCount;
            return true;
        }
        // Source pointer stale (already gone from list).
        return false;
    }

    /** Anti-pattern: only clear move pointer — leaves empty ghost (phantom). */
    void clearMoveOnly()
    {
        m_moveSourceLeafId = -1;
    }

    bool removeWindow(int windowId)
    {
        const int idx = indexOfWindow(windowId);
        if (idx < 0) {
            return false;
        }
        if (m_leaves[size_t(idx)].id == m_moveSourceLeafId) {
            m_moveSourceLeafId = -1;
            m_moveWindowId = -1;
        }
        m_leaves.erase(m_leaves.begin() + idx);
        ++m_destroyCount;
        return true;
    }

    int pruneEmpty()
    {
        int n = 0;
        for (int i = int(m_leaves.size()) - 1; i >= 0; --i) {
            if (m_leaves[size_t(i)].windowId < 0) {
                if (m_leaves[size_t(i)].id == m_moveSourceLeafId) {
                    m_moveSourceLeafId = -1;
                }
                m_leaves.erase(m_leaves.begin() + i);
                ++m_destroyCount;
                ++n;
            }
        }
        return n;
    }

private:
    std::vector<Leaf> m_leaves;
    int m_nextLeafId = 1;
    int m_moveSourceLeafId = -1;
    int m_moveWindowId = -1;
    int m_destroyCount = 0;
};

} // namespace KWin::leafcolumn
