/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// Pure column-width preset parsing and cycling for the Scrolling layout.
//
// Presets are fractions of the view (0.1–1.0). Cycle walks to the next larger
// value and wraps to the narrowest; reverse walks to the next smaller and wraps
// to the widest. Full width (1.0) is just another preset — callers that want a
// maximize-column action can setPrimarySplit(1.0) or include 1.0 in the list.
//
// No KWin / Qt types — unit-tested standalone (tests/columnwidthpresets_test.cpp).

namespace KWin::columnwidthpresets
{

inline constexpr double kMin = 0.1;
inline constexpr double kMax = 1.0;
// Same epsilon the hardcoded cycle used (p > cur + 0.01) so a column sitting
// on a preset advances instead of sticking, and float noise does not skip.
inline constexpr double kCycleEpsilon = 0.01;
inline constexpr double kUniqueEpsilon = 1e-4;

inline std::vector<double> defaults()
{
    return {1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0};
}

inline std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.remove_suffix(1);
    }
    return s;
}

inline bool parseNumber(std::string_view s, double *out)
{
    s = trim(s);
    if (s.empty() || !out) {
        return false;
    }
    // from_chars: no exceptions (KWin is built with -fno-exceptions).
    double v = 0.0;
    const char *const begin = s.data();
    const char *const end = s.data() + s.size();
    const std::from_chars_result r = std::from_chars(begin, end, v);
    if (r.ec != std::errc{}) {
        return false;
    }
    const char *p = r.ptr;
    while (p < end && (*p == ' ' || *p == '\t')) {
        ++p;
    }
    if (p != end) {
        return false;
    }
    *out = v;
    return true;
}

// One token: "0.5", "1/3", "50%", or "50" (values > 1 treated as percents).
inline bool parseToken(std::string_view tok, double *out)
{
    tok = trim(tok);
    if (tok.empty() || !out) {
        return false;
    }

    if (tok.back() == '%') {
        double pct = 0.0;
        if (!parseNumber(tok.substr(0, tok.size() - 1), &pct)) {
            return false;
        }
        *out = pct / 100.0;
        return true;
    }

    const auto slash = tok.find('/');
    if (slash != std::string_view::npos) {
        double num = 0.0;
        double den = 0.0;
        if (!parseNumber(tok.substr(0, slash), &num) || !parseNumber(tok.substr(slash + 1), &den)
            || den == 0.0) {
            return false;
        }
        *out = num / den;
        return true;
    }

    double v = 0.0;
    if (!parseNumber(tok, &v)) {
        return false;
    }
    if (v > 1.0) {
        v /= 100.0; // 25 → 0.25
    }
    *out = v;
    return true;
}

inline void appendUniqueSorted(std::vector<double> *out, double v)
{
    v = std::clamp(v, kMin, kMax);
    for (const double e : *out) {
        if (std::fabs(e - v) < kUniqueEpsilon) {
            return;
        }
    }
    out->push_back(v);
}

// Parse kcfg / kwinrc tokens. Invalid entries are skipped; an empty result
// falls back to the historical {1/3, 1/2, 2/3, 1.0} set. Comma-separated
// strings (a single kwinrc value) are split. Output is sorted ascending so
// cycle/reverse always walk by width.
inline std::vector<double> parse(const std::vector<std::string> &tokens)
{
    std::vector<double> out;
    for (const std::string &raw : tokens) {
        std::string_view rest(raw);
        while (!rest.empty()) {
            const auto comma = rest.find(',');
            const std::string_view piece = (comma == std::string_view::npos) ? rest : rest.substr(0, comma);
            double v = 0.0;
            if (parseToken(piece, &v)) {
                appendUniqueSorted(&out, v);
            }
            if (comma == std::string_view::npos) {
                break;
            }
            rest.remove_prefix(comma + 1);
        }
    }
    if (out.empty()) {
        return defaults();
    }
    std::sort(out.begin(), out.end());
    return out;
}

// direction >= 0: first preset strictly larger than current, else wrap to front.
// direction < 0: last preset strictly smaller than current, else wrap to back.
inline double cycle(double current, const std::vector<double> &presets, int direction)
{
    const std::vector<double> &p = presets.empty() ? defaults() : presets;
    if (p.size() == 1) {
        return p.front();
    }
    if (direction >= 0) {
        for (const double v : p) {
            if (v > current + kCycleEpsilon) {
                return v;
            }
        }
        return p.front();
    }
    for (int i = int(p.size()) - 1; i >= 0; --i) {
        if (p[std::size_t(i)] < current - kCycleEpsilon) {
            return p[std::size_t(i)];
        }
    }
    return p.back();
}

} // namespace KWin::columnwidthpresets
