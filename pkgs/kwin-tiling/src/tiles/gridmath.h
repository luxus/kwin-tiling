/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <vector>

// Pure geometry for a smoothly-interpolating grid layout. No KWin/Qt types —
// unit-tested standalone (tests/gridmath_test.cpp), same pattern as
// columnmath.h. Rects are relative [0,1] fractions of the work area.

namespace KWin::gridmath
{

struct Rect
{
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

struct GridShape
{
    int rows = 1;
    int cols = 1;
    int capacity = 1;
    int prevCapacity = 0;
};

inline GridShape targetShape(int n)
{
    n = std::max(n, 1);
    int prevCapacity = 0;
    for (int k = 1;; ++k) {
        const int squareCap = k * k;
        if (n <= squareCap) {
            return {k, k, squareCap, prevCapacity};
        }
        prevCapacity = squareCap;
        const int wideCap = k * (k + 1);
        if (n <= wideCap) {
            return {k, k + 1, wideCap, prevCapacity};
        }
        prevCapacity = wideCap;
    }
}

inline std::vector<Rect> cellRects(int n, const GridShape &shape)
{
    std::vector<Rect> out;
    if (n <= 0) {
        return out;
    }
    out.resize(static_cast<size_t>(n));
    if (n == 1) {
        out[0] = {0.0, 0.0, 1.0, 1.0};
        return out;
    }

    const int r = shape.rows;
    const int c = shape.cols;
    const int p = n - shape.prevCapacity;
    const int spanRows = r - p + 1;
    const double rowH = 1.0 / r;
    const double colW = 1.0 / c;

    out[0] = {0.0, 0.0, colW, spanRows * rowH};

    int idx = 1;
    for (int row = 0; row < spanRows; ++row) {
        for (int col = 1; col < c; ++col) {
            out[static_cast<size_t>(idx++)] = {col * colW, row * rowH, colW, rowH};
        }
    }
    for (int row = spanRows; row < r; ++row) {
        for (int col = 0; col < c; ++col) {
            out[static_cast<size_t>(idx++)] = {col * colW, row * rowH, colW, rowH};
        }
    }
    return out;
}

} // namespace KWin::gridmath