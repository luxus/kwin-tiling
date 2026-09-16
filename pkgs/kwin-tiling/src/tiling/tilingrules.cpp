/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilingrules.h"
#include "tiles/classmatch.h"
#include "window.h"

#include <KConfigGroup>

#include <string_view>

namespace KWin
{

TilingRules::TilingRules()
{
}

void TilingRules::load(const KConfigGroup &group)
{
    m_ignoreClasses = group.readEntry("IgnoreClass", QStringList());
    m_ignoreTitles = group.readEntry("IgnoreTitle", QStringList());
    m_floatingClasses = group.readEntry("FloatingClass", QStringList());
    m_floatingTitles = group.readEntry("FloatingTitle", QStringList());
    m_alwaysTileClasses = group.readEntry("AlwaysTileClass", QStringList());
    m_floatUtility = group.readEntry("FloatUtility", true);
    m_floatDialog = group.readEntry("FloatDialog", true);
    m_floatTransient = group.readEntry("FloatTransient", true);

    // AssignOutput: list of "classPattern:outputName" entries pinning a window
    // class to a specific monitor (e.g. "firefox:DP-2"). The class pattern is
    // matched like the float/ignore rules; the output name is split off after
    // the first ':' and kept verbatim for the controller to resolve.
    m_assignOutput.clear();
    const QStringList assignEntries = group.readEntry("AssignOutput", QStringList());
    for (const QString &entry : assignEntries) {
        const int sep = entry.indexOf(QLatin1Char(':'));
        if (sep <= 0 || sep >= entry.size() - 1) {
            continue;
        }
        const QString pattern = entry.left(sep).trimmed().toLower();
        const QString outputName = entry.mid(sep + 1).trimmed();
        if (!pattern.isEmpty() && !outputName.isEmpty()) {
            m_assignOutput.append(qMakePair(pattern, outputName));
        }
    }

    // Normalize class patterns: strip whitespace and lower-case for case-insensitive matching.
    auto normalize = [](QStringList &list) {
        for (QString &s : list) {
            s = s.trimmed().toLower();
        }
    };
    normalize(m_ignoreClasses);
    normalize(m_ignoreTitles);
    normalize(m_floatingClasses);
    normalize(m_floatingTitles);
    normalize(m_alwaysTileClasses);
}

bool TilingRules::isIgnored(const Window *window) const
{
    if (!window || !window->isClient()) {
        return true;
    }
    if (matchClass(window, m_ignoreClasses)) {
        return true;
    }
    if (matchTitle(window, m_ignoreTitles)) {
        return true;
    }
    return false;
}

bool TilingRules::isFloating(const Window *window) const
{
    if (!window) {
        return true;
    }
    if (!window->isResizable()) {
        return true;
    }
    if (m_floatDialog && window->isDialog()) {
        return true;
    }
    if (m_floatTransient && window->isTransient()) {
        return true;
    }
    if (m_floatUtility && window->isUtility()) {
        return true;
    }
    if (window->isPopupWindow() || window->isAppletPopup()) {
        return true;
    }
    if (matchClass(window, m_floatingClasses)) {
        return true;
    }
    if (matchTitle(window, m_floatingTitles)) {
        return true;
    }
    return false;
}

TilingState::Mode TilingRules::initialMode(const Window *window) const
{
    if (isIgnored(window)) {
        return TilingState::Mode::Floating;
    }
    if (isAlwaysTiled(window)) {
        return TilingState::Mode::Tiled;
    }
    if (isFloating(window)) {
        return TilingState::Mode::Floating;
    }
    return TilingState::Mode::Tiled;
}

QString TilingRules::outputForWindow(const Window *window) const
{
    if (!window || m_assignOutput.isEmpty()) {
        return {};
    }
    for (const auto &rule : m_assignOutput) {
        if (matchClass(window, QStringList{rule.first})) {
            return rule.second;
        }
    }
    return {};
}

bool TilingRules::matchClass(const Window *window, const QStringList &patterns) const
{
    if (patterns.isEmpty() || !window) {
        return false;
    }
    // Exact match (or trailing-* prefix). Substring contains() caused false
    // positives (pattern "code" matched "encode").
    const QByteArray resourceClass = window->resourceClass().toLower().toUtf8();
    const QByteArray resourceName = window->resourceName().toLower().toUtf8();
    for (const QString &pattern : patterns) {
        const QByteArray p = pattern.toUtf8();
        if (classmatch::matchToken(std::string_view(resourceClass.constData(), size_t(resourceClass.size())),
                                   std::string_view(p.constData(), size_t(p.size())))
            || classmatch::matchToken(std::string_view(resourceName.constData(), size_t(resourceName.size())),
                                      std::string_view(p.constData(), size_t(p.size())))) {
            return true;
        }
    }
    return false;
}

bool TilingRules::matchTitle(const Window *window, const QStringList &patterns) const
{
    if (patterns.isEmpty() || !window) {
        return false;
    }
    const QByteArray title = window->caption().toLower().toUtf8();
    for (const QString &pattern : patterns) {
        const QByteArray p = pattern.toUtf8();
        if (classmatch::matchToken(std::string_view(title.constData(), size_t(title.size())),
                                   std::string_view(p.constData(), size_t(p.size())))) {
            return true;
        }
    }
    return false;
}

} // namespace KWin
