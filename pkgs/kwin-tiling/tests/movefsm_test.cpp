/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../src/tiling/movefsm.h"

#include <cassert>
#include <cstdio>

using namespace KWin::movefsm;

int main()
{
    assert(classifyFinish({false, true, false}) == FinishKind::NotOurs);
    assert(classifyFinish({true, false, false}) == FinishKind::FloatedAway);
    // Float during cross-output drag still wins over CrossOutputDrop.
    assert(classifyFinish({true, false, true}) == FinishKind::FloatedAway);
    assert(classifyFinish({true, true, true}) == FinishKind::CrossOutputDrop);
    assert(classifyFinish({true, true, false}) == FinishKind::SameOutputDrop);
    // NotOurs ignores outputChanged (no context → no cross-output claim).
    assert(classifyFinish({false, true, true}) == FinishKind::NotOurs);

    assert(deferMigrateOnOutputChange(true, false));
    assert(deferMigrateOnOutputChange(false, true));
    assert(deferMigrateOnOutputChange(true, true));
    assert(!deferMigrateOnOutputChange(false, false));

    std::puts("movefsm_test: OK");
    return 0;
}
