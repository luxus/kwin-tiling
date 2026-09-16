/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "columnslayoutengine.h"

#include "columnlayoutmath.h"
#include "customtile.h"
#include "insertpolicy.h"
#include "movestate.h"
#include "window.h"

#include <algorithm>
#include <vector>

namespace KWin
{

ColumnsLayoutEngine::ColumnsLayoutEngine(QObject *parent)
    : LayoutEngine(parent)
{
}

ColumnsLayoutEngine::~ColumnsLayoutEngine()
{
    for (Column &col : m_columns) {
        col.stack.unhideAll();
    }
}

void ColumnsLayoutEngine::attach(RootTile *root)
{
    m_root = root;
    takeOwnershipOfRoot(m_root);
}

void ColumnsLayoutEngine::addWindow(Window *window)
{
    if (!m_root || !window) {
        return;
    }

    std::vector<int> counts;
    counts.reserve(static_cast<size_t>(m_columns.count()));
    for (const Column &col : m_columns) {
        counts.push_back(col.stack.count());
    }
    const auto target = columnlayoutmath::addTarget(m_columns.count(), m_maxColumns,
                                                    focusedColumnOrNeg(),
                                                    columnlayoutmath::fewestColumn(counts));

    if (target.openNewColumn) {
        Column col;
        col.stack.setRoot(m_root);
        if (!col.stack.insertWindow(window)) {
            return;
        }
        const int at = std::clamp(target.column, 0, int(m_columns.count()));
        m_columns.insert(at, col);
        rebalanceWidths();
    } else {
        const int col = std::clamp(target.column, 0, int(m_columns.count()) - 1);
        if (col < 0 || !m_columns[col].stack.insertWindow(window)) {
            return;
        }
    }
    m_activeWindow = window;
    reflow();
}

void ColumnsLayoutEngine::removeWindow(Window *window)
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
        if (prevMoveCol >= 0 && prevMoveCol < m_columns.count()) {
            m_columns[prevMoveCol].stack.cancelMove(window);
            if (m_columns[prevMoveCol].stack.isEmpty()) {
                m_columns.removeAt(prevMoveCol);
                rebalanceWidths();
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
    } else if (path == movestate::RemovePath::SiblingOtherColumn) {
        m_columns[c].stack.removeWindow(window);
        const bool columnEmptied = m_columns[c].stack.isEmpty();
        if (columnEmptied) {
            m_columns.removeAt(c);
            rebalanceWidths();
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
        rebalanceWidths();
    }
    clearActiveIf();
    reflow();
}

bool ColumnsLayoutEngine::ownsGhostLeaf(Window *window) const
{
    if (!window || !m_moveHasSource || m_moveSourceColumn < 0 || m_moveSourceColumn >= m_columns.count()) {
        return false;
    }
    return m_columns[m_moveSourceColumn].stack.ownsGhostLeaf(window);
}

void ColumnsLayoutEngine::beginMoveWindow(Window *window)
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

bool ColumnsLayoutEngine::endMoveWindow(Window *window, Window *target)
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

bool ColumnsLayoutEngine::endMoveWindowOnZone(Window *window, Window *target, DropZone zone)
{
    if (zone == DropZone::Swap || !target || target == window) {
        return endMoveWindow(window, target);
    }
    if (!m_moveHasSource || m_moveSourceColumn < 0 || m_moveSourceColumn >= m_columns.count()) {
        return false;
    }

    int tgtCol = -1;
    int tgtRow = -1;
    if (!findWindow(target, &tgtCol, &tgtRow)) {
        return endMoveWindow(window, nullptr);
    }

    const int srcCol = m_moveSourceColumn;
    StackColumn &source = m_columns[srcCol].stack;
    int srcRow = source.indexOf(window);
    if (srcRow < 0) {
        srcRow = source.moveSourceIndex();
    }
    const int srcCount = source.count();
    const auto pos = insertpolicy::insertPos(srcCol, srcRow < 0 ? 0 : srcRow, srcCount, tgtCol, tgtRow, zone);

    source.cancelMove(window);
    m_moveHasSource = false;
    m_moveSourceColumn = -1;
    if (source.isEmpty()) {
        m_columns.removeAt(srcCol);
        rebalanceWidths();
    }

    if (pos.column < 0 || pos.column >= m_columns.count()) {
        addWindow(window);
        return true;
    }
    if (m_columns[pos.column].stack.insertWindow(window, pos.row)) {
        reflow();
        return true;
    }
    return false;
}

void ColumnsLayoutEngine::cancelMoveWindow(Window *window)
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
            rebalanceWidths();
        }
        reflow();
    }
}

void ColumnsLayoutEngine::moveWindow(Window *window, int delta)
{
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l) || delta == 0) {
        return;
    }
    const QList<Window *> ws = windows();
    const int flat = ws.indexOf(window);
    const int dest = std::clamp(flat + delta, 0, ws.count() - 1);
    if (dest == flat) {
        return;
    }
    Window *other = ws.at(dest);
    int oc = -1;
    int ol = -1;
    if (!findWindow(other, &oc, &ol)) {
        return;
    }
    if (c == oc) {
        m_columns[c].stack.swapAt(l, ol);
    } else {
        StackColumn::Detached detachedWindow = m_columns[c].stack.detachWindow(window);
        StackColumn::Detached detachedTarget = m_columns[oc].stack.detachWindow(other);
        m_columns[c].stack.attachLeaf(detachedTarget, l);
        m_columns[oc].stack.attachLeaf(detachedWindow, ol);
    }
    reflow();
}

void ColumnsLayoutEngine::reorderWindow(Window *window, int delta)
{
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l) || delta == 0) {
        return;
    }
    const int flat = flatIndex(c, l);
    const int total = windows().count();
    const int newFlat = std::clamp(flat + delta, 0, total - 1);
    if (newFlat == flat) {
        return;
    }
    StackColumn::Detached detached = m_columns[c].stack.detachWindow(window);
    if (!detached.isValid()) {
        return;
    }
    if (m_columns[c].stack.isEmpty()) {
        m_columns.removeAt(c);
        rebalanceWidths();
    }
    insertDetachedAtFlat(detached, newFlat);
    reflow();
}

void ColumnsLayoutEngine::dropWindow(Window *window, Window *target, const QPointF &pos, const RectF &area)
{
    if (!window) {
        return;
    }
    if (target && target != window) {
        int c = -1;
        int r = -1;
        if (findWindow(target, &c, &r)) {
            const auto geom = target->frameGeometry();
            const DropZone zone = insertpolicy::classifyPoint(pos.x(), pos.y(), geom.x(), geom.y(),
                                                              geom.width(), geom.height());
            const int at = (zone == DropZone::Swap) ? r : insertpolicy::insertRowAtTarget(r, zone);
            if (m_columns[c].stack.insertWindow(window, at)) {
                m_activeWindow = window;
                reflow();
            }
            return;
        }
    }

    if (m_columns.isEmpty()) {
        addWindow(window);
        return;
    }

    std::vector<double> widths;
    widths.reserve(static_cast<size_t>(m_columns.count()));
    for (const Column &col : m_columns) {
        widths.push_back(col.width);
    }
    const qreal relX = (area.width() > 0) ? (pos.x() - area.x()) / area.width() : 1.0;
    const qreal relY = (area.height() > 0) ? (pos.y() - area.y()) / area.height() : 1.0;
    const int col = std::clamp(columnlayoutmath::columnAtX(widths, relX), 0, int(m_columns.count()) - 1);
    const int at = (relY < 0.5) ? 0 : m_columns[col].stack.count();
    if (m_columns[col].stack.insertWindow(window, at)) {
        m_activeWindow = window;
        reflow();
    }
}

void ColumnsLayoutEngine::pruneEmpty()
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
        rebalanceWidths();
        reflow();
    }
}

void ColumnsLayoutEngine::reflow()
{
    if (m_columns.isEmpty()) {
        return;
    }
    if (reflowZoomed(allLeaves())) {
        return;
    }

    normalizeWidths();
    qreal x = 0.0;
    for (Column &col : m_columns) {
        col.stack.fill(RectF(x, 0.0, col.width, 1.0));
        x += col.width;
    }
    Q_EMIT layoutChanged();
}

QList<CustomTile *> ColumnsLayoutEngine::allLeaves() const
{
    QList<CustomTile *> result;
    for (const Column &col : m_columns) {
        result += col.stack.leaves();
    }
    return result;
}

QList<Window *> ColumnsLayoutEngine::windows() const
{
    QList<Window *> result;
    for (const Column &col : m_columns) {
        result += col.stack.windows();
    }
    return result;
}

Window *ColumnsLayoutEngine::windowInDirection(Window *from, FocusDirection direction) const
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

void ColumnsLayoutEngine::setActiveWindow(Window *window)
{
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l)) {
        return;
    }
    m_activeWindow = window;
}

void ColumnsLayoutEngine::setPrimarySplit(qreal ratio)
{
    const int ac = activeColumnIndex();
    if (ac < 0 || m_columns.count() < 2) {
        return;
    }
    ratio = std::clamp(ratio, columnlayoutmath::kMinColWidth, columnlayoutmath::kMaxColWidth);
    const qreal delta = ratio - m_columns[ac].width;
    bool ok = false;
    if (ac < m_columns.count() - 1) {
        ok = columnlayoutmath::transferWidth(m_columns[ac].width, m_columns[ac + 1].width, delta);
    } else {
        ok = columnlayoutmath::transferWidth(m_columns[ac - 1].width, m_columns[ac].width, -delta);
    }
    if (ok) {
        normalizeWidths();
        reflow();
    }
}

qreal ColumnsLayoutEngine::primarySplit() const
{
    const int ac = activeColumnIndex();
    if (ac < 0 || ac >= m_columns.count()) {
        return m_columns.isEmpty() ? 0.5 : m_columns.first().width;
    }
    return m_columns[ac].width;
}

void ColumnsLayoutEngine::setMaxColumns(int maxColumns)
{
    m_maxColumns = columnlayoutmath::clampMaxColumns(maxColumns);
}

void ColumnsLayoutEngine::resetSizes()
{
    for (Column &col : m_columns) {
        col.stack.clearWeights();
    }
    rebalanceWidths();
    reflow();
}

void ColumnsLayoutEngine::consumeWindow()
{
    int c = -1;
    int l = -1;
    if (!m_activeWindow || !findWindow(m_activeWindow, &c, &l)) {
        return;
    }
    int target = columnlayoutmath::consumeTargetColumn(c, m_columns.count());
    if (target < 0) {
        return;
    }
    StackColumn::Detached detached = m_columns[c].stack.detachWindow(m_activeWindow);
    if (!detached.isValid()) {
        return;
    }
    if (m_columns[c].stack.isEmpty()) {
        m_columns.removeAt(c);
        if (c < target) {
            --target;
        }
        rebalanceWidths();
    }
    m_columns[target].stack.attachLeaf(detached);
    reflow();
}

void ColumnsLayoutEngine::expelWindow()
{
    int c = -1;
    int l = -1;
    if (!m_activeWindow || !findWindow(m_activeWindow, &c, &l)) {
        return;
    }
    const int at = columnlayoutmath::expelInsertAt(c, m_columns.count(), m_maxColumns, m_columns[c].stack.count());
    if (at < 0) {
        return;
    }
    StackColumn::Detached detached = m_columns[c].stack.detachWindow(m_activeWindow);
    if (!detached.isValid()) {
        return;
    }
    Column col;
    col.stack.setRoot(m_root);
    col.stack.attachLeaf(detached);
    m_columns.insert(at, col);
    rebalanceWidths();
    reflow();
}

void ColumnsLayoutEngine::adjustWindowHeight(Window *window, qreal delta)
{
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l) || m_columns[c].stack.count() < 2) {
        return;
    }
    m_columns[c].stack.bumpWeight(window, delta);
    reflow();
}

bool ColumnsLayoutEngine::applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged)
{
    int c = -1;
    int l = -1;
    if (!findWindow(window, &c, &l)) {
        return false;
    }

    const int n = m_columns[c].stack.count();
    const auto geom = window->frameGeometry();
    if (heightChanged && n >= 2 && area.height() > 0 && geom.height() > 0) {
        m_columns[c].stack.applyHeightDrag(window, geom.height() / area.height(), 0, n);
    }

    if (widthChanged && area.width() > 0 && geom.width() > 0 && m_columns.count() > 1) {
        const qreal newW = std::clamp(geom.width() / area.width(),
                                      columnlayoutmath::kMinColWidth, columnlayoutmath::kMaxColWidth);
        const qreal delta = newW - m_columns[c].width;
        if (c < m_columns.count() - 1) {
            columnlayoutmath::transferWidth(m_columns[c].width, m_columns[c + 1].width, delta);
        } else {
            columnlayoutmath::transferWidth(m_columns[c - 1].width, m_columns[c].width, -delta);
        }
        normalizeWidths();
    }

    reflow();
    return true;
}

bool ColumnsLayoutEngine::findWindow(Window *window, int *colIdx, int *leafIdx) const
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

int ColumnsLayoutEngine::focusedColumnOrNeg() const
{
    if (!m_activeWindow) {
        return -1;
    }
    int c = -1;
    int l = -1;
    return findWindow(m_activeWindow, &c, &l) ? c : -1;
}

int ColumnsLayoutEngine::activeColumnIndex() const
{
    const int focused = focusedColumnOrNeg();
    return focused >= 0 ? focused : 0;
}

int ColumnsLayoutEngine::flatIndex(int col, int row) const
{
    int flat = row;
    for (int i = 0; i < col; ++i) {
        flat += m_columns[i].stack.count();
    }
    return flat;
}

void ColumnsLayoutEngine::insertDetachedAtFlat(const StackColumn::Detached &detached, int flatIdx)
{
    if (!detached.isValid()) {
        return;
    }
    if (m_columns.isEmpty()) {
        Column col;
        col.stack.setRoot(m_root);
        col.stack.attachLeaf(detached);
        m_columns.append(col);
        rebalanceWidths();
        return;
    }
    int running = 0;
    for (int c = 0; c < m_columns.count(); ++c) {
        const int n = m_columns[c].stack.count();
        if (flatIdx <= running + n) {
            m_columns[c].stack.attachLeaf(detached, flatIdx - running);
            return;
        }
        running += n;
    }
    m_columns.last().stack.attachLeaf(detached);
}

void ColumnsLayoutEngine::rebalanceWidths()
{
    const std::vector<double> w = columnlayoutmath::equalWidths(m_columns.count());
    for (int i = 0; i < m_columns.count(); ++i) {
        m_columns[i].width = w[static_cast<size_t>(i)];
    }
}

void ColumnsLayoutEngine::normalizeWidths()
{
    std::vector<double> w;
    w.reserve(static_cast<size_t>(m_columns.count()));
    for (const Column &col : m_columns) {
        w.push_back(col.width);
    }
    w = columnlayoutmath::normalizeWidths(w);
    for (int i = 0; i < m_columns.count(); ++i) {
        m_columns[i].width = w[static_cast<size_t>(i)];
    }
}

} // namespace KWin
