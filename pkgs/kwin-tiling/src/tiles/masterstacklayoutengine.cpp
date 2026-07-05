/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "masterstacklayoutengine.h"
#include "customtile.h"
#include "window.h"

#include <algorithm>

namespace KWin
{

MasterStackLayoutEngine::MasterStackLayoutEngine(QObject *parent, LayoutKind kind)
    : LayoutEngine(parent)
    , m_kind(kind)
{
}

MasterStackLayoutEngine::~MasterStackLayoutEngine()
{
    m_column.unhideAll();
    m_left.unhideAll();
    m_center.unhideAll();
    m_right.unhideAll();
}

StackColumn *MasterStackLayoutEngine::findColumn(Window *window, SideColumn *side)
{
    if (m_left.contains(window)) {
        if (side) {
            *side = SideColumn::Left;
        }
        return &m_left;
    }
    if (m_center.contains(window)) {
        if (side) {
            *side = SideColumn::Center;
        }
        return &m_center;
    }
    if (m_right.contains(window)) {
        if (side) {
            *side = SideColumn::Right;
        }
        return &m_right;
    }
    return nullptr;
}

const StackColumn *MasterStackLayoutEngine::findColumn(Window *window, SideColumn *side) const
{
    return const_cast<MasterStackLayoutEngine *>(this)->findColumn(window, side);
}

StackColumn *MasterStackLayoutEngine::columnFor(SideColumn side)
{
    switch (side) {
    case SideColumn::Left:
        return &m_left;
    case SideColumn::Center:
        return &m_center;
    case SideColumn::Right:
        return &m_right;
    }
    return &m_center;
}

const StackColumn *MasterStackLayoutEngine::columnFor(SideColumn side) const
{
    return const_cast<MasterStackLayoutEngine *>(this)->columnFor(side);
}

void MasterStackLayoutEngine::attach(RootTile *root)
{
    if (root) {
        const QList<Tile *> existingChildren = root->childTiles();
        for (Tile *child : existingChildren) {
            if (CustomTile *custom = qobject_cast<CustomTile *>(child)) {
                root->destroyChild(custom);
            }
        }
        root->setLayoutDirection(Tile::LayoutDirection::Floating);
        root->setRelativeGeometry(RectF(0, 0, 1, 1));
    }
    if (isCentered()) {
        m_left.setRoot(root);
        m_center.setRoot(root);
        m_right.setRoot(root);
    } else {
        m_column.setRoot(root);
    }
}

void MasterStackLayoutEngine::addWindowCentered(Window *window)
{
    StackColumn *target = nullptr;
    if (m_center.count() < m_masterCount) {
        target = &m_center;
    } else if (m_nextSideIsRight) {
        target = &m_right;
        m_nextSideIsRight = false;
    } else {
        target = &m_left;
        m_nextSideIsRight = true;
    }
    if (target->insertWindow(window)) {
        reflow();
    }
}

void MasterStackLayoutEngine::addWindow(Window *window)
{
    if (isCentered()) {
        addWindowCentered(window);
        return;
    }
    if (m_column.insertWindow(window)) {
        reflow();
    }
}

void MasterStackLayoutEngine::removeWindow(Window *window)
{
    if (isCentered()) {
        if (StackColumn *col = findColumn(window)) {
            col->removeWindow(window);
            reflow();
        }
        return;
    }
    m_column.removeWindow(window);
    reflow();
}

void MasterStackLayoutEngine::moveWindow(Window *window, int delta)
{
    if (isCentered()) {
        if (StackColumn *col = findColumn(window)) {
            col->swapByDelta(window, delta);
            reflow();
        }
        return;
    }
    m_column.swapByDelta(window, delta);
    reflow();
}

void MasterStackLayoutEngine::beginMoveWindow(Window *window)
{
    if (isCentered()) {
        SideColumn side;
        if (StackColumn *col = findColumn(window, &side)) {
            col->beginMove(window);
            m_moveHasSource = true;
            m_moveSourceSide = side;
        }
        return;
    }
    m_column.beginMove(window);
}

bool MasterStackLayoutEngine::endMoveWindow(Window *window, Window *target)
{
    if (isCentered()) {
        if (!m_moveHasSource) {
            return false;
        }
        m_moveHasSource = false;
        StackColumn *sourceCol = columnFor(m_moveSourceSide);
        if (!sourceCol) {
            return false;
        }

        if (target && target != window) {
            SideColumn targetSide;
            if (StackColumn *targetCol = findColumn(target, &targetSide)) {
                if (targetCol == sourceCol) {
                    const bool handled = sourceCol->endMove(window, target);
                    if (handled) {
                        reflow();
                    }
                    return handled;
                }

                const int srcIdx = sourceCol->indexOf(window);
                const int tgtIdx = targetCol->indexOf(target);
                if (srcIdx < 0 || tgtIdx < 0) {
                    return sourceCol->endMove(window, nullptr);
                }

                StackColumn::Detached detachedWindow = sourceCol->detachWindow(window);
                StackColumn::Detached detachedTarget = targetCol->detachWindow(target);
                sourceCol->attachLeaf(detachedTarget, srcIdx);
                targetCol->attachLeaf(detachedWindow, tgtIdx);
                reflow();
                return true;
            }
        }

        const bool handled = sourceCol->endMove(window, nullptr);
        if (handled) {
            reflow();
        }
        return handled;
    }

    const bool handled = m_column.endMove(window, target);
    if (handled) {
        reflow();
    }
    return handled;
}

void MasterStackLayoutEngine::cancelMoveWindow(Window *window)
{
    if (isCentered()) {
        if (!m_moveHasSource) {
            return;
        }
        m_moveHasSource = false;
        if (StackColumn *col = columnFor(m_moveSourceSide)) {
            if (col->cancelMove(window)) {
                reflow();
            }
        }
        return;
    }
    if (m_column.cancelMove(window)) {
        reflow();
    }
}

void MasterStackLayoutEngine::dropWindow(Window *window, Window *target, const QPointF &pos, const RectF &area)
{
    if (!window) {
        return;
    }

    if (isCentered()) {
        StackColumn *col = nullptr;
        int at = -1;
        if (target && target != window) {
            col = findColumn(target);
            if (col) {
                at = col->indexOf(target);
            }
        } else {
            qreal leftWidth = 0.0;
            qreal centerWidth = 1.0;
            qreal rightWidth = 0.0;
            qreal leftX = 0.0;
            qreal centerX = 0.0;
            qreal rightX = 0.0;
            centeredHorizontalLayout(leftWidth, centerWidth, rightWidth, leftX, centerX, rightX);
            const qreal relX = (area.width() > 0) ? (pos.x() - area.x()) / area.width() : 0.5;
            if (m_left.count() > 0 || m_right.count() > 0) {
                if (relX < leftX + leftWidth) {
                    col = &m_left;
                } else if (relX > centerX + centerWidth) {
                    col = &m_right;
                } else {
                    col = &m_center;
                }
            } else {
                col = &m_center;
            }
        }
        if (!col) {
            col = &m_center;
        }
        if (col->insertWindow(window, at)) {
            reflow();
        }
        return;
    }

    int index;
    if (target && target != window) {
        index = m_column.indexOf(target);
        if (index < 0) {
            index = m_column.count();
        }
    } else {
        const qreal relX = (area.width() > 0) ? (pos.x() - area.x()) / area.width() : 1.0;
        index = (relX < m_masterRatio) ? 0 : m_column.count();
    }

    if (m_column.insertWindow(window, index)) {
        reflow();
    }
}

void MasterStackLayoutEngine::pruneEmpty()
{
    if (isCentered()) {
        bool changed = m_left.pruneEmpty() || m_center.pruneEmpty() || m_right.pruneEmpty();
        if (changed) {
            reflow();
        }
        return;
    }
    if (m_column.pruneEmpty()) {
        reflow();
    }
}

void MasterStackLayoutEngine::reflow()
{
    if (isCentered()) {
        reflowCentered();
        return;
    }

    if (m_column.isEmpty()) {
        return;
    }
    if (reflowZoomed(m_column.leaves())) {
        return;
    }

    const int count = m_column.count();
    if (count == 1) {
        m_column.fill(RectF(0, 0, 1, 1));
        Q_EMIT layoutChanged();
        return;
    }

    reflowMasterStack(count);
    Q_EMIT layoutChanged();
}

void MasterStackLayoutEngine::reflowMasterStack(int count)
{
    const int masters = std::min(m_masterCount, count - 1);
    const qreal masterWidth = m_masterRatio;
    const qreal stackWidth = 1.0 - masterWidth;

    if (m_masterOnRight) {
        m_column.fillRange(0, masters, RectF(stackWidth, 0.0, masterWidth, 1.0));
        m_column.fillRange(masters, count, RectF(0.0, 0.0, stackWidth, 1.0));
    } else {
        m_column.fillRange(0, masters, RectF(0.0, 0.0, masterWidth, 1.0));
        m_column.fillRange(masters, count, RectF(masterWidth, 0.0, stackWidth, 1.0));
    }
}

void MasterStackLayoutEngine::centeredHorizontalLayout(qreal &leftWidth, qreal &centerWidth, qreal &rightWidth,
                                                     qreal &leftX, qreal &centerX, qreal &rightX) const
{
    leftWidth = 0.0;
    centerWidth = 1.0;
    rightWidth = 0.0;
    leftX = 0.0;
    centerX = 0.0;
    rightX = 0.0;

    if (m_left.count() > 0 || m_right.count() > 0) {
        constexpr qreal minColumnWidth = 0.15;
        const qreal requestedSide = (1.0 - m_masterRatio) / 2.0;
        const qreal sideWidth = std::max(requestedSide, minColumnWidth);
        centerWidth = std::max(0.0, 1.0 - 2.0 * sideWidth);
        leftWidth = sideWidth;
        rightWidth = sideWidth;
        leftX = 0.0;
        centerX = leftWidth;
        rightX = leftWidth + centerWidth;
    }
}

void MasterStackLayoutEngine::reflowCentered()
{
    const int total = m_left.count() + m_center.count() + m_right.count();
    if (total == 0) {
        return;
    }

    QList<CustomTile *> allLeaves;
    allLeaves += m_left.leaves();
    allLeaves += m_center.leaves();
    allLeaves += m_right.leaves();
    if (reflowZoomed(allLeaves)) {
        return;
    }

    if (total == 1) {
        if (m_center.count() > 0) {
            m_center.fill(RectF(0, 0, 1, 1));
        } else if (m_left.count() > 0) {
            m_left.fill(RectF(0, 0, 1, 1));
        } else {
            m_right.fill(RectF(0, 0, 1, 1));
        }
        Q_EMIT layoutChanged();
        return;
    }

    qreal leftWidth = 0.0;
    qreal centerWidth = 1.0;
    qreal rightWidth = 0.0;
    qreal leftX = 0.0;
    qreal centerX = 0.0;
    qreal rightX = 0.0;
    centeredHorizontalLayout(leftWidth, centerWidth, rightWidth, leftX, centerX, rightX);

    if (m_left.count() > 0) {
        m_left.fill(RectF(leftX, 0.0, leftWidth, 1.0));
    }
    if (m_center.count() > 0) {
        m_center.fill(RectF(centerX, 0.0, centerWidth, 1.0));
    }
    if (m_right.count() > 0) {
        m_right.fill(RectF(rightX, 0.0, rightWidth, 1.0));
    }

    Q_EMIT layoutChanged();
}

void MasterStackLayoutEngine::columnRangeFor(int idx, int count, int &first, int &last) const
{
    const int masters = std::min(m_masterCount, count - 1);
    if (idx < masters) {
        first = 0;
        last = masters;
    } else {
        first = masters;
        last = count;
    }
}

void MasterStackLayoutEngine::adjustWindowHeight(Window *window, qreal delta)
{
    if (isCentered()) {
        if (StackColumn *col = findColumn(window)) {
            if (col->count() < 2) {
                return;
            }
            col->bumpWeight(window, delta);
            reflow();
        }
        return;
    }

    const int count = m_column.count();
    if (count < 2) {
        return;
    }
    const int idx = m_column.indexOf(window);
    if (idx < 0) {
        return;
    }
    int first = 0;
    int last = count;
    columnRangeFor(idx, count, first, last);
    if (last - first < 2) {
        return;
    }
    m_column.bumpWeight(window, delta);
    reflow();
}

void MasterStackLayoutEngine::setMasterRatio(qreal ratio)
{
    ratio = std::clamp(ratio, 0.1, 0.9);
    if (qFuzzyCompare(m_masterRatio, ratio)) {
        return;
    }
    m_masterRatio = ratio;
    reflow();
}

void MasterStackLayoutEngine::setMasterCount(int count)
{
    count = std::max(count, 1);
    if (m_masterCount == count) {
        return;
    }
    m_masterCount = count;
    reflow();
}

void MasterStackLayoutEngine::resetSizes()
{
    m_masterRatio = 0.5;
    if (isCentered()) {
        m_left.clearWeights();
        m_center.clearWeights();
        m_right.clearWeights();
    } else {
        m_column.clearWeights();
    }
    reflow();
}

void MasterStackLayoutEngine::flipMaster()
{
    if (isCentered()) {
        return;
    }
    m_masterOnRight = !m_masterOnRight;
    reflow();
}

bool MasterStackLayoutEngine::endResizeWindow(Window *window, const RectF &area)
{
    if (!window || (area.width() <= 0 && area.height() <= 0)) {
        return false;
    }

    if (isCentered()) {
        SideColumn side;
        StackColumn *col = findColumn(window, &side);
        if (!col) {
            return false;
        }

        const int count = col->count();
        if (count < 1) {
            return false;
        }
        if (count == 1) {
            reflow();
            return true;
        }

        const auto geom = window->frameGeometry();
        if (area.height() > 0 && count >= 2 && geom.height() > 0) {
            col->applyHeightDrag(window, geom.height() / area.height(), 0, count);
            reflow();
        }

        if (area.width() > 0 && geom.width() > 0) {
            const qreal frac = geom.width() / area.width();
            if (side == SideColumn::Center) {
                setMasterRatio(frac);
            } else {
                setMasterRatio(1.0 - 2.0 * frac);
            }
        }
        return true;
    }

    const int idx = m_column.indexOf(window);
    if (idx < 0) {
        return false;
    }

    const int count = m_column.count();
    if (count < 2) {
        reflow();
        return true;
    }

    const auto geom = window->frameGeometry();
    int colStart = 0;
    int colEnd = count;
    columnRangeFor(idx, count, colStart, colEnd);
    if (area.height() > 0 && (colEnd - colStart) >= 2) {
        const qreal newHeight = geom.height();
        if (newHeight > 0) {
            m_column.applyHeightDrag(window, newHeight / area.height(), colStart, colEnd);
            reflow();
        }
    }

    if (area.width() > 0 && geom.width() > 0) {
        const int masters = std::min(m_masterCount, count - 1);
        const qreal ratio = (idx < masters)
            ? geom.width() / area.width()
            : (geom.x() - area.x()) / area.width();
        setMasterRatio(ratio);
    }
    return true;
}

QList<Window *> MasterStackLayoutEngine::windows() const
{
    if (isCentered()) {
        QList<Window *> result;
        result += m_left.windows();
        result += m_center.windows();
        result += m_right.windows();
        return result;
    }
    return m_column.windows();
}

Window *MasterStackLayoutEngine::primaryWindow() const
{
    if (isCentered()) {
        return m_center.windowAt(0);
    }
    const QList<Window *> ws = m_column.windows();
    return ws.isEmpty() ? nullptr : ws.first();
}

Window *MasterStackLayoutEngine::windowInDirection(Window *from, FocusDirection direction) const
{
    if (isCentered()) {
        return windowInDirectionCentered(from, direction);
    }

    const QList<Window *> ws = m_column.windows();
    if (ws.isEmpty()) {
        return nullptr;
    }

    const int idx = from ? ws.indexOf(from) : -1;
    if (idx < 0) {
        return ws.first();
    }

    switch (direction) {
    case FocusDirection::Left:
        if (idx == 0) {
            return nullptr;
        }
        return ws[0];
    case FocusDirection::Right:
        if (idx == 0) {
            return ws.count() > 1 ? ws[1] : nullptr;
        }
        return nullptr;
    case FocusDirection::Up:
        if (idx > 0) {
            return ws[idx - 1];
        }
        return ws.count() > 1 ? ws[1] : nullptr;
    case FocusDirection::Down:
        if (idx < ws.count() - 1) {
            return ws[idx + 1];
        }
        return ws[0];
    }

    return nullptr;
}

Window *MasterStackLayoutEngine::windowInDirectionCentered(Window *from, FocusDirection direction) const
{
    if (!from) {
        if (Window *w = m_center.windowAt(0)) {
            return w;
        }
        if (Window *w = m_left.windowAt(0)) {
            return w;
        }
        return m_right.windowAt(0);
    }

    SideColumn side;
    const StackColumn *col = findColumn(from, &side);
    if (!col) {
        return nullptr;
    }

    const int row = col->indexOf(from);
    const auto atRow = [](const StackColumn *column, int r) -> Window * {
        if (!column || column->count() == 0) {
            return nullptr;
        }
        return column->windowAt(std::clamp(r, 0, column->count() - 1));
    };

    switch (direction) {
    case FocusDirection::Left:
        if (side == SideColumn::Right) {
            return atRow(&m_center, row);
        }
        if (side == SideColumn::Center) {
            return atRow(&m_left, row);
        }
        return nullptr;
    case FocusDirection::Right:
        if (side == SideColumn::Left) {
            return atRow(&m_center, row);
        }
        if (side == SideColumn::Center) {
            return atRow(&m_right, row);
        }
        return nullptr;
    case FocusDirection::Up:
        return col->vertical(from, false);
    case FocusDirection::Down:
        return col->vertical(from, true);
    }

    return nullptr;
}

} // namespace KWin