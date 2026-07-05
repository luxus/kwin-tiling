/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "stackedlayoutengine.h"
#include "customtile.h"
#include "window.h"

namespace KWin
{

StackedLayoutEngine::StackedLayoutEngine(QObject *parent)
    : LayoutEngine(parent)
{
}

StackedLayoutEngine::~StackedLayoutEngine()
{
    // Never hand windows to another engine (layout switch) left hidden.
    m_column.unhideAll();
}

void StackedLayoutEngine::attach(RootTile *root)
{
    takeOwnershipOfRoot(root);
    m_column.setRoot(root);
}

void StackedLayoutEngine::addWindow(Window *window)
{
    if (m_column.insertWindow(window)) {
        reflow();
    }
}

void StackedLayoutEngine::removeWindow(Window *window)
{
    m_column.removeWindow(window);
    reflow();
}

void StackedLayoutEngine::moveWindow(Window *window, int delta)
{
    m_column.swapByDelta(window, delta);
    reflow();
}

void StackedLayoutEngine::beginMoveWindow(Window *window)
{
    m_column.beginMove(window);
}

bool StackedLayoutEngine::endMoveWindow(Window *window, Window *target)
{
    const bool handled = m_column.endMove(window, target);
    if (handled) {
        reflow();
    }
    return handled;
}

void StackedLayoutEngine::cancelMoveWindow(Window *window)
{
    if (m_column.cancelMove(window)) {
        reflow();
    }
}

void StackedLayoutEngine::pruneEmpty()
{
    if (m_column.pruneEmpty()) {
        reflow();
    }
}

void StackedLayoutEngine::reflow()
{
    if (m_column.isEmpty()) {
        return;
    }
    if (reflowZoomed(m_column.leaves())) {
        return;
    }
    m_column.fill(RectF(0, 0, 1, 1));
    Q_EMIT layoutChanged();
}

void StackedLayoutEngine::adjustWindowHeight(Window *window, qreal delta)
{
    if (m_column.count() < 2) {
        return;
    }
    m_column.bumpWeight(window, delta);
    reflow();
}

bool StackedLayoutEngine::applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged)
{
    Q_UNUSED(widthChanged)
    if (!m_column.contains(window)) {
        return false;
    }
    // A single window fills the screen; snap it back instead of keeping the
    // dragged size.
    if (m_column.count() < 2) {
        reflow();
        return true;
    }
    if (heightChanged && area.height() > 0) {
        const qreal newHeight = window->frameGeometry().height();
        if (newHeight > 0) {
            m_column.applyHeightDrag(window, newHeight / area.height(), 0, m_column.count());
        }
    }
    reflow();
    return true;
}

QList<Window *> StackedLayoutEngine::windows() const
{
    return m_column.windows();
}

Window *StackedLayoutEngine::windowInDirection(Window *from, FocusDirection direction) const
{
    const QList<Window *> ws = m_column.windows();
    if (ws.isEmpty()) {
        return nullptr;
    }
    if (!from || m_column.indexOf(from) < 0) {
        return ws.first();
    }
    switch (direction) {
    case FocusDirection::Up:
        return m_column.vertical(from, false);
    case FocusDirection::Down:
        return m_column.vertical(from, true);
    case FocusDirection::Left:
    case FocusDirection::Right:
        // No horizontal neighbours in a single column.
        return nullptr;
    }
    return nullptr;
}

} // namespace KWin
