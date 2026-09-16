/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "classmatch.h"

#include <string_view>

// Identify the xwaylandvideobridge capture surface (ContentsWindow).
// NixOS Plasma autostarts the bridge; tiling it presents a black box.
// No Qt — unit-tested from classmatch_test.cpp.

namespace KWin::videobridge
{

inline constexpr std::string_view kClass = "xwaylandvideobridge";
inline constexpr std::string_view kDesktopClass = "org.kde.xwaylandvideobridge";
inline constexpr std::string_view kRole = "contentswindow";

inline bool matchesClass(std::string_view resourceClass, std::string_view resourceName)
{
    return classmatch::matchToken(resourceClass, kClass)
        || classmatch::matchToken(resourceName, kClass)
        || classmatch::matchToken(resourceClass, kDesktopClass)
        || classmatch::matchToken(resourceName, kDesktopClass);
}

inline bool matchesRole(std::string_view role)
{
    return classmatch::matchToken(role, kRole);
}

inline bool isVideoBridge(std::string_view resourceClass, std::string_view resourceName,
                          std::string_view role)
{
    return matchesClass(resourceClass, resourceName) || matchesRole(role);
}

} // namespace KWin::videobridge
