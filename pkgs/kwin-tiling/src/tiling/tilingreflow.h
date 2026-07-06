/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"
#include "kwin_export.h"
#include "tiles/layoutengine.h"

#include <QVariant>

namespace KWin
{

class LogicalOutput;
class TilingController;
class TileManager;
class Window;

struct KWIN_EXPORT ReflowContext
{
    enum class Reason {
        Reflow,
        Add,
        Remove,
        Migrate,
        GapChange,
        LayoutSwitch,
        Retile,
        DesktopSwitch,
        Float,
        Swap,
        Insert,
        Resize,
        Reorder,
    };

    Reason reason = Reason::Reflow;
    int groupId = 0;
    int nextStaggerIndex = 0;
    LayoutEngine::LayoutKind layout = LayoutEngine::LayoutKind::MasterStack;
};

struct KWIN_EXPORT ReflowHint
{
    enum class Direction {
        None,
        FromLeft,
        FromRight,
        FromAbove,
        FromBelow,
    };

    ReflowContext::Reason reason = ReflowContext::Reason::Reflow;
    Direction direction = Direction::None;
    LayoutEngine::LayoutKind layout = LayoutEngine::LayoutKind::MasterStack;
    int groupId = 0;
    int staggerIndex = 0;

    static ReflowHint build(Window *window, const RectF &target, ReflowContext &ctx);
    QVariant toVariant() const;
};

ReflowHint::Direction inferReflowDirection(const RectF &oldGeom, const RectF &newGeom);

void publishTilingReflowHint(Window *window, const ReflowHint &hint);

/**
 * RAII guard that sets per-output reflow context on TilingController for the
 * duration of a tiling operation (add/remove/gap change/layout switch, etc.).
 */
class KWIN_EXPORT ReflowScope
{
public:
    ReflowScope(TilingController *controller, LogicalOutput *output, ReflowContext::Reason reason,
                LayoutEngine::LayoutKind layout = LayoutEngine::LayoutKind::MasterStack);
    ~ReflowScope();

    ReflowScope(const ReflowScope &) = delete;
    ReflowScope &operator=(const ReflowScope &) = delete;

private:
    TilingController *m_controller = nullptr;
    LogicalOutput *m_output = nullptr;
    bool m_active = false;
};

ReflowContext &reflowContextForTile(TileManager *manager);

} // namespace KWin