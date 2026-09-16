/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for tilingdbuspolicy.h (path/interface, register
    abort-avoidance, known layout names for setLayout).
*/

#include "../src/tiling/tilingdbuspolicy.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace KWin::tilingdbus;

int main()
{
    assert(objectPath() == "/Tiling");
    assert(interfaceName() == "org.kde.KWin.Tiling");
    assert(kMaxRegisterAttempts == 20);
    assert(kRegisterRetryMs == 100);

    // Test mode never registers (KWin unit tests, QStandardPaths test mode).
    assert(classifyRegister(true, true, false) == RegisterDecision::SkipTestMode);
    assert(classifyRegister(true, false, false) == RegisterDecision::SkipTestMode);
    assert(!shouldRetry(RegisterDecision::SkipTestMode, 0));

    // Disconnected bus: retry until the attempt cap, then give up.
    assert(classifyRegister(false, false, false) == RegisterDecision::SkipBusDisconnected);
    assert(shouldRetry(RegisterDecision::SkipBusDisconnected, 0));
    assert(shouldRetry(RegisterDecision::SkipBusDisconnected, 19));
    assert(!shouldRetry(RegisterDecision::SkipBusDisconnected, 20));

    // Occupied path: never registerObject (Qt can abort the compositor).
    assert(classifyRegister(false, true, true) == RegisterDecision::SkipAlreadyRegistered);
    assert(!shouldRetry(RegisterDecision::SkipAlreadyRegistered, 0));

    // Happy path.
    assert(classifyRegister(false, true, false) == RegisterDecision::Register);
    assert(!shouldRetry(RegisterDecision::Register, 0));

    // Occupied wins over disconnected? Disconnected is checked first so a down
    // bus with a stale local tree still retries rather than treating it as
    // "already registered".
    assert(classifyRegister(false, false, true) == RegisterDecision::SkipBusDisconnected);

    // Kinetic-shaped setLayout: only our kind names, case-insensitive.
    assert(isKnownLayoutName("MasterStack"));
    assert(isKnownLayoutName("masterstack"));
    assert(isKnownLayoutName("Stacked"));
    assert(isKnownLayoutName("Scrolling"));
    assert(isKnownLayoutName("Centered"));
    assert(isKnownLayoutName("Grid"));
    assert(isKnownLayoutName("Columns"));
    assert(!isKnownLayoutName(""));
    assert(!isKnownLayoutName("NotALayout"));
    // KineticWE extras that are not our kinds must not round-trip.
    assert(!isKnownLayoutName("CenterTile"));
    assert(!isKnownLayoutName("AutoGrid"));
    assert(!isKnownLayoutName("Monocle"));

    std::puts("tilingdbuspolicy_test: OK");
    return 0;
}
