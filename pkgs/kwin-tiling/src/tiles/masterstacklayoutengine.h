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
 * Both compose StackColumn primitives; only the arrangement differs:
 *   - MasterStack: one column, two index ranges (master run + stack run).
 *   - Centered   : three columns (left | centre master | right), each a
 *                  StackColumn. New windows fill the centre up to masterCount,
 *                  then alternate onto the side stacks.
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
    Window *primaryWindow() const override;
    Window *windowInDirection(Window *from, FocusDirection direction) const override;

    void setPrimarySplit(qreal ratio) override { setMasterRatio(ratio); }
    qreal primarySplit() const override { return m_masterRatio; }
    void setPrimaryCount(int count) override { setMasterCount(count); }
    void adjustWindowHeight(Window *window, qreal delta) override;
    void resetSizes() override;
    void flipMaster() override;

    qreal masterRatio() const { return m_masterRatio; }
    void setMasterRatio(qreal ratio);

    int masterCount() const { return m_masterCount; }
    void setMasterCount(int count);

private:
    enum class SideColumn {
        Left,
        Center,
        Right,
    };

    bool isCentered() const { return m_kind == LayoutKind::Centered; }

    StackColumn *findColumn(Window *window, SideColumn *side = nullptr);
    const StackColumn *findColumn(Window *window, SideColumn *side = nullptr) const;
    StackColumn *columnFor(SideColumn side);
    const StackColumn *columnFor(SideColumn side) const;

    void addWindowCentered(Window *window);
    void reflowMasterStack(int count);
    void reflowCentered();
    void centeredHorizontalLayout(qreal &leftWidth, qreal &centerWidth, qreal &rightWidth,
                                  qreal &leftX, qreal &centerX, qreal &rightX) const;
    Window *windowInDirectionCentered(Window *from, FocusDirection direction) const;
    void columnRangeFor(int idx, int count, int &first, int &last) const;
    bool applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged) override;

    const LayoutKind m_kind;
    StackColumn m_column;
    StackColumn m_left;
    StackColumn m_center;
    StackColumn m_right;
    qreal m_masterRatio = 0.5;
    int m_masterCount = 1;
    bool m_masterOnRight = false;
    bool m_nextSideIsRight = false;
    bool m_moveHasSource = false;
    SideColumn m_moveSourceSide = SideColumn::Center;
};

} // namespace KWin