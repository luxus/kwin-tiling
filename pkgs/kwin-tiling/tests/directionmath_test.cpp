/*
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for directionmath.h — run with:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/directionmath_test \
            pkgs/kwin-tiling/tests/directionmath_test.cpp && /tmp/directionmath_test
*/

#include "../src/tiles/directionmath.h"

#include <cassert>
#include <cstdio>

using namespace KWin::directionmath;

int main()
{
    const std::vector<Rect> grid = {
        {0.0, 0.0, 0.5, 0.5},
        {0.5, 0.0, 0.5, 0.5},
        {0.0, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
    };

    assert(nearestInDirection(grid, 0, Direction::Right) == 1);
    assert(nearestInDirection(grid, 0, Direction::Down) == 2);
    assert(nearestInDirection(grid, 1, Direction::Left) == 0);
    assert(nearestInDirection(grid, 3, Direction::Up) == 1);

    const std::vector<Rect> column = {
        {0.0, 0.0, 1.0, 0.33},
        {0.0, 0.33, 1.0, 0.33},
        {0.0, 0.66, 1.0, 0.34},
    };
    assert(nearestInDirection(column, 1, Direction::Up) == 0);
    assert(nearestInDirection(column, 1, Direction::Down) == 2);
    assert(nearestInDirection(column, 1, Direction::Left) == -1);

    const std::vector<Rect> single = {{0.0, 0.0, 1.0, 1.0}};
    assert(nearestInDirection(single, 0, Direction::Left) == -1);
    assert(nearestInDirection(single, 0, Direction::Right) == -1);
    assert(nearestInDirection(grid, 99, Direction::Right) == -1);

    std::puts("directionmath_test: OK");
    return 0;
}