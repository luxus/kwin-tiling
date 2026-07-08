/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for sizingpolicy.h (MasterRatio persist gating).
*/

#include "../src/tiling/sizingpolicy.h"

#include <cassert>
#include <cstdio>

using namespace KWin::sizingpolicy;

int main()
{
    assert(!canResizePrimary(-1.0));
    assert(canResizePrimary(0.0));
    assert(canResizePrimary(0.5));

    assert(shouldPersistMasterRatio(LayoutKind::MasterStack));
    assert(shouldPersistMasterRatio(LayoutKind::Centered));
    assert(!shouldPersistMasterRatio(LayoutKind::Stacked));
    assert(!shouldPersistMasterRatio(LayoutKind::Grid));
    assert(!shouldPersistMasterRatio(LayoutKind::Scrolling));

    // MasterStack with real ratio → write.
    assert(shouldWriteMasterRatio(LayoutKind::MasterStack, 0.5));
    // Stacked has primarySplit < 0 → no write.
    assert(!shouldWriteMasterRatio(LayoutKind::Stacked, -1.0));
    // Scrolling has a positive split (column width) but must not write MasterRatio.
    assert(!shouldWriteMasterRatio(LayoutKind::Scrolling, 0.5));
    // MasterStack with non-positive split → no write.
    assert(!shouldWriteMasterRatio(LayoutKind::MasterStack, 0.0));
    assert(!shouldWriteMasterRatio(LayoutKind::MasterStack, -1.0));

    std::puts("sizingpolicy_test: OK");
    return 0;
}
