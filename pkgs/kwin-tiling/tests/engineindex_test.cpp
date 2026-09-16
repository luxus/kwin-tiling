/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for engineindex.h (Window→engine reverse index).
*/

#include "../src/tiling/engineindex.h"

#include <cassert>
#include <cstdio>

using namespace KWin::engineindex;

int main()
{
    Index idx;

    // Bind + lookup.
    idx.bind(1, 10, 100, 200);
    assert(idx.size() == 1);
    assert(idx.engineFor(1) == 10);
    const Binding *b = idx.find(1);
    assert(b);
    assert(b->engine == 10);
    assert(b->output == 100);
    assert(b->desktop == 200);
    assert(idx.find(99) == nullptr);
    assert(idx.engineFor(99) == -1);

    // Invalid ids are ignored.
    idx.bind(-1, 10, 100, 200);
    idx.bind(2, -1, 100, 200);
    assert(idx.size() == 1);

    // Re-bind is migrate (same window, new engine).
    idx.bind(1, 11, 101, 201);
    assert(idx.size() == 1);
    assert(idx.engineFor(1) == 11);
    b = idx.find(1);
    assert(b && b->output == 101 && b->desktop == 201);

    // Two windows on one engine; unbindEngine drops both, leaves others.
    idx.bind(2, 11, 101, 201);
    idx.bind(3, 12, 100, 200);
    assert(idx.size() == 3);
    idx.unbindEngine(11);
    assert(idx.size() == 1);
    assert(idx.engineFor(1) == -1);
    assert(idx.engineFor(2) == -1);
    assert(idx.engineFor(3) == 12);

    // unbindOutput drops every window on that monitor.
    idx.bind(4, 13, 100, 200);
    idx.bind(5, 14, 102, 200);
    idx.unbindOutput(100);
    assert(idx.engineFor(3) == -1);
    assert(idx.engineFor(4) == -1);
    assert(idx.engineFor(5) == 14);

    // Explicit unbind + clear.
    idx.unbind(5);
    assert(idx.find(5) == nullptr);
    idx.bind(6, 15, 103, 203);
    idx.clear();
    assert(idx.size() == 0);

    // Ghost-leaf contract: beginMove does not unbind. The window stays
    // attributed to the source engine until cancel/remove.
    idx.bind(7, 20, 1, 1);
    assert(idx.engineFor(7) == 20);
    // (no unbind here — that would be the bug: lookup would miss mid-drag)

    std::puts("engineindex_test: OK");
    return 0;
}
