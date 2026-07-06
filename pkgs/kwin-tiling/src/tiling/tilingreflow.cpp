/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilingreflow.h"

#include "effect/effecthandler.h"
#include "effect/effectwindow.h"
#include "tiles/tilemanager.h"
#include "tiling/tilingcontroller.h"
#include "window.h"
#include "workspace.h"

#include <QtMath>

namespace KWin
{

namespace
{

constexpr qreal kDirectionThreshold = 2.0;

} // namespace

ReflowHint::Direction inferReflowDirection(const RectF &oldGeom, const RectF &newGeom)
{
    const qreal oldCx = oldGeom.x() + oldGeom.width() / 2.0;
    const qreal oldCy = oldGeom.y() + oldGeom.height() / 2.0;
    const qreal newCx = newGeom.x() + newGeom.width() / 2.0;
    const qreal newCy = newGeom.y() + newGeom.height() / 2.0;

    const qreal dx = newCx - oldCx;
    const qreal dy = newCy - oldCy;

    if (qAbs(dx) < kDirectionThreshold && qAbs(dy) < kDirectionThreshold) {
        return ReflowHint::Direction::None;
    }

    if (qAbs(dx) >= qAbs(dy)) {
        if (dx > 0) {
            return ReflowHint::Direction::FromRight;
        }
        if (dx < 0) {
            return ReflowHint::Direction::FromLeft;
        }
    } else {
        if (dy > 0) {
            return ReflowHint::Direction::FromBelow;
        }
        if (dy < 0) {
            return ReflowHint::Direction::FromAbove;
        }
    }
    return ReflowHint::Direction::None;
}

ReflowHint ReflowHint::build(Window *window, const RectF &target, ReflowContext &ctx)
{
    ReflowHint hint;
    hint.reason = ctx.reason;
    hint.layout = ctx.layout;
    hint.groupId = ctx.groupId;
    hint.staggerIndex = ctx.nextStaggerIndex++;
    if (window) {
        hint.direction = inferReflowDirection(window->frameGeometry(), target);
    }
    return hint;
}

QVariant ReflowHint::toVariant() const
{
    QVariantMap map;
    map.insert(QStringLiteral("reason"), static_cast<int>(reason));
    map.insert(QStringLiteral("direction"), static_cast<int>(direction));
    map.insert(QStringLiteral("layout"), static_cast<int>(layout));
    map.insert(QStringLiteral("groupId"), groupId);
    map.insert(QStringLiteral("staggerIndex"), staggerIndex);
    return map;
}

void publishTilingReflowHint(Window *window, const ReflowHint &hint)
{
    if (!window) {
        return;
    }
    if (EffectWindow *effectWindow = window->effectWindow()) {
        effectWindow->setData(WindowTilingReflowRole, hint.toVariant());
    }
}

ReflowContext &reflowContextForTile(TileManager *manager)
{
    static ReflowContext s_default;
    if (!manager) {
        return s_default;
    }
    Workspace *workspace = Workspace::self();
    if (!workspace) {
        return s_default;
    }
    TilingController *controller = workspace->tilingController();
    if (!controller) {
        return s_default;
    }
    return controller->reflowContextFor(manager->output());
}

ReflowScope::ReflowScope(TilingController *controller, LogicalOutput *output, ReflowContext::Reason reason,
                       LayoutEngine::LayoutKind layout)
    : m_controller(controller)
    , m_output(output)
{
    if (!m_controller || !m_output) {
        return;
    }
    ReflowContext ctx;
    ctx.reason = reason;
    ctx.layout = layout;
    ctx.groupId = m_controller->nextReflowGroupId();
    ctx.nextStaggerIndex = 0;
    m_controller->pushReflowContext(m_output, ctx);
    m_active = true;
}

ReflowScope::~ReflowScope()
{
    if (!m_active || !m_controller || !m_output) {
        return;
    }
    m_controller->popReflowContext(m_output);
}

} // namespace KWin