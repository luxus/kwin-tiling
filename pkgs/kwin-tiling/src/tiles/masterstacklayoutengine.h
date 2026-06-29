/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
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
 * Master-stack and centered-master layouts.
 *
 * Both arrange a single StackColumn (the leaf strip) into columns by index
 * range, so the master ratio/count, the leaf/weight/drag/monocle mechanics, and
 * the resize handling are shared; only the rectangles differ:
 *   - MasterStack: master run on one side, stack run on the other (2 columns).
 *   - Centered   : the master run centred at `masterRatio` width, the remaining
 *                  windows split into a left and a right column (3 columns,
 *                  e.g. 20/60/20), each stacked. A lone window (no stack) fills
 *                  the full width.
 */
class KWIN_EXPORT MasterStackLayoutEngine : public LayoutEngine
{
    Q_OBJECT

public:
    explicit MasterStackLayoutEngine(QObject *parent = nullptr, LayoutKind kind = LayoutKind::MasterStack);
    ~MasterStackLayoutEngine() override;

    LayoutKind layoutKind() const override { return m_kind; }
    void attach(RootTile *root) override;
    void addWindow(Window *window) override;
    void removeWindow(Window *window) override;
    void moveWindow(Window *window, int delta) override;
    void beginMoveWindow(Window *window) override;
    bool endMoveWindow(Window *window, Window *target) override;
    void cancelMoveWindow(Window *window) override;
    void dropWindow(Window *window, Window *target, const QPointF &pos, const RectF &area) override;
    void reflow() override;
    void pruneEmpty() override;

    QList<Window *> windows() const override;
    Window *windowInDirection(Window *from, FocusDirection direction) const override;

    void setPrimarySplit(qreal ratio) override { setMasterRatio(ratio); }
    qreal primarySplit() const override { return m_masterRatio; }
    void setPrimaryCount(int count) override { setMasterCount(count); }
    void adjustWindowHeight(Window *window, qreal delta) override;
    bool endResizeWindow(Window *window, const RectF &area) override;
    void resetSizes() override;
    void flipMaster() override;

    qreal masterRatio() const { return m_masterRatio; }
    void setMasterRatio(qreal ratio);

    int masterCount() const { return m_masterCount; }
    void setMasterCount(int count);

private:
    // MasterStack: master run [0, masters) and stack run [masters, count).
    void reflowMasterStack(int count);
    // Centered: centre run + left/right runs (or full width when no stack).
    void reflowCentered(int count);
    Window *windowInDirectionCentered(Window *from, FocusDirection direction) const;
    // Centered split: number of windows in the centre run and in the left run
    // (the rest go right). leftCount is 0 when there is no stack.
    void centeredCounts(int count, int &masters, int &leftCount) const;
    // The [first, last) strip range of the column containing leaf `idx`, for
    // the current kind — used to scope height resize to one column.
    void columnRangeFor(int idx, int count, int &first, int &last) const;

    const LayoutKind m_kind;
    StackColumn m_column;
    qreal m_masterRatio = 0.5;
    int m_masterCount = 1;
    bool m_masterOnRight = false;
};

} // namespace KWin
