/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Pure sizing / MasterRatio persistence policy for keyboard and resize finish.
// Layout kind ints match LayoutEngine::LayoutKind (stable enum values).

namespace KWin::sizingpolicy
{

// Keep in sync with LayoutEngine::LayoutKind.
enum class LayoutKind {
    MasterStack = 0,
    Stacked = 1,
    Scrolling = 2,
    Centered = 3,
    Grid = 4,
};

/** primarySplit() < 0 means the engine has no primary split (Stacked/Grid). */
inline bool canResizePrimary(double primarySplit)
{
    return primarySplit >= 0.0;
}

/** Persist MasterRatio only for master-style layouts (not Scrolling column width). */
inline bool shouldPersistMasterRatio(LayoutKind kind)
{
    return kind == LayoutKind::MasterStack || kind == LayoutKind::Centered;
}

/**
 * After a successful endResize / resizePrimary, should we write MasterRatio?
 * Requires a positive primary split and a master-style layout kind.
 */
inline bool shouldWriteMasterRatio(LayoutKind kind, double primarySplit)
{
    return canResizePrimary(primarySplit) && shouldPersistMasterRatio(kind) && primarySplit > 0.0;
}

} // namespace KWin::sizingpolicy
