/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Pure interactive-move FSM decisions for TilingController.
// Keeps finish/output-leave branching testable without Workspace.
// (Full controller split deferred until a larger lifecycle feature.)

namespace KWin::movefsm
{

enum class Phase {
    Idle,
    Moving,
    Resizing,
};

enum class FinishKind {
    NotOurs,           // no active move context
    FloatedAway,       // user floated during drag
    CrossOutputDrop,   // left original output
    SameOutputDrop,    // same output; engine handles swap/insert
};

struct MoveOpen {
    bool hasContext = false;
    bool stillTiled = true;
    bool outputChanged = false;
};

inline FinishKind classifyFinish(const MoveOpen &m)
{
    if (!m.hasContext) {
        return FinishKind::NotOurs;
    }
    if (!m.stillTiled) {
        return FinishKind::FloatedAway;
    }
    if (m.outputChanged) {
        return FinishKind::CrossOutputDrop;
    }
    return FinishKind::SameOutputDrop;
}

/** During outputChanged: defer destination placement while drag is open. */
inline bool deferMigrateOnOutputChange(bool moveOpen, bool resizeOpen)
{
    return moveOpen || resizeOpen;
}

} // namespace KWin::movefsm
