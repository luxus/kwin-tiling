/*
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone self-check for classmatch.h — run with:

        g++ -std=c++20 -O2 -Wall -Wextra -o /tmp/classmatch_test \
            pkgs/kwin-tiling/tests/classmatch_test.cpp && /tmp/classmatch_test
*/

#include "../src/tiles/classmatch.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace KWin::classmatch;

int main()
{
    // Exact match (case-insensitive).
    assert(matchToken("Code", "code"));
    assert(matchToken("org.kde.dolphin", "org.kde.dolphin"));
    assert(!matchToken("org.kde.dolphin", "dolphin")); // not substring

    // Short token must NOT match longer classes (the old contains() bug).
    assert(!matchToken("encode", "code"));
    assert(!matchToken("vscode", "code"));
    assert(!matchClassOrName("encode", "encode", {"code"}));

    // Exact class/name either-side match.
    assert(matchClassOrName("firefox", "Navigator", {"firefox"}));
    assert(matchClassOrName("Navigator", "firefox", {"firefox"}));
    assert(!matchClassOrName("chrome", "chromium", {"firefox"}));

    // Trailing * is explicit prefix wildcard.
    assert(matchToken("org.kde.dolphin", "org.kde.*"));
    assert(matchToken("code", "co*"));
    assert(!matchToken("encode", "code*")); // prefix "code" does not match "encode"
    assert(matchToken("anything", "*"));

    // Empty pattern never matches.
    assert(!matchToken("code", ""));
    assert(!matchAny("code", {}));

    std::puts("classmatch_test: OK");
    return 0;
}
