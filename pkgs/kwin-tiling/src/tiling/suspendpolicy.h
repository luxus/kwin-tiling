/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Pure [Tiling] Enabled suspend/resume decisions. TilingController applies the
// side effects; this header is the unit-tested policy (tests/suspendpolicy_test.cpp).

namespace KWin::suspendpolicy
{

enum class Mode {
    Floating,
    Tiled,
};

struct WindowFlags {
    Mode mode = Mode::Floating;
    bool suspendedByDisable = false;
};

struct SuspendResult {
    Mode mode = Mode::Floating;
    bool suspendedByDisable = true;
    bool removeFromLayout = true;
    bool restoreBorder = true;
};

struct ResumeResult {
    Mode mode = Mode::Floating;
    bool suspendedByDisable = false;
    bool addToLayout = false;
};

/** Live Enabled=false for one currently-tiled window. */
inline SuspendResult onSuspend(WindowFlags in)
{
    SuspendResult out;
    if (in.mode != Mode::Tiled) {
        out.mode = in.mode;
        out.suspendedByDisable = in.suspendedByDisable;
        out.removeFromLayout = false;
        out.restoreBorder = false;
        return out;
    }
    out.mode = Mode::Floating;
    out.suspendedByDisable = true;
    out.removeFromLayout = true;
    out.restoreBorder = true;
    return out;
}

/**
 * Live Enabled=true resume for a window that was suspended by disable.
 * @p rulesWantFloat: TilingRules::initialMode would be Floating (utility, etc.).
 * Manual floats (never suspended) are skipped by the caller via suspendedByDisable.
 */
inline ResumeResult onResume(WindowFlags in, bool rulesWantFloat)
{
    ResumeResult out;
    if (!in.suspendedByDisable) {
        out.mode = in.mode;
        out.suspendedByDisable = false;
        out.addToLayout = false;
        return out;
    }
    out.suspendedByDisable = false;
    if (rulesWantFloat) {
        out.mode = Mode::Floating;
        out.addToLayout = false;
        return out;
    }
    out.mode = Mode::Tiled;
    out.addToLayout = true;
    return out;
}

/** Whether reconfigure should suspend-all vs resume-suspended vs neither. */
enum class EnabledTransition {
    StayDisabled,
    StayEnabled,
    DisableNow, // was on → off
    EnableNow,  // was off → on
};

inline EnabledTransition classifyEnabledChange(bool wasEnabled, bool nowEnabled)
{
    if (wasEnabled && !nowEnabled) {
        return EnabledTransition::DisableNow;
    }
    if (!wasEnabled && nowEnabled) {
        return EnabledTransition::EnableNow;
    }
    if (nowEnabled) {
        return EnabledTransition::StayEnabled;
    }
    return EnabledTransition::StayDisabled;
}

} // namespace KWin::suspendpolicy
