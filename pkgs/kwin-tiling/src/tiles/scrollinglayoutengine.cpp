/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scrollinglayoutengine.h"
#include "customtile.h"
#include "movestate.h"
#include "window.h"

#include <algorithm>

namespace KWin
{

ScrollingLayoutEngine::ScrollingLayoutEngine(QObject *parent)
    : LayoutEngine(parent)
{
}

ScrollingLayoutEngine::~ScrollingLayoutEngine()
{
    // Un-hide any columns scrolled off-screen so windows handed to another
    // engine (e.g. on a layout switch) are never left invisible.
    for (Column &col : m_columns) {
        col.stack.unhideAll();
    }
}

void ScrollingLayoutEngine::attach(RootTile *root)
{
    m_root = root;
    takeOwnershipOfRoot(m_root);
}

void ScrollingLayoutEngine::addWindow(Window *window)
{
    if (!m_root) {
        return;
    }

    // New window opens a new column immediately to the right of the active one
    // (niri's default), so existing windows never shrink.
    int colIdx = m_columns.isEmpty() ? 0 : activeColumnIndex() + 1;
    colIdx = std::clamp(colIdx, 0, int(m_columns.count()));

    Column col;
    col.width = m_defaultColWidth;
    col.stack.setRoot(m_root);
    if (!col.stack.insertWindow(window)) {
        return; // KWin refused to manage it; StackColumn already cleaned up
    }
    m_columns.insert(colIdx, col);
    m_activeWindow = window;

    reflow();
}

void ScrollingLayoutEngine::removeWindow(Window *window)
{
    int c = -1;
    int l = -1;
    const bool found = findWindow(window, &c, &l);
    const int prevMoveCol = m_moveHasSource ? m_moveSourceColumn : -1;
    const auto path = movestate::classifyRemove(m_moveHasSource, found, c, prevMoveCol);

    auto clearActiveIf = [this, window]() {
        if (m_activeWindow == window) {
            m_activeWindow = nullptr;
        }
    };

    if (path == movestate::RemovePath::CancelSource) {
        // Destroy empty source holder (untile-for-drag or drag window leaving).
        if (prevMoveCol >= 0 && prevMoveCol < m_columns.count()) {
            m_columns[prevMoveCol].stack.cancelMove(window);
            if (m_columns[prevMoveCol].stack.isEmpty()) {
                m_columns.removeAt(prevMoveCol);
            }
        }
        m_moveHasSource = false;
        m_moveSourceColumn = -1;
        if (!findWindow(window, &c, &l)) {
            clearActiveIf();
            pruneEmpty();
            reflow();
            return;
        }
        // Fall through to normal remove if the window is still managed elsewhere.
    } else if (path == movestate::RemovePath::SiblingOtherColumn) {
        m_columns[c].stack.removeWindow(window);
        const bool columnEmptied = m_columns[c].stack.isEmpty();
        if (columnEmptied) {
            m_columns.removeAt(c);
        }
        m_moveSourceColumn = movestate::afterWindowRemoved(prevMoveCol, c, columnEmptied, false);
        if (m_moveSourceColumn < 0) {
            m_moveHasSource = false;
        }
        clearActiveIf();
        reflow();
        return;
    }

    if (!findWindow(window, &c, &l)) {
        clearActiveIf();
        pruneEmpty();
        reflow();
        return;
    }

    m_columns[c].stack.removeWindow(window);
    if (m_columns[c].stack.isEmpty()) {
        m_columns.removeAt(c);
    }
    clearActiveIf();
    reflow();
}

void ScrollingLayoutEngine::beginMoveWindow(Window *window)
{
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l)) {
        return;
    }
    m_columns[c].stack.beginMove(window);
    m_moveHasSource = true;
    m_moveSourceColumn = c;
}

bool ScrollingLayoutEngine::endMoveWindow(Window *window, Window *target)
{
    if (!m_moveHasSource || m_moveSourceColumn < 0 || m_moveSourceColumn >= m_columns.count()) {
        return false;
    }
    m_moveHasSource = false;
    const int sourceCol = m_moveSourceColumn;
    m_moveSourceColumn = -1;
    StackColumn &source = m_columns[sourceCol].stack;

    if (target && target != window) {
        int targetCol = -1;
        int targetLeaf = -1;
        if (!findWindow(target, &targetCol, &targetLeaf)) {
            const bool handled = source.endMove(window, nullptr);
            if (handled) {
                reflow();
            }
            return handled;
        }

        if (targetCol == sourceCol) {
            const bool handled = source.endMove(window, target);
            if (handled) {
                reflow();
            }
            return handled;
        }

        const int srcIdx = source.indexOf(window);
        const int tgtIdx = m_columns[targetCol].stack.indexOf(target);
        if (srcIdx < 0 || tgtIdx < 0) {
            const bool handled = source.endMove(window, nullptr);
            if (handled) {
                reflow();
            }
            return handled;
        }

        StackColumn::Detached detachedWindow = source.detachWindow(window);
        StackColumn::Detached detachedTarget = m_columns[targetCol].stack.detachWindow(target);
        source.attachLeaf(detachedTarget, srcIdx);
        m_columns[targetCol].stack.attachLeaf(detachedWindow, tgtIdx);
        reflow();
        return true;
    }

    const bool handled = source.endMove(window, nullptr);
    if (handled) {
        reflow();
    }
    return handled;
}

void ScrollingLayoutEngine::cancelMoveWindow(Window *window)
{
    if (!m_moveHasSource || m_moveSourceColumn < 0 || m_moveSourceColumn >= m_columns.count()) {
        return;
    }
    m_moveHasSource = false;
    const int sourceCol = m_moveSourceColumn;
    m_moveSourceColumn = -1;

    if (m_columns[sourceCol].stack.cancelMove(window)) {
        if (m_columns[sourceCol].stack.isEmpty()) {
            m_columns.removeAt(sourceCol);
        }
        reflow();
    }
}

void ScrollingLayoutEngine::moveWindow(Window *window, int delta)
{
    // Move the window's whole column one step along the strip.
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l) || delta == 0) {
        return;
    }
    const int target = std::clamp(c + (delta > 0 ? 1 : -1), 0, int(m_columns.count()) - 1);
    if (target == c) {
        return;
    }
    m_columns.move(c, target);
    reflow();
}

void ScrollingLayoutEngine::pruneEmpty()
{
    bool changed = false;
    for (int c = m_columns.count() - 1; c >= 0; --c) {
        if (m_columns[c].stack.pruneEmpty()) {
            changed = true;
        }
        if (m_columns[c].stack.isEmpty()) {
            m_columns.removeAt(c);
            changed = true;
        }
    }
    if (changed) {
        reflow();
    }
}

void ScrollingLayoutEngine::reflow()
{
    if (m_columns.isEmpty()) {
        return;
    }

    // Monocle: the zoomed window fills the viewport and the rest are hidden.
    if (reflowZoomed(allLeaves())) {
        return;
    }

    scrollActiveIntoView();

    qreal x = 0.0;
    for (int c = 0; c < m_columns.count(); ++c) {
        Column &col = m_columns[c];
        const qreal colX = x - m_scrollOffset;
        // A column that does not overlap the [0, 1] viewport is scrolled fully
        // off-screen: hide it instead of letting its tile clamp to the screen
        // edge or spill onto the neighbouring monitor. The active window is
        // never hidden (it is always scrolled into view).
        const bool offscreen = (colX >= 1.0) || (colX + col.width <= 0.0);
        col.stack.fill(RectF(colX, 0.0, col.width, 1.0), offscreen, m_activeWindow);
        x += col.width;
    }

    Q_EMIT layoutChanged();
}

void ScrollingLayoutEngine::scrollActiveIntoView()
{
    qreal total = 0.0;
    for (const Column &col : m_columns) {
        total += col.width;
    }

    // Whole strip fits the viewport: center it (no scrolling needed).
    if (total <= 1.0) {
        m_scrollOffset = (total - 1.0) / 2.0;
        return;
    }

    // Bring the active column's [left, right) just into the [scroll, scroll+1)
    // viewport (PaperWM/niri "don't move unless off-screen" behaviour).
    const int ac = activeColumnIndex();
    qreal left = 0.0;
    for (int i = 0; i < ac; ++i) {
        left += m_columns[i].width;
    }
    const qreal right = left + m_columns[ac].width;

    if (left < m_scrollOffset) {
        m_scrollOffset = left;
    } else if (right > m_scrollOffset + 1.0) {
        m_scrollOffset = right - 1.0;
    }
    m_scrollOffset = std::clamp(m_scrollOffset, 0.0, total - 1.0);
}

QList<CustomTile *> ScrollingLayoutEngine::allLeaves() const
{
    QList<CustomTile *> result;
    for (const Column &col : m_columns) {
        result += col.stack.leaves();
    }
    return result;
}

QList<Window *> ScrollingLayoutEngine::windows() const
{
    QList<Window *> result;
    for (const Column &col : m_columns) {
        result += col.stack.windows();
    }
    return result;
}

Window *ScrollingLayoutEngine::windowInDirection(Window *from, FocusDirection direction) const
{
    if (m_columns.isEmpty()) {
        return nullptr;
    }

    int c = -1;
    int l = -1;
    if (!from || !findWindow(from, &c, &l)) {
        return m_columns.first().stack.windowAt(0);
    }

    switch (direction) {
    case FocusDirection::Left:
        return c > 0 ? m_columns[c - 1].stack.windowAt(std::min(l, m_columns[c - 1].stack.count() - 1)) : nullptr;
    case FocusDirection::Right:
        return c < m_columns.count() - 1 ? m_columns[c + 1].stack.windowAt(std::min(l, m_columns[c + 1].stack.count() - 1)) : nullptr;
    case FocusDirection::Up:
        return m_columns[c].stack.windowAt(l - 1);
    case FocusDirection::Down:
        return m_columns[c].stack.windowAt(l + 1);
    }
    return nullptr;
}

void ScrollingLayoutEngine::setActiveWindow(Window *window)
{
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l)) {
        return; // not managed by this engine
    }
    if (m_activeWindow == window) {
        return;
    }
    m_activeWindow = window;
    reflow(); // scroll the newly active column into view
}

void ScrollingLayoutEngine::setPrimarySplit(qreal ratio)
{
    ratio = std::clamp(ratio, 0.1, 1.0);
    m_defaultColWidth = ratio;
    const int ac = activeColumnIndex();
    if (ac < 0 || ac >= m_columns.count()) {
        return;
    }
    if (qFuzzyCompare(m_columns[ac].width, ratio)) {
        return;
    }
    m_columns[ac].width = ratio;
    reflow();
}

qreal ScrollingLayoutEngine::primarySplit() const
{
    const int ac = activeColumnIndex();
    if (ac < 0 || ac >= m_columns.count()) {
        return m_defaultColWidth;
    }
    return m_columns[ac].width;
}

void ScrollingLayoutEngine::setDefaultColumnWidth(qreal width)
{
    // Only affects columns opened from now on; existing columns keep their
    // width so a reconfigure never resets a layout the user arranged.
    m_defaultColWidth = std::clamp(width, 0.1, 1.0);
}

void ScrollingLayoutEngine::resetSizes()
{
    for (Column &col : m_columns) {
        col.width = m_defaultColWidth;
        col.stack.clearWeights();
    }
    reflow();
}

void ScrollingLayoutEngine::centerActiveColumn()
{
    const int ac = activeColumnIndex();
    if (ac < 0 || ac >= m_columns.count()) {
        return;
    }
    qreal left = 0.0;
    for (int i = 0; i < ac; ++i) {
        left += m_columns[i].width;
    }
    // Centre the active column; reflow()'s scrollActiveIntoView leaves a fully
    // visible active column where it is, and clamps this to the valid range.
    m_scrollOffset = left - (1.0 - m_columns[ac].width) / 2.0;
    reflow();
}

void ScrollingLayoutEngine::cycleColumnWidth()
{
    const int ac = activeColumnIndex();
    if (ac < 0 || ac >= m_columns.count()) {
        return;
    }
    // niri-style width presets; step to the first one wider than the current,
    // wrapping back to the narrowest once past full width.
    static constexpr qreal presets[] = {1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0};
    const qreal cur = m_columns[ac].width;
    qreal next = presets[0];
    for (const qreal p : presets) {
        if (p > cur + 0.01) {
            next = p;
            break;
        }
    }
    m_columns[ac].width = next;
    reflow();
}

void ScrollingLayoutEngine::consumeWindow()
{
    int c = -1;
    int l = -1;
    if (!m_activeWindow || !findWindow(m_activeWindow, &c, &l)) {
        return;
    }
    if (c <= 0) {
        return; // no column to the left to merge into
    }
    StackColumn::Detached detached = m_columns[c].stack.detachWindow(m_activeWindow);
    if (!detached.isValid()) {
        return;
    }
    const int leftIdx = c - 1; // unaffected by removing the (higher) source column
    if (m_columns[c].stack.isEmpty()) {
        m_columns.removeAt(c);
    }
    m_columns[leftIdx].stack.attachLeaf(detached); // append to the left column
    reflow();
}

void ScrollingLayoutEngine::expelWindow()
{
    int c = -1;
    int l = -1;
    if (!m_activeWindow || !findWindow(m_activeWindow, &c, &l)) {
        return;
    }
    if (m_columns[c].stack.count() <= 1) {
        return; // already alone in its column
    }
    StackColumn::Detached detached = m_columns[c].stack.detachWindow(m_activeWindow);
    if (!detached.isValid()) {
        return;
    }
    Column col;
    col.width = m_columns[c].width;
    col.stack.setRoot(m_root);
    col.stack.attachLeaf(detached);
    m_columns.insert(c + 1, col); // new column immediately to the right
    reflow();
}

void ScrollingLayoutEngine::adjustWindowHeight(Window *window, qreal delta)
{
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l) || m_columns[c].stack.count() < 2) {
        return;
    }
    m_columns[c].stack.bumpWeight(window, delta);
    reflow();
}

bool ScrollingLayoutEngine::applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged)
{
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l)) {
        return false;
    }

    const int n = m_columns[c].stack.count();
    if (n < 2) {
        reflow();
        return true;
    }

    const auto geom = window->frameGeometry();
    if (heightChanged && area.height() > 0) {
        const qreal newHeight = geom.height();
        if (newHeight > 0) {
            m_columns[c].stack.applyHeightDrag(window, newHeight / area.height(), 0, n);
        }
    }

    if (widthChanged && area.width() > 0 && geom.width() > 0) {
        m_columns[c].width = std::clamp(geom.width() / area.width(), 0.1, 1.0);
    }

    reflow();
    return true;
}

bool ScrollingLayoutEngine::findWindow(Window *window, int *colIdx, int *leafIdx) const
{
    for (int c = 0; c < m_columns.count(); ++c) {
        const int l = m_columns[c].stack.indexOf(window);
        if (l >= 0) {
            if (colIdx) {
                *colIdx = c;
            }
            if (leafIdx) {
                *leafIdx = l;
            }
            return true;
        }
    }
    return false;
}

int ScrollingLayoutEngine::activeColumnIndex() const
{
    if (m_activeWindow) {
        int c = -1;
        int l = -1;
        if (findWindow(m_activeWindow, &c, &l)) {
            return c;
        }
    }
    return 0;
}

} // namespace KWin
