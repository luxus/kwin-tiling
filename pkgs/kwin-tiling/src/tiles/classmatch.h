/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <string>
#include <string_view>
#include <vector>

// Exact (or trailing-*) class/title matching for tiling rules.
// Avoids accidental substring false positives (e.g. pattern "code" matching
// "encode"). No Qt — unit-tested standalone (tests/classmatch_test.cpp).

namespace KWin::classmatch
{

inline std::string toLower(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return out;
}

/**
 * Match a resource class/name/title against a pattern.
 * - Exact case-insensitive equality, or
 * - If pattern ends with '*', prefix match on the part before '*'.
 * Empty pattern never matches.
 */
inline bool matchToken(std::string_view value, std::string_view pattern)
{
    if (pattern.empty()) {
        return false;
    }
    const std::string v = toLower(value);
    const std::string p = toLower(pattern);
    if (!p.empty() && p.back() == '*') {
        const std::string_view prefix(p.data(), p.size() - 1);
        if (prefix.empty()) {
            return true; // "*" matches all
        }
        return v.size() >= prefix.size() && v.compare(0, prefix.size(), prefix) == 0;
    }
    return v == p;
}

/**
 * Match class OR name against patterns (same semantics as TilingRules::matchClass).
 */
inline bool matchClassOrName(std::string_view resourceClass, std::string_view resourceName,
                             const std::vector<std::string> &patterns)
{
    for (const std::string &p : patterns) {
        if (matchToken(resourceClass, p) || matchToken(resourceName, p)) {
            return true;
        }
    }
    return false;
}

inline bool matchAny(std::string_view value, const std::vector<std::string> &patterns)
{
    for (const std::string &p : patterns) {
        if (matchToken(value, p)) {
            return true;
        }
    }
    return false;
}

} // namespace KWin::classmatch
