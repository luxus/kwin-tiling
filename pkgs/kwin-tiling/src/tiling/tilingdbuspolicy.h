/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "tilingconfig.h"

#include <string_view>

// Pure D-Bus registration policy for org.kde.KWin.Tiling at /Tiling.
//
// Qt can abort the compositor if registerObject() is called on a path that
// is already claimed. Classify first; never retry a failed registerObject.
// Unit-tested in tests/tilingdbuspolicy_test.cpp (no Qt/KWin).

namespace KWin::tilingdbus
{

inline constexpr std::string_view objectPath()
{
    return "/Tiling";
}

inline constexpr std::string_view interfaceName()
{
    return "org.kde.KWin.Tiling";
}

// First attempt is deferred off Workspace::init (singleShot 0); further
// retries only apply while the session bus is still disconnected.
inline constexpr int kMaxRegisterAttempts = 20;
inline constexpr int kRegisterRetryMs = 100;

enum class RegisterDecision {
    Register,
    SkipTestMode,
    SkipAlreadyRegistered,
    SkipBusDisconnected,
};

inline RegisterDecision classifyRegister(bool testMode, bool busConnected, bool pathOccupied)
{
    if (testMode) {
        return RegisterDecision::SkipTestMode;
    }
    if (!busConnected) {
        return RegisterDecision::SkipBusDisconnected;
    }
    if (pathOccupied) {
        return RegisterDecision::SkipAlreadyRegistered;
    }
    return RegisterDecision::Register;
}

inline bool shouldRetry(RegisterDecision decision, int attempts)
{
    return decision == RegisterDecision::SkipBusDisconnected && attempts < kMaxRegisterAttempts;
}

/** Reject unknown kind strings so setLayout does not silently map to MasterStack. */
inline bool isKnownLayoutName(std::string_view name)
{
    return tilingconfig::parseLayoutKind(name).has_value();
}

} // namespace KWin::tilingdbus
