/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "layoutengine.h"
#include "stackcolumn.h"

namespace KWin
{

class CustomTile;
class RootTile;
class Window;

/**
 * Scrolling layout (PaperWM / niri / scroll style).
 *
 * Windows live in columns placed left-to-right on a horizontal strip that may
 * be wider than the screen. The screen is a viewport that scrolls (via
 * m_scrollOffset, in view-width fractions) to keep the active column visible.
 * Columns scrolled out of the [0, 1] viewport are *hidden* (Window::setHidden),
 * not positioned past the screen edge: KWin tiles use global coordinates and a
 * window belongs to the output under its centre, so an off-screen position
 * would spill onto — and be migrated to — the adjacent monitor. Hiding keeps
 * scrolling confined to a single output (multi-monitor scrolling is not a goal).
 *
 * Each column is a StackColumn (the shared vertical-stack primitive), so the
 * height splitting, weights and resize behave exactly like the other layouts.
 * Only the horizontal placement, viewport scrolling and column consume/expel
 * are specific to this engine.
 */
class KWIN_EXPORT ScrollingLayoutEngine : public LayoutEngine
{
    Q_OBJECT

public:
    explicit ScrollingLayoutEngine(QObject *parent = nullptr);
    ~ScrollingLayoutEngine() override;

    LayoutKind layoutKind() const override { return LayoutKind::Scrolling; }
    void attach(RootTile *root) override;
    void addWindow(Window *window) override;
    void removeWindow(Window *window) override;
    void moveWindow(Window *window, int delta) override;
    void reflow() override;
    void pruneEmpty() override;

    QList<Window *> windows() const override;
    Window *windowInDirection(Window *from, FocusDirection direction) const override;
    void setActiveWindow(Window *window) override;

    // Active-column width control. Reuses the controller's master-ratio path
    // (Meta+Ctrl+L/H) to grow/shrink the focused column.
    void setPrimarySplit(qreal ratio) override;
    qreal primarySplit() const override;

    // Width given to newly opened columns (fraction of the view). Does not
    // touch existing columns, so a reconfigure won't clobber user resizes.
    void setDefaultColumnWidth(qreal width) override;

    // QoL: reset every column to the default width; centre the active column in
    // the viewport; cycle the active column through width presets.
    void resetSizes() override;
    void centerActiveColumn() override;
    void cycleColumnWidth() override;
    // niri-style: merge the active window into the column on its left, or split
    // it out into its own column to the right.
    void consumeWindow() override;
    void expelWindow() override;

    void adjustWindowHeight(Window *window, qreal delta) override;

private:
    bool applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged) override;
    struct Column
    {
        StackColumn stack;  // the vertical stack of windows in this column
        qreal width = 0.5;  // fraction of the view width
    };

    bool findWindow(Window *window, int *colIdx, int *leafIdx) const;
    int activeColumnIndex() const;
    void scrollActiveIntoView();
    QList<CustomTile *> allLeaves() const;

    QPointer<RootTile> m_root;
    QList<Column> m_columns;
    QPointer<Window> m_activeWindow;
    qreal m_scrollOffset = 0.0;   // in view-width fractions
    qreal m_defaultColWidth = 0.5;
};

} // namespace KWin
