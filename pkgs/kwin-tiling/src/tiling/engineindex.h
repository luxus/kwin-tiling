/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <unordered_map>

// Window→engine reverse index: O(1) lookup instead of scanning every
// (output, desktop) engine. TilingController stores Window*/LayoutEngine* in
// a QHash of the same shape; this header is the unit-tested contract
// (bind/unbind/migrate/engine-replace/output-remove) using integer ids.
//
// A window is in at most one engine. Interactive-move "ghost" leaves do not
// unbind — the window still belongs to the source engine until cancel/remove.

namespace KWin::engineindex
{

struct Binding {
    int engine = -1;
    int output = -1;
    int desktop = -1;
};

class Index
{
public:
    void bind(int window, int engine, int output, int desktop)
    {
        if (window < 0 || engine < 0) {
            return;
        }
        m_map[window] = Binding{engine, output, desktop};
    }

    void unbind(int window)
    {
        m_map.erase(window);
    }

    // Drop every window recorded against @p engine (layout switch / retile
    // replaces the engine object).
    void unbindEngine(int engine)
    {
        for (auto it = m_map.begin(); it != m_map.end();) {
            if (it->second.engine == engine) {
                it = m_map.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Drop every window recorded against @p output (monitor unplug).
    void unbindOutput(int output)
    {
        for (auto it = m_map.begin(); it != m_map.end();) {
            if (it->second.output == output) {
                it = m_map.erase(it);
            } else {
                ++it;
            }
        }
    }

    const Binding *find(int window) const
    {
        const auto it = m_map.find(window);
        return it == m_map.end() ? nullptr : &it->second;
    }

    int engineFor(int window) const
    {
        const Binding *b = find(window);
        return b ? b->engine : -1;
    }

    void clear()
    {
        m_map.clear();
    }

    std::size_t size() const
    {
        return m_map.size();
    }

private:
    std::unordered_map<int, Binding> m_map;
};

} // namespace KWin::engineindex
