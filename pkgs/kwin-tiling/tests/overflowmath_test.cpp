/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Path A go/no-go: a 0.5-wide column with 40% past the left output edge keeps
    full width, stays pinned, and is hidden from the neighbour view.
*/

#include "../src/tiles/overflowmath.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace KWin::overflowmath;

static bool approx(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    const Rect left{0.0, 0.0, 1920.0, 1080.0};
    const Rect right{1920.0, 0.0, 1920.0, 1080.0};
    const Rect outputs[] = {left, right};

    // ~40% of a 0.5-wide column past the right edge: colX = 1 - 0.6*width = 0.7
    const Rect relative{0.7, 0.0, 0.5, 1.0};
    const Rect parent{0.0, 0.0, 1.0, 1.0};

    // Without Path A, CustomTile [0,1] + parent intersect shrink the column.
    const Rect clamped = clampToParent(clampRelative(relative, false), parent, false);
    assert(approx(clamped.w, 0.3));
    assert(approx(clamped.x, 0.7));

    // With Path A, both clamps keep Column::width.
    const Rect overflowRel = clampToParent(clampRelative(relative, true), parent, true);
    assert(approx(overflowRel.x, 0.7) && approx(overflowRel.w, 0.5));

    const Rect sliver = absoluteFromRelative(left, clamped);
    const Rect full = absoluteFromRelative(left, overflowRel);
    assert(approx(sliver.w, 576.0)); // 0.3 * 1920 — resized to the visible sliver
    assert(approx(full.w, 960.0)); // 0.5 * 1920 — full column width
    assert(approx(full.x, 1344.0));
    assert(approx(full.right() - left.right(), 384.0)); // 40% of 960 past the edge

    const Rect clippedGeom = windowGeometry(full, left, false);
    const Rect overflowGeom = windowGeometry(full, left, true);
    assert(approx(clippedGeom.w, 576.0));
    assert(approx(overflowGeom.w, 960.0));
    assert(approx(overflowGeom.h, 1080.0));

    // Center of the 40%-overflow column is still on the left output (1824).
    // Pin still matters: outputsIntersecting / scene paint would include the neighbour.
    const int centerOut = outputIndexAt(outputs, 2, overflowGeom.centerX(), overflowGeom.centerY());
    assert(centerOut == 0);
    assert(overflowGeom.centerX() < 1920.0);

    const bool pin = shouldPinOutput(/*tiled=*/true, /*allowOverflow=*/true, /*interactiveMove=*/false);
    assert(pin);
    assert(!shouldPinOutput(true, true, /*interactiveMove=*/true));
    assert(!shouldPinOutput(true, /*allowOverflow=*/false, false));
    assert(!shouldPinOutput(/*tiled=*/false, true, false));

    assert(resolveOutputIndex(pin, /*manager=*/0, centerOut) == 0);

    // A column 60% off would migrate by center without the pin.
    const Rect mostlyOff = absoluteFromRelative(left, {0.8, 0.0, 0.5, 1.0});
    const int migrated = outputIndexAt(outputs, 2, mostlyOff.centerX(), mostlyOff.centerY());
    assert(migrated == 1);
    assert(resolveOutputIndex(shouldPinOutput(true, true, false), 0, migrated) == 0);

    // Neighbour view must hide the overflow window (do not paint there).
    assert(hideOnView(pin, /*manager=*/0, /*view=*/1));
    assert(!hideOnView(pin, 0, /*view=*/0));
    assert(!hideOnView(/*pin=*/false, 0, 1));

    // Input on the neighbour overflow pixels is rejected; on-output peek hits.
    assert(hitTestOnOutput(pin, overflowGeom, left, 1500.0, 100.0));
    assert(!hitTestOnOutput(pin, overflowGeom, left, 2000.0, 100.0));
    assert(containsPoint(overflowGeom, 2000.0, 100.0)); // geometry reaches the neighbour
    assert(hitTestOnOutput(/*pin=*/false, overflowGeom, left, 2000.0, 100.0));

    // isOnOutput: geometry would say both; pin says only the manager output.
    assert(intersects(overflowGeom, right));
    assert(isOnOutput(pin, 0, 0, overflowGeom, left));
    assert(!isOnOutput(pin, 0, 1, overflowGeom, right));
    assert(isOnOutput(/*pin=*/false, 0, 1, overflowGeom, right));

    std::puts("overflowmath_test: OK");
    return 0;
}
