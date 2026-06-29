/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

namespace KWin
{

class KWIN_EXPORT TilingState
{
public:
    enum class Mode {
        Floating,
        Tiled,
    };

    Mode mode = Mode::Floating;
};

} // namespace KWin
