/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gridlayoutengine.h"
#include "customtile.h"
#include "gridmath.h"
#include "window.h"

namespace KWin
{

GridLayoutEngine::GridLayoutEngine(QObject *parent)
    : LayoutEngine(parent)
{
}

GridLayoutEngine::~GridLayoutEngine()
{
    m_column.unhideAll();
}

void GridLayoutEngine::attach(RootTile *root)
{
    takeOwnershipOfRoot(root);
    m_column.setRoot(root);
}

void GridLayoutEngine::addWindow(Window *window)
{
    if (m_column.insertWindow(window)) {
        reflow();
    }
}

void GridLayoutEngine::removeWindow(Window *window)
{
    m_column.removeWindow(window);
    reflow();
}

void GridLayoutEngine::moveWindow(Window *window, int delta)
{
    m_column.swapByDelta(window, delta);
    reflow();
}

void GridLayoutEngine::beginMoveWindow(Window *window)
{
    m_column.beginMove(window);
}

bool GridLayoutEngine::endMoveWindow(Window *window, Window *target)
{
    const bool handled = m_column.endMove(window, target);
    if (handled) {
        reflow();
    }
    return handled;
}

void GridLayoutEngine::cancelMoveWindow(Window *window)
{
    if (m_column.cancelMove(window)) {
        reflow();
    }
}

void GridLayoutEngine::pruneEmpty()
{
    if (m_column.pruneEmpty()) {
        reflow();
    }
}

void GridLayoutEngine::reflow()
{
    if (m_column.isEmpty()) {
        return;
    }
    if (reflowZoomed(m_column.leaves())) {
        return;
    }

    const QList<CustomTile *> leaves = m_column.leaves();
    const int n = leaves.count();
    const gridmath::GridShape shape = gridmath::targetShape(n);
    const std::vector<gridmath::Rect> cells = gridmath::cellRects(n, shape);
    for (int i = 0; i < n; ++i) {
        CustomTile *leaf = leaves[i];
        if (!leaf) {
            continue;
        }
        const gridmath::Rect &cell = cells[static_cast<size_t>(i)];
        leaf->setRelativeGeometry(RectF(cell.x, cell.y, cell.w, cell.h));
        const QList<Window *> ws = leaf->windows();
        if (!ws.isEmpty()) {
            ws.first()->setHidden(false);
        }
    }
    Q_EMIT layoutChanged();
}

bool GridLayoutEngine::applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged)
{
    Q_UNUSED(window)
    Q_UNUSED(area)
    Q_UNUSED(widthChanged)
    Q_UNUSED(heightChanged)
    reflow();
    return true;
}

QList<Window *> GridLayoutEngine::windows() const
{
    return m_column.windows();
}

Window *GridLayoutEngine::windowInDirection(Window *from, FocusDirection direction) const
{
    QList<QPair<Window *, RectF>> entries;
    for (CustomTile *leaf : m_column.leaves()) {
        if (!leaf) {
            continue;
        }
        const QList<Window *> ws = leaf->windows();
        if (ws.isEmpty()) {
            continue;
        }
        entries.append({ws.first(), leaf->relativeGeometry()});
    }
    return windowInDirectionFromRects(entries, from, direction);
}

} // namespace KWin