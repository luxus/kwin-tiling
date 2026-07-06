/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace KWin
{

class Workspace;

/**
 * Centered layout-switch OSD for sessions without plasmashell (e.g. KWin +
 * Noctalia). Plasma sessions should use org.kde.plasmashell osdService instead.
 */
class TilingOsd
{
public:
    static void show(Workspace *workspace, const QString &text, const QString &iconName);
};

} // namespace KWin