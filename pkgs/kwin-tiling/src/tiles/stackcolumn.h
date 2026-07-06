/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "columnmath.h"
#include "core/rect.h"
#include "customtile.h"
#include "tile.h"
#include "window.h"

#include <QHash>
#include <QList>
#include <QPointer>

#include <algorithm>
#include <vector>

namespace KWin
{

class RootTile;

/**
 * One ordered, weighted vertical stack of windows — the unit every tiling
 * layout is built from, factored out so the engines stop re-implementing it.
 *
 * A StackColumn owns a list of single-window leaf tiles (children of a shared
 * RootTile) plus their per-window height weights, and knows how to fill a
 * rectangle with them. Engines compose it differently:
 *   - Stacked    : one StackColumn filling the whole root.
 *   - MasterStack: one StackColumn rendered as two index ranges (master/stack)
 *                  via fillRange(); ratio/count live in the engine.
 *   - Scrolling  : many StackColumns laid side by side; leaves move between
 *                  them (consume/expel) via detachWindow()/attachLeaf().
 *
 * It deliberately never calls reflow() or emits signals — the owning engine
 * arranges all of its columns and emits layoutChanged() once. Monocle/zoom is
 * handled by LayoutEngine::reflowZoomed() across every engine's leaves.
 */
class StackColumn
{
public:
    // A leaf detached from one column for re-insertion into another (Scrolling
    // consume/expel), carrying its height weight so it survives the move.
    struct Detached
    {
        QPointer<CustomTile> leaf;
        qreal weight = 1.0;
        bool isValid() const
        {
            return !leaf.isNull();
        }
    };

    void setRoot(RootTile *root)
    {
        m_root = root;
    }

    bool isEmpty() const
    {
        return m_leaves.isEmpty();
    }

    int count() const
    {
        return m_leaves.count();
    }

    bool contains(Window *window) const
    {
        return indexOf(window) >= 0;
    }

    int indexOf(Window *window) const
    {
        for (int i = 0; i < m_leaves.count(); ++i) {
            if (m_leaves[i] && m_leaves[i]->windows().contains(window)) {
                return i;
            }
        }
        return -1;
    }

    Window *windowAt(int index) const
    {
        if (index < 0 || index >= m_leaves.count() || !m_leaves[index]) {
            return nullptr;
        }
        const QList<Window *> ws = m_leaves[index]->windows();
        return ws.isEmpty() ? nullptr : ws.first();
    }

    QList<Window *> windows() const
    {
        QList<Window *> result;
        result.reserve(m_leaves.count());
        for (const auto &leaf : m_leaves) {
            if (leaf && !leaf->windows().isEmpty()) {
                result.append(leaf->windows().first());
            }
        }
        return result;
    }

    QList<CustomTile *> leaves() const
    {
        QList<CustomTile *> result;
        result.reserve(m_leaves.count());
        for (const auto &leaf : m_leaves) {
            if (leaf) {
                result.append(leaf);
            }
        }
        return result;
    }

    // Create a single-window leaf and insert it at `at` in this column's order
    // (append when `at` is out of range). Returns the leaf, or nullptr if KWin
    // refused to manage the window in the tile.
    CustomTile *insertWindow(Window *window, int at = -1)
    {
        if (!m_root || !window) {
            return nullptr;
        }
        // Always append as the last root child; logical order is tracked here in
        // m_leaves and geometry is set explicitly, so the child index among the
        // (floating) root's children is irrelevant.
        CustomTile *leaf = m_root->createChildAt(RectF(0, 0, 1, 1), Tile::LayoutDirection::Floating, m_root->childTiles().count());
        if (!leaf) {
            return nullptr;
        }
        if (!leaf->manage(window)) {
            qWarning() << "StackColumn: failed to manage window" << window->caption() << "in leaf, desktop mismatch?";
            m_root->destroyChild(leaf);
            return nullptr;
        }
        const int idx = (at < 0 || at > m_leaves.count()) ? m_leaves.count() : at;
        m_leaves.insert(idx, leaf);
        return leaf;
    }

    void removeWindow(Window *window)
    {
        const int idx = indexOf(window);
        if (idx < 0) {
            return;
        }
        // Never leave a removed window hidden (it may have been a scrolled-off
        // column); it is leaving this engine (closed / moved / layout switch).
        if (window) {
            window->setHidden(false);
        }
        CustomTile *leaf = m_leaves.takeAt(idx);
        if (leaf) {
            leaf->unmanage(window);
            if (m_root) {
                m_root->destroyChild(leaf);
            }
        }
        m_weights.remove(window);
    }

    // Reorder within the column: swap the window with the one `delta` steps away
    // (clamped). Matches the engines' original move semantics.
    void swapByDelta(Window *window, int delta)
    {
        const int idx = indexOf(window);
        if (idx < 0) {
            return;
        }
        const int newIdx = std::clamp(idx + delta, 0, int(m_leaves.count()) - 1);
        if (newIdx != idx) {
            m_leaves.swapItemsAt(idx, newIdx);
        }
    }

    // --- interactive drag move/swap (used by Stacked & MasterStack) ----------

    void beginMove(Window *window)
    {
        const int idx = indexOf(window);
        if (idx >= 0 && idx < m_leaves.count()) {
            m_moveSourceLeaf = m_leaves[idx];
        }
    }

    // Swap the dragged window with `target` (each keeps the other's leaf/slot),
    // or restore it to its source leaf when there is no target. Returns true if
    // a move was in progress (engine should reflow).
    bool endMove(Window *window, Window *target)
    {
        QPointer<CustomTile> sourceLeaf = m_moveSourceLeaf;
        m_moveSourceLeaf.clear();
        if (!sourceLeaf) {
            return false;
        }
        if (target && target != window) {
            const int targetIdx = indexOf(target);
            if (targetIdx >= 0 && m_leaves[targetIdx]) {
                CustomTile *targetLeaf = m_leaves[targetIdx];
                targetLeaf->unmanage(target);
                targetLeaf->manage(window);
                sourceLeaf->manage(target);
                return true;
            }
        }
        if (!sourceLeaf->windows().contains(window)) {
            sourceLeaf->manage(window);
        }
        return true;
    }

    // The dragged window left this column/output: drop the (possibly emptied)
    // source leaf so no phantom tile is left behind. Returns true if changed.
    bool cancelMove(Window *window)
    {
        QPointer<CustomTile> sourceLeaf = m_moveSourceLeaf;
        m_moveSourceLeaf.clear();
        if (!sourceLeaf || !m_leaves.contains(sourceLeaf)) {
            return false;
        }
        if (sourceLeaf->windows().isEmpty() || sourceLeaf->windows().contains(window)) {
            if (sourceLeaf->windows().contains(window)) {
                sourceLeaf->unmanage(window);
            }
            m_leaves.removeOne(sourceLeaf);
            if (m_root) {
                m_root->destroyChild(sourceLeaf);
            }
            return true;
        }
        return false;
    }

    // --- height weights ------------------------------------------------------

    qreal weight(int index) const
    {
        Window *w = windowAt(index);
        return w ? m_weights.value(w, 1.0) : 1.0;
    }

    // Keyboard height resize: grow/shrink a window's share of its column.
    void bumpWeight(Window *window, qreal delta)
    {
        if (!window) {
            return;
        }
        const qreal current = m_weights.value(window, 1.0);
        m_weights[window] = columnmath::clampWeight(current * (1.0 + delta));
    }

    // Mouse height resize: the window was dragged to occupy `heightFraction` of
    // its column; derive the weight that reproduces it, relative to the other
    // leaves in the sub-range [first, last) (the whole column for single-column
    // layouts; the master or stack run for MasterStack).
    void applyHeightDrag(Window *window, qreal heightFraction, int first, int last)
    {
        const int idx = indexOf(window);
        if (idx < 0) {
            return;
        }
        first = std::max(0, first);
        last = std::min(last, int(m_leaves.count()));
        qreal otherSum = 0.0;
        for (int i = first; i < last; ++i) {
            if (i != idx) {
                otherSum += weight(i);
            }
        }
        m_weights[window] = columnmath::clampWeight(columnmath::weightForFraction(heightFraction, otherSum));
    }

    void clearWeights()
    {
        m_weights.clear();
    }

    // --- geometry ------------------------------------------------------------

    // Lay leaves [first, last) vertically inside `area` (root-relative 0..1),
    // splitting the height by weights. When `offscreen` is true the windows are
    // hidden (a scrolled-off viewport column) except `keepVisible`; otherwise
    // they are shown (which also clears any hidden state a prior monocle left).
    void fillRange(int first, int last, const RectF &area, bool offscreen = false, Window *keepVisible = nullptr)
    {
        first = std::max(0, first);
        last = std::min(last, int(m_leaves.count()));
        if (first >= last) {
            return;
        }
        std::vector<double> weights;
        weights.reserve(last - first);
        for (int i = first; i < last; ++i) {
            weights.push_back(weight(i));
        }
        const auto dist = columnmath::distribute(weights);
        for (int k = 0; k < int(dist.size()); ++k) {
            CustomTile *leaf = m_leaves[first + k];
            if (!leaf) {
                continue;
            }
            const auto [yOffset, heightFraction] = dist[k];
            leaf->setRelativeGeometry(RectF(area.x(),
                                            area.y() + yOffset * area.height(),
                                            area.width(),
                                            heightFraction * area.height()));
            const QList<Window *> ws = leaf->windows();
            if (!ws.isEmpty() && ws.first()) {
                ws.first()->setHidden(offscreen && ws.first() != keepVisible);
            }
        }
    }

    void fill(const RectF &area, bool offscreen = false, Window *keepVisible = nullptr)
    {
        fillRange(0, m_leaves.count(), area, offscreen, keepVisible);
    }

    void unhideAll()
    {
        for (const auto &leaf : m_leaves) {
            if (!leaf) {
                continue;
            }
            for (Window *w : leaf->windows()) {
                if (w) {
                    w->setHidden(false);
                }
            }
        }
    }

    // Drop empty/destroyed leaves. Returns true if anything was removed.
    bool pruneEmpty()
    {
        bool changed = false;
        for (int i = m_leaves.count() - 1; i >= 0; --i) {
            CustomTile *leaf = m_leaves[i];
            if (!leaf || leaf->windows().isEmpty()) {
                if (leaf && m_root) {
                    m_root->destroyChild(leaf);
                }
                m_leaves.removeAt(i);
                changed = true;
            }
        }
        return changed;
    }

    // Vertical neighbour within this column (nullptr at the ends).
    Window *vertical(Window *from, bool down) const
    {
        const int idx = indexOf(from);
        if (idx < 0) {
            return nullptr;
        }
        return windowAt(idx + (down ? 1 : -1));
    }

    // --- cross-column moves (Scrolling consume/expel) ------------------------

    // Detach the window's leaf without destroying it, carrying its weight so it
    // can be re-inserted into another column unchanged.
    Detached detachWindow(Window *window)
    {
        Detached detached;
        const int idx = indexOf(window);
        if (idx < 0) {
            return detached;
        }
        detached.leaf = m_leaves.takeAt(idx);
        detached.weight = window ? m_weights.value(window, 1.0) : 1.0;
        m_weights.remove(window);
        return detached;
    }

    void attachLeaf(const Detached &detached, int at = -1)
    {
        if (!detached.leaf) {
            return;
        }
        const int idx = (at < 0 || at > m_leaves.count()) ? m_leaves.count() : at;
        m_leaves.insert(idx, detached.leaf);
        const QList<Window *> ws = detached.leaf->windows();
        if (!ws.isEmpty() && ws.first()) {
            m_weights[ws.first()] = detached.weight;
        }
    }

private:
    QPointer<RootTile> m_root;
    QList<QPointer<CustomTile>> m_leaves;
    QHash<Window *, qreal> m_weights;
    // Source leaf remembered during an interactive drag so the window can be
    // swapped/restored on release.
    QPointer<CustomTile> m_moveSourceLeaf;
};

} // namespace KWin
