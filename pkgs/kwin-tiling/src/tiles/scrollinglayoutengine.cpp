/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scrollinglayoutengine.h"
#include "columnwidthpresets.h"
#include "customtile.h"
#include "movestate.h"
#include "scrollingcolumn.h"
#include "scrollingmove.h"
#include "viewportmath.h"
#include "window.h"

#include <algorithm>
#include <vector>

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
    // Path A (#40): overflow geometry is legal on this tree. Leaves inherit
    // m_allowOverflow so CustomTile [0,1] and windowGeometry() output intersect
    // do not resize peeking columns. Hide-for-offscreen stays until #41.
    if (m_root) {
        m_root->setAllowOverflow(true);
    }
}

void ScrollingLayoutEngine::addWindow(Window *window)
{
    if (!m_root) {
        return;
    }

    // New window opens a new column immediately to the right of the active one
    // (niri's default), so existing windows never shrink.
    const int from = m_columns.isEmpty() ? -1 : activeColumnIndex();
    int colIdx = m_columns.isEmpty() ? 0 : from + 1;
    colIdx = std::clamp(colIdx, 0, int(m_columns.count()));

    Column col;
    col.width = m_defaultColWidth;
    col.stack.setRoot(m_root);
    if (!col.stack.insertWindow(window)) {
        return; // KWin refused to manage it; StackColumn already cleaned up
    }
    m_columns.insert(colIdx, col);
    m_activeWindow = window;
    m_focusFromColumn = from;

    reflow();
}

void ScrollingLayoutEngine::removeWindow(Window *window)
{
    int c = -1;
    int l = -1;
    const bool found = findWindow(window, &c, &l);
    const int prevMoveCol = m_moveHasSource ? m_moveSourceColumn : -1;
    const bool isMoveWindow = m_moveHasSource && prevMoveCol >= 0 && prevMoveCol < m_columns.count()
        && m_columns[prevMoveCol].stack.ownsGhostLeaf(window);
    if (!movestate::shouldHandleRemove(found, isMoveWindow)) {
        return;
    }
    const auto path = movestate::classifyRemove(m_moveHasSource, found, c, prevMoveCol, isMoveWindow);

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

bool ScrollingLayoutEngine::ownsGhostLeaf(Window *window) const
{
    if (!window || !m_moveHasSource || m_moveSourceColumn < 0 || m_moveSourceColumn >= m_columns.count()) {
        return false;
    }
    return m_columns[m_moveSourceColumn].stack.ownsGhostLeaf(window);
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
    // Meta+Alt Left/Right and Meta+Shift Left/Right: slide the whole column
    // (niri move-column-left/right). Consume/expel is a different action.
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l) || delta == 0) {
        return;
    }
    const int target = scrollingmove::stepIndex(c, delta, m_columns.count());
    if (target == c) {
        return;
    }
    m_columns.move(c, target);
    reflow();
}

void ScrollingLayoutEngine::moveWindowInColumn(Window *window, int delta)
{
    // Meta+Alt Up/Down: swap with the vertical neighbour (niri move-window-up/down).
    // Column index is unchanged.
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l) || delta == 0) {
        return;
    }
    const int target = scrollingmove::stepIndex(l, delta, m_columns[c].stack.count());
    if (target == l) {
        return;
    }
    m_columns[c].stack.swapByDelta(window, target - l);
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
    std::vector<double> widths;
    widths.reserve(static_cast<size_t>(m_columns.count()));
    for (const Column &col : m_columns) {
        widths.push_back(col.width);
    }
    const int ac = activeColumnIndex();
    m_scrollOffset = viewportmath::scrollOffsetForFocus(
        widths, ac, m_focusFromColumn, m_scrollOffset, m_centerMode);
    m_focusFromColumn = ac;
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

bool ScrollingLayoutEngine::contains(Window *window) const
{
    return findWindow(window, nullptr, nullptr);
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
    m_focusFromColumn = activeColumnIndex();
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

void ScrollingLayoutEngine::setCenterFocusedColumn(viewportmath::CenterFocusedColumn mode)
{
    if (m_centerMode == mode) {
        return;
    }
    m_centerMode = mode;
    reflow();
}

void ScrollingLayoutEngine::setColumnWidthPresets(const QList<qreal> &presets)
{
    if (presets.isEmpty()) {
        m_columnWidthPresets = {1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0};
        return;
    }
    m_columnWidthPresets = presets;
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
    // One-shot centre (Meta+Shift+C), independent of CenterFocusedColumn.
    // never/on-overflow fit-scroll in reflow() leaves a fully visible column
    // where it is; always re-centers to the same offset.
    m_scrollOffset = viewportmath::centerScrollOffset(left, m_columns[ac].width);
    reflow();
}

void ScrollingLayoutEngine::cycleColumnWidth()
{
    cycleColumnWidthBy(+1);
}

void ScrollingLayoutEngine::cycleColumnWidthReverse()
{
    cycleColumnWidthBy(-1);
}

void ScrollingLayoutEngine::cycleColumnWidthBy(int direction)
{
    const int ac = activeColumnIndex();
    if (ac < 0 || ac >= m_columns.count()) {
        return;
    }
    std::vector<double> presets;
    presets.reserve(size_t(m_columnWidthPresets.size()));
    for (const qreal p : m_columnWidthPresets) {
        presets.push_back(double(p));
    }
    const qreal next = qreal(columnwidthpresets::cycle(double(m_columns[ac].width), presets, direction));
    if (qFuzzyCompare(m_columns[ac].width, next)) {
        return;
    }
    m_columns[ac].width = next;
    reflow();
}

void ScrollingLayoutEngine::consumeWindow()
{
    consumeIntoColumn();
}

void ScrollingLayoutEngine::expelWindow()
{
    expelFromColumn();
}

void ScrollingLayoutEngine::consumeIntoColumn()
{
    // niri consume-window-into-column: first tile of the next column appends
    // to the focused column. Remaining column widths are not rewritten.
    const int ac = activeColumnIndex();
    const auto op = scrollingcolumn::planConsumeIntoColumn(ac, m_columns.count());
    if (!op.apply) {
        return;
    }
    Window *first = m_columns[op.sourceCol].stack.windowAt(op.sourceLeaf);
    if (!first) {
        return;
    }
    StackColumn::Detached detached = m_columns[op.sourceCol].stack.detachWindow(first);
    if (!detached.isValid()) {
        return;
    }
    if (m_columns[op.sourceCol].stack.isEmpty()) {
        m_columns.removeAt(op.sourceCol);
    }
    m_columns[op.destCol].stack.attachLeaf(detached);
    reflow();
}

void ScrollingLayoutEngine::expelFromColumn()
{
    // niri expel-window-from-column: last tile of the focused column becomes a
    // new column to the right, copying the source width (principle 1).
    const int ac = activeColumnIndex();
    if (ac < 0 || ac >= m_columns.count()) {
        return;
    }
    const auto op = scrollingcolumn::planExpelFromColumn(ac, m_columns.count(), m_columns[ac].stack.count());
    if (!op.apply) {
        return;
    }
    Window *bottom = m_columns[op.sourceCol].stack.windowAt(op.sourceLeaf);
    if (!bottom) {
        return;
    }
    StackColumn::Detached detached = m_columns[op.sourceCol].stack.detachWindow(bottom);
    if (!detached.isValid()) {
        return;
    }
    Column col;
    col.width = m_columns[op.sourceCol].width;
    col.stack.setRoot(m_root);
    col.stack.attachLeaf(detached);
    m_columns.insert(op.sourceCol + 1, col);
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
