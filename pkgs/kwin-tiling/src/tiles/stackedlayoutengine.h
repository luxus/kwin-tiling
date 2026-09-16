/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "layoutengine.h"
#include "stackcolumn.h"

namespace KWin
{

class RootTile;
class Window;

/**
 * Stacked (single-column) layout.
 *
 * Every tiled window fills the full width of the root and is stacked vertically,
 * its height split by per-window weights. There is no master / primary window.
 *
 * Implemented as a single StackColumn filling the whole root — all the leaf,
 * weight, drag and monocle mechanics live in the shared primitive, so this
 * engine is just "one column, full area".
 */
class KWIN_EXPORT StackedLayoutEngine : public LayoutEngine
{
    Q_OBJECT

public:
    explicit StackedLayoutEngine(QObject *parent = nullptr);
    ~StackedLayoutEngine() override;

    LayoutKind layoutKind() const override { return LayoutKind::Stacked; }
    void attach(RootTile *root) override;
    void addWindow(Window *window) override;
    void removeWindow(Window *window) override;
    void moveWindow(Window *window, int delta) override;
    void reorderWindow(Window *window, int delta) override;
    void beginMoveWindow(Window *window) override;
    bool endMoveWindow(Window *window, Window *target) override;
    bool endMoveWindowOnZone(Window *window, Window *target, DropZone zone) override;
    void cancelMoveWindow(Window *window) override;
    void dropWindow(Window *window, Window *target, const QPointF &pos, const RectF &area) override;
    void reflow() override;
    void pruneEmpty() override;
    bool ownsGhostLeaf(Window *window) const override { return m_column.ownsGhostLeaf(window); }

    void adjustWindowHeight(Window *window, qreal delta) override;

    QList<Window *> windows() const override;
    bool contains(Window *window) const override;
    Window *windowInDirection(Window *from, FocusDirection direction) const override;

private:
    bool applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged) override;

    StackColumn m_column;
};

} // namespace KWin
