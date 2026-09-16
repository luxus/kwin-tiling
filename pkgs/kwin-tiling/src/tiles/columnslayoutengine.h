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

class CustomTile;
class RootTile;
class Window;

/**
 * Columns layout — equal-width columns that always fill the view.
 *
 * Distinct from Scrolling: there is no viewport, no off-screen hide, and
 * column widths always sum to 1.0. Compose StackColumn + columnlayoutmath /
 * insertpolicy; do not leak Scrolling's scrollOffset / consume-left behaviour.
 *
 * Columns grow from 1 to maxColumns (default 3) as windows are added; further
 * windows stack in the focused column (or the fewest, leftmost on a tie).
 */
class KWIN_EXPORT ColumnsLayoutEngine : public LayoutEngine
{
    Q_OBJECT

public:
    explicit ColumnsLayoutEngine(QObject *parent = nullptr);
    ~ColumnsLayoutEngine() override;

    LayoutKind layoutKind() const override { return LayoutKind::Columns; }
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
    bool ownsGhostLeaf(Window *window) const override;

    QList<Window *> windows() const override;
    Window *windowInDirection(Window *from, FocusDirection direction) const override;
    void setActiveWindow(Window *window) override;

    void setPrimarySplit(qreal ratio) override;
    qreal primarySplit() const override;
    void setMaxColumns(int maxColumns) override;
    void resetSizes() override;
    void consumeWindow() override;
    void expelWindow() override;
    void adjustWindowHeight(Window *window, qreal delta) override;

private:
    bool applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged) override;

    struct Column
    {
        StackColumn stack;
        qreal width = 0.0;
    };

    bool findWindow(Window *window, int *colIdx, int *leafIdx) const;
    int activeColumnIndex() const;
    int focusedColumnOrNeg() const;
    void rebalanceWidths();
    void normalizeWidths();
    QList<CustomTile *> allLeaves() const;
    int flatIndex(int col, int row) const;
    void insertDetachedAtFlat(const StackColumn::Detached &detached, int flatIdx);

    QPointer<RootTile> m_root;
    QList<Column> m_columns;
    QPointer<Window> m_activeWindow;
    int m_maxColumns = 3;
    bool m_moveHasSource = false;
    int m_moveSourceColumn = -1;
};

} // namespace KWin
