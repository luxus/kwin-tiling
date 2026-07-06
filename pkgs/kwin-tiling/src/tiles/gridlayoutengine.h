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
 * Grid layout — smoothly-interpolating k×k / k×(k+1) cell placement.
 *
 * Implemented as a single StackColumn whose leaf order is the grid slot order;
 * reflow assigns geometries from gridmath instead of vertical weights.
 */
class KWIN_EXPORT GridLayoutEngine : public LayoutEngine
{
    Q_OBJECT

public:
    explicit GridLayoutEngine(QObject *parent = nullptr);
    ~GridLayoutEngine() override;

    LayoutKind layoutKind() const override { return LayoutKind::Grid; }
    void attach(RootTile *root) override;
    void addWindow(Window *window) override;
    void removeWindow(Window *window) override;
    void moveWindow(Window *window, int delta) override;
    void beginMoveWindow(Window *window) override;
    bool endMoveWindow(Window *window, Window *target) override;
    void cancelMoveWindow(Window *window) override;
    void reflow() override;
    void pruneEmpty() override;

    QList<Window *> windows() const override;
    Window *windowInDirection(Window *from, FocusDirection direction) const override;

private:
    bool applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged) override;

    StackColumn m_column;
};

} // namespace KWin