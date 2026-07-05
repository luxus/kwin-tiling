/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scrollinglayoutengine.h"
#include "customtile.h"
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
    if (!m_root) {
        return;
    }

    // Take full ownership of the root tile (same as the other engines).
    // Scrolling stays within its own output: tiles clamp to [0, 1] like every
    // other layout, and columns scrolled out of the viewport are *hidden* (see
    // reflow()) rather than positioned past the screen edge.
    const QList<Tile *> existingChildren = m_root->childTiles();
    for (Tile *child : existingChildren) {
        if (CustomTile *custom = qobject_cast<CustomTile *>(child)) {
            m_root->destroyChild(custom);
        }
    }
    m_root->setLayoutDirection(Tile::LayoutDirection::Floating);
    m_root->setRelativeGeometry(RectF(0, 0, 1, 1));
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
    if (!findWindow(window, &c, &l)) {
        return;
    }

    m_columns[c].stack.removeWindow(window); // unhides, unmanages, destroys leaf
    if (m_columns[c].stack.isEmpty()) {
        m_columns.removeAt(c);
    }
    if (m_activeWindow == window) {
        m_activeWindow = nullptr;
    }

    reflow();
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

Window *ScrollingLayoutEngine::primaryWindow() const
{
    const QList<Window *> ws = windows();
    return ws.isEmpty() ? nullptr : ws.first();
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

bool ScrollingLayoutEngine::endResizeWindow(Window *window, const RectF &area)
{
    if (!window || (area.width() <= 0 && area.height() <= 0)) {
        return false;
    }
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
    if (area.height() > 0) {
        const qreal newHeight = geom.height();
        if (newHeight > 0) {
            m_columns[c].stack.applyHeightDrag(window, newHeight / area.height(), 0, n);
        }
    }

    if (area.width() > 0 && geom.width() > 0) {
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
