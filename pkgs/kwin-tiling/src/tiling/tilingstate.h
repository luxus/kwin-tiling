/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
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
    bool borderForced = false;
    bool originalNoBorder = false;
    // Set when [Tiling] Enabled went false while this window was tiled; used so
    // a live re-enable can re-tile only those windows (not manual floats).
    bool suspendedByDisable = false;
};

} // namespace KWin
