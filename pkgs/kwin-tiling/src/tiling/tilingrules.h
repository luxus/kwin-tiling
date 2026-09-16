/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include "tilingstate.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

class KConfigGroup;

namespace KWin
{

class Window;

/**
 * Decides whether a window should be tiled or floated based on configured rules
 * and window properties.
 */
class KWIN_EXPORT TilingRules
{
public:
    explicit TilingRules();

    void load(const KConfigGroup &group);

    /**
     * Returns true if the window should be ignored entirely by tiling
     * (e.g. shell/panel/launcher windows). Always includes the
     * xwaylandvideobridge capture surface, even when IgnoreClass is empty.
     */
    bool isIgnored(const Window *window) const;

    /**
     * Returns true only for the xwaylandvideobridge capture surface — the one
     * window that is documented to be fully transparent. Separate from
     * isIgnored(): user IgnoreClass rules and non-client windows must never
     * be forced transparent / un-maximized.
     */
    bool isVideoBridgeSurface(const Window *window) const;

    /**
     * Returns true if the window should be forced to float.
     */
    bool isFloating(const Window *window) const;

    bool isAlwaysTiled(const Window *window) const { return matchClass(window, m_alwaysTileClasses); }

    /**
     * Returns the initial tiling mode for a newly created window.
     */
    TilingState::Mode initialMode(const Window *window) const;

    /**
     * Returns the name of the output a window's class is pinned to via the
     * [TilingRules] AssignOutput map, or an empty string when no rule matches.
     * Entries are "classPattern:outputName" (class matched like the float/ignore
     * rules; output name compared verbatim by the caller).
     */
    QString outputForWindow(const Window *window) const;

private:
    bool matchClass(const Window *window, const QStringList &patterns) const;
    bool matchTitle(const Window *window, const QStringList &patterns) const;

    QStringList m_ignoreClasses;
    QStringList m_ignoreTitles;
    QStringList m_floatingClasses;
    QStringList m_floatingTitles;
    QStringList m_alwaysTileClasses;
    // (classPattern, outputName) pairs from AssignOutput, first match wins.
    QList<QPair<QString, QString>> m_assignOutput;
    bool m_floatUtility = true;
    bool m_floatDialog = true;
    bool m_floatTransient = true;
};

} // namespace KWin
