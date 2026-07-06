/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "layoutengine.h"

#include "customtile.h"
#include "directionmath.h"
#include "tile.h"
#include "window.h" // QPointer<Window> in setZoomedWindow needs the complete type

#include <vector>

namespace KWin
{

LayoutEngine::LayoutEngine(QObject *parent)
    : QObject(parent)
{
}

LayoutEngine::~LayoutEngine() = default;

bool LayoutEngine::endResizeWindow(Window *window, const RectF &area, const RectF &startGeometry)
{
    if (!window || (area.width() <= 0 && area.height() <= 0)) {
        return false;
    }
    const auto geom = window->frameGeometry();
    const bool widthChanged = qAbs(geom.width() - startGeometry.width()) > 2.0;
    const bool heightChanged = qAbs(geom.height() - startGeometry.height()) > 2.0;
    return applyResize(window, area, widthChanged, heightChanged);
}

void LayoutEngine::takeOwnershipOfRoot(RootTile *root)
{
    if (!root) {
        return;
    }
    const QList<Tile *> existingChildren = root->childTiles();
    for (Tile *child : existingChildren) {
        if (CustomTile *custom = qobject_cast<CustomTile *>(child)) {
            root->destroyChild(custom);
        }
    }
    root->setLayoutDirection(Tile::LayoutDirection::Floating);
    root->setRelativeGeometry(RectF(0, 0, 1, 1));
}

bool LayoutEngine::reflowZoomed(const QList<CustomTile *> &allLeaves)
{
    if (!m_zoomedWindow) {
        return false;
    }
    bool present = false;
    for (CustomTile *leaf : allLeaves) {
        if (leaf && leaf->windows().contains(m_zoomedWindow)) {
            present = true;
            break;
        }
    }
    if (!present) {
        return false;
    }

    // The zoomed window fills the root; every other window is hidden.
    for (CustomTile *leaf : allLeaves) {
        if (!leaf) {
            continue;
        }
        const QList<Window *> ws = leaf->windows();
        Window *w = ws.isEmpty() ? nullptr : ws.first();
        if (w == m_zoomedWindow) {
            leaf->setRelativeGeometry(RectF(0, 0, 1, 1));
            w->setHidden(false);
        } else if (w) {
            w->setHidden(true);
        }
    }
    Q_EMIT layoutChanged();
    return true;
}

void LayoutEngine::setZoomedWindow(Window *window)
{
    if (m_zoomedWindow == window) {
        return;
    }
    m_zoomedWindow = window;
    reflow();
}

QString LayoutEngine::layoutKindToString(LayoutKind kind)
{
    switch (kind) {
    case LayoutKind::MasterStack:
        return QStringLiteral("MasterStack");
    case LayoutKind::Stacked:
        return QStringLiteral("Stacked");
    case LayoutKind::Scrolling:
        return QStringLiteral("Scrolling");
    case LayoutKind::Centered:
        return QStringLiteral("Centered");
    case LayoutKind::Grid:
        return QStringLiteral("Grid");
    }
    return QStringLiteral("MasterStack");
}

QString LayoutEngine::layoutDisplayName(LayoutKind kind)
{
    switch (kind) {
    case LayoutKind::MasterStack:
        return QStringLiteral("Master & Stack");
    case LayoutKind::Stacked:
        return QStringLiteral("Stacked");
    case LayoutKind::Scrolling:
        return QStringLiteral("Scrolling");
    case LayoutKind::Centered:
        return QStringLiteral("Centered");
    case LayoutKind::Grid:
        return QStringLiteral("Grid");
    }
    return QStringLiteral("Master & Stack");
}

LayoutEngine::LayoutKind LayoutEngine::layoutKindFromString(const QString &name, LayoutKind fallback)
{
    if (name.compare(QLatin1String("MasterStack"), Qt::CaseInsensitive) == 0) {
        return LayoutKind::MasterStack;
    }
    if (name.compare(QLatin1String("Stacked"), Qt::CaseInsensitive) == 0) {
        return LayoutKind::Stacked;
    }
    if (name.compare(QLatin1String("Scrolling"), Qt::CaseInsensitive) == 0) {
        return LayoutKind::Scrolling;
    }
    if (name.compare(QLatin1String("Centered"), Qt::CaseInsensitive) == 0) {
        return LayoutKind::Centered;
    }
    if (name.compare(QLatin1String("Grid"), Qt::CaseInsensitive) == 0) {
        return LayoutKind::Grid;
    }
    return fallback;
}

Window *LayoutEngine::windowInDirectionFromRects(const QList<QPair<Window *, RectF>> &entries,
                                                   Window *from,
                                                   FocusDirection direction) const
{
    if (entries.isEmpty()) {
        return nullptr;
    }
    if (!from) {
        return entries.first().first;
    }

    std::vector<directionmath::Rect> rects;
    rects.reserve(static_cast<size_t>(entries.size()));
    int fromIdx = -1;
    for (int i = 0; i < entries.size(); ++i) {
        const auto &[w, geom] = entries[i];
        if (w == from) {
            fromIdx = i;
        }
        rects.push_back({geom.x(), geom.y(), geom.width(), geom.height()});
    }
    if (fromIdx < 0) {
        return entries.first().first;
    }

    directionmath::Direction dir = directionmath::Direction::Left;
    switch (direction) {
    case FocusDirection::Left:
        dir = directionmath::Direction::Left;
        break;
    case FocusDirection::Right:
        dir = directionmath::Direction::Right;
        break;
    case FocusDirection::Up:
        dir = directionmath::Direction::Up;
        break;
    case FocusDirection::Down:
        dir = directionmath::Direction::Down;
        break;
    }

    const int idx = directionmath::nearestInDirection(rects, fromIdx, dir);
    return idx < 0 ? nullptr : entries[idx].first;
}

} // namespace KWin
