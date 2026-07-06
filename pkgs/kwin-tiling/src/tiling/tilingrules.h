/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include "tilingstate.h"

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
     * (e.g. shell/panel/launcher windows).
     */
    bool isIgnored(const Window *window) const;

    /**
     * Returns true if the window should be forced to float.
     */
    bool isFloating(const Window *window) const;

    bool isAlwaysTiled(const Window *window) const { return matchClass(window, m_alwaysTileClasses); }
    bool prefersStacked(const Window *window) const { return matchClass(window, m_stackedClasses); }

    /**
     * Returns the initial tiling mode for a newly created window.
     */
    TilingState::Mode initialMode(const Window *window) const;

private:
    bool matchClass(const Window *window, const QStringList &patterns) const;
    bool matchTitle(const Window *window, const QStringList &patterns) const;

    QStringList m_ignoreClasses;
    QStringList m_ignoreTitles;
    QStringList m_floatingClasses;
    QStringList m_floatingTitles;
    QStringList m_alwaysTileClasses;
    QStringList m_stackedClasses;
    bool m_floatUtility = true;
    bool m_floatDialog = true;
    bool m_floatTransient = true;
};

} // namespace KWin
