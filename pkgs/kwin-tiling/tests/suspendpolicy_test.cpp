/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for suspendpolicy.h (Enabled=false / re-enable).
*/

#include "../src/tiling/suspendpolicy.h"

#include <cassert>
#include <cstdio>

using namespace KWin::suspendpolicy;

int main()
{
    // classifyEnabledChange
    assert(classifyEnabledChange(true, false) == EnabledTransition::DisableNow);
    assert(classifyEnabledChange(false, true) == EnabledTransition::EnableNow);
    assert(classifyEnabledChange(true, true) == EnabledTransition::StayEnabled);
    assert(classifyEnabledChange(false, false) == EnabledTransition::StayDisabled);

    // Suspend a tiled window → float + flag + remove + restore border.
    {
        const SuspendResult r = onSuspend({Mode::Tiled, false});
        assert(r.mode == Mode::Floating);
        assert(r.suspendedByDisable);
        assert(r.removeFromLayout);
        assert(r.restoreBorder);
    }

    // Suspend an already-floating window → no layout/border work.
    {
        const SuspendResult r = onSuspend({Mode::Floating, false});
        assert(r.mode == Mode::Floating);
        assert(!r.suspendedByDisable);
        assert(!r.removeFromLayout);
        assert(!r.restoreBorder);
    }

    // Resume suspended → tile again.
    {
        const ResumeResult r = onResume({Mode::Floating, true}, false);
        assert(r.mode == Mode::Tiled);
        assert(!r.suspendedByDisable);
        assert(r.addToLayout);
    }

    // Resume suspended but rules want float → stay floating, no add.
    {
        const ResumeResult r = onResume({Mode::Floating, true}, true);
        assert(r.mode == Mode::Floating);
        assert(!r.suspendedByDisable);
        assert(!r.addToLayout);
    }

    // Manual float (never suspended) → resume is a no-op for layout.
    {
        const ResumeResult r = onResume({Mode::Floating, false}, false);
        assert(r.mode == Mode::Floating);
        assert(!r.addToLayout);
    }

    std::puts("suspendpolicy_test: OK");
    return 0;
}
