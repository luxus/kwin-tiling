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
}

void MasterStackLayoutEngine::attach(RootTile *root)
{
    if (root) {
        // Take full ownership of the root: drop any pre-existing default-layout
        // children and make it a plain floating container the engine drives.
        const QList<Tile *> existingChildren = root->childTiles();
        for (Tile *child : existingChildren) {
            if (CustomTile *custom = qobject_cast<CustomTile *>(child)) {
                root->destroyChild(custom);
            }
        }
        root->setLayoutDirection(Tile::LayoutDirection::Floating);
        root->setRelativeGeometry(RectF(0, 0, 1, 1));
    }
    m_column.setRoot(root);
}

void MasterStackLayoutEngine::addWindow(Window *window)
{
    if (m_column.insertWindow(window)) {
        reflow();
    }
}

void MasterStackLayoutEngine::removeWindow(Window *window)
{
    m_column.removeWindow(window);
    reflow();
}

void MasterStackLayoutEngine::moveWindow(Window *window, int delta)
{
    m_column.swapByDelta(window, delta);
    reflow();
}

void MasterStackLayoutEngine::beginMoveWindow(Window *window)
{
    m_column.beginMove(window);
}

bool MasterStackLayoutEngine::endMoveWindow(Window *window, Window *target)
{
    const bool handled = m_column.endMove(window, target);
    if (handled) {
        reflow();
    }
    return handled;
}

void MasterStackLayoutEngine::cancelMoveWindow(Window *window)
{
    if (m_column.cancelMove(window)) {
        reflow();
    }
}

void MasterStackLayoutEngine::dropWindow(Window *window, Window *target, const QPointF &pos, const RectF &area)
{
    if (!window) {
        return;
    }

    // Decide where in the layout order the dropped window lands.
    int index;
    if (target && target != window) {
        // Dropped onto a tiled window: take its slot (it and the rest shift).
        index = m_column.indexOf(target);
        if (index < 0) {
            index = m_column.count();
        }
    } else if (m_kind == LayoutKind::Centered) {
        // Empty space in centered mode: append (lands in a side column).
        index = m_column.count();
    } else {
        // Dropped on empty space: left of the master/stack divider joins the
        // master column (top); right of it appends to the stack.
        const qreal relX = (area.width() > 0) ? (pos.x() - area.x()) / area.width() : 1.0;
        index = (relX < m_masterRatio) ? 0 : m_column.count();
    }

    if (m_column.insertWindow(window, index)) {
        reflow();
    }
}

void MasterStackLayoutEngine::pruneEmpty()
{
    if (m_column.pruneEmpty()) {
        reflow();
    }
}

void MasterStackLayoutEngine::reflow()
{
    if (m_column.isEmpty()) {
        return;
    }
    if (reflowZoomed(m_column.leaves())) {
        return;
    }

    const int count = m_column.count();
    if (count == 1) {
        // A lone window fills the screen in either mode.
        m_column.fill(RectF(0, 0, 1, 1));
        Q_EMIT layoutChanged();
        return;
    }

    if (m_kind == LayoutKind::Centered) {
        reflowCentered(count);
    } else {
        reflowMasterStack(count);
    }
    Q_EMIT layoutChanged();
}

void MasterStackLayoutEngine::reflowMasterStack(int count)
{
    // Clamp master count to at most count - 1 so there is always a stack.
    const int masters = std::min(m_masterCount, count - 1);
    const qreal masterWidth = m_masterRatio;
    const qreal stackWidth = 1.0 - masterWidth;

    if (m_masterOnRight) {
        m_column.fillRange(0, masters, RectF(stackWidth, 0.0, masterWidth, 1.0)); // master (right)
        m_column.fillRange(masters, count, RectF(0.0, 0.0, stackWidth, 1.0));     // stack (left)
    } else {
        m_column.fillRange(0, masters, RectF(0.0, 0.0, masterWidth, 1.0));          // master (left)
        m_column.fillRange(masters, count, RectF(masterWidth, 0.0, stackWidth, 1.0)); // stack (right)
    }
}

void MasterStackLayoutEngine::centeredCounts(int count, int &masters, int &leftCount) const
{
    masters = std::clamp(m_masterCount, 1, std::max(1, count));
    const int extra = count - masters;
    leftCount = (extra > 0) ? extra / 2 : 0; // left gets floor(extra/2); right takes the rest
}

void MasterStackLayoutEngine::reflowCentered(int count)
{
    int masters = 0;
    int leftCount = 0;
    centeredCounts(count, masters, leftCount);
    const int extra = count - masters;

    if (extra <= 0) {
        // No stack windows: the master run fills the full width.
        m_column.fillRange(0, count, RectF(0, 0, 1, 1));
        return;
    }

    const qreal centreWidth = std::clamp(m_masterRatio, 0.1, 0.9);
    const qreal sideWidth = (1.0 - centreWidth) / 2.0;
    const int leftEnd = masters + leftCount;

    m_column.fillRange(0, masters, RectF(sideWidth, 0.0, centreWidth, 1.0));                 // centre
    m_column.fillRange(masters, leftEnd, RectF(0.0, 0.0, sideWidth, 1.0));                   // left
    m_column.fillRange(leftEnd, count, RectF(sideWidth + centreWidth, 0.0, sideWidth, 1.0)); // right
}

void MasterStackLayoutEngine::columnRangeFor(int idx, int count, int &first, int &last) const
{
    if (m_kind == LayoutKind::Centered) {
        int masters = 0;
        int leftCount = 0;
        centeredCounts(count, masters, leftCount);
        const int extra = count - masters;
        if (extra <= 0 || idx < masters) { // centre (or full-width when no stack)
            first = 0;
            last = (extra <= 0) ? count : masters;
            return;
        }
        const int leftEnd = masters + leftCount;
        if (idx < leftEnd) { // left
            first = masters;
            last = leftEnd;
        } else { // right
            first = leftEnd;
            last = count;
        }
        return;
    }

    // MasterStack: master run [0, masters) or stack run [masters, count).
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
    const int count = m_column.count();
    if (count < 2) {
        return;
    }
    const int idx = m_column.indexOf(window);
    if (idx < 0) {
        return;
    }
    // Resizing only shares height within a column, so it needs >= 2 windows in
    // the window's own column (master/stack, or centre/left/right).
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
    m_column.clearWeights();
    reflow();
}

void MasterStackLayoutEngine::flipMaster()
{
    m_masterOnRight = !m_masterOnRight;
    reflow();
}

bool MasterStackLayoutEngine::endResizeWindow(Window *window, const RectF &area)
{
    if (!window || (area.width() <= 0 && area.height() <= 0)) {
        return false;
    }
    const int idx = m_column.indexOf(window);
    if (idx < 0) {
        return false;
    }

    // A single window fills the screen; there is no master/stack boundary to
    // move. Reflow so the window snaps back to full screen.
    const int count = m_column.count();
    if (count < 2) {
        reflow();
        return true;
    }

    const auto geom = window->frameGeometry();

    // Mouse-driven height resize within the window's column: derive its weight
    // from its final height, same model as keyboard.
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

    // Map the window's new width to the primary ratio (a no-op for pure height
    // drags inside a column).
    if (area.width() > 0 && geom.width() > 0) {
        if (m_kind == LayoutKind::Centered) {
            int masters = 0;
            int leftCount = 0;
            centeredCounts(count, masters, leftCount);
            // Centre window width is the ratio directly; a side window's width
            // is (1 - ratio)/2, so invert that.
            const qreal frac = geom.width() / area.width();
            setMasterRatio(idx < masters ? frac : (1.0 - 2.0 * frac));
        } else {
            const int masters = std::min(m_masterCount, count - 1);
            const qreal ratio = (idx < masters)
                ? geom.width() / area.width()
                : (geom.x() - area.x()) / area.width();
            setMasterRatio(ratio);
        }
    }
    return true;
}

QList<Window *> MasterStackLayoutEngine::windows() const
{
    return m_column.windows();
}

Window *MasterStackLayoutEngine::windowInDirection(Window *from, FocusDirection direction) const
{
    if (m_kind == LayoutKind::Centered) {
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
        // From any stack window, focus the master window.
        if (idx == 0) {
            return nullptr;
        }
        return ws[0];
    case FocusDirection::Right:
        // From the master, focus the top stack window.
        if (idx == 0) {
            return ws.count() > 1 ? ws[1] : nullptr;
        }
        return nullptr;
    case FocusDirection::Up:
        if (idx > 0) {
            return ws[idx - 1];
        }
        // From master, wrap to the top of the stack.
        return ws.count() > 1 ? ws[1] : nullptr;
    case FocusDirection::Down:
        if (idx < ws.count() - 1) {
            return ws[idx + 1];
        }
        // From the bottom of the stack, wrap back to master.
        return ws[0];
    }

    return nullptr;
}

Window *MasterStackLayoutEngine::windowInDirectionCentered(Window *from, FocusDirection direction) const
{
    const QList<Window *> ws = m_column.windows();
    const int count = ws.count();
    if (count == 0) {
        return nullptr;
    }
    const int idx = from ? ws.indexOf(from) : -1;
    if (idx < 0) {
        return ws.first();
    }

    int masters = 0;
    int leftCount = 0;
    centeredCounts(count, masters, leftCount);
    const int leftStart = masters;
    const int rightStart = masters + leftCount;
    const bool inCentre = idx < leftStart;
    const bool inLeft = idx >= leftStart && idx < rightStart;
    const int row = inCentre ? idx : (inLeft ? idx - leftStart : idx - rightStart);

    // Pick the window at `row` of a column [start, start+size), clamped.
    const auto at = [&ws](int start, int size, int r) -> Window * {
        if (size <= 0) {
            return nullptr;
        }
        return ws[start + std::clamp(r, 0, size - 1)];
    };

    switch (direction) {
    case FocusDirection::Left:
        if (!inCentre && !inLeft) { // right -> centre
            return at(0, masters, row);
        }
        if (inCentre) { // centre -> left
            return at(leftStart, leftCount, row);
        }
        return nullptr; // already left-most
    case FocusDirection::Right:
        if (inLeft) { // left -> centre
            return at(0, masters, row);
        }
        if (inCentre) { // centre -> right
            return at(rightStart, count - rightStart, row);
        }
        return nullptr; // already right-most
    case FocusDirection::Up:
        return (row > 0) ? ws[idx - 1] : nullptr;
    case FocusDirection::Down: {
        int first = 0;
        int last = count;
        columnRangeFor(idx, count, first, last);
        return (idx + 1 < last) ? ws[idx + 1] : nullptr;
    }
    }
    return nullptr;
}

} // namespace KWin
