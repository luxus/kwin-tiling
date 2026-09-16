/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
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
 * Peeking columns (overlap [0, 1]) are placed at colX - scrollOffset with
 * stored Column::width. Path A (`Tile::m_allowOverflow` on this root) skips
 * the CustomTile [0,1] clamp and windowGeometry() output intersect, and pins
 * the window to this TileManager output so a centre past the edge does not
 * migrate. Fully off-viewport columns are still *hidden* (Window::setHidden)
 * until #41; hide is also used for monocle. CenterFocusedColumn always/on-overflow
 * can therefore peek neighbours without migrating them. Do not interpolate
 * m_scrollOffset.
 *
 * Each column is a StackColumn (the shared vertical-stack primitive), so the
 * height splitting, weights and resize behave exactly like the other layouts.
 * Only the horizontal placement, viewport scrolling, column consume/expel,
 * and drop-insert (consume into a column vs new column at cursor X) are
 * specific to this engine. Distinct from Columns InsertAbove/Below (#32).
 *
 * Directional Meta+Alt: Up/Down reorders inside the focused column
 * (`moveWindowInColumn`); Left/Right slides the whole column (`moveWindow`).
 * Consume/expel stays Meta+Shift+[ / ].
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
    void moveWindowInColumn(Window *window, int delta) override;
    void beginMoveWindow(Window *window) override;
    bool endMoveWindow(Window *window, Window *target) override;
    void cancelMoveWindow(Window *window) override;
    void dropWindow(Window *window, Window *target, const QPointF &pos, const RectF &area) override;
    bool dropConsumesIntoTarget(Window *window, Window *target) const override;
    void reflow() override;
    void pruneEmpty() override;
    bool ownsGhostLeaf(Window *window) const override;

    QList<Window *> windows() const override;
    bool contains(Window *window) const override;
    Window *windowInDirection(Window *from, FocusDirection direction) const override;
    void setActiveWindow(Window *window) override;

    // Active-column width control. Reuses the controller's master-ratio path
    // (Meta+Ctrl+L/H) to grow/shrink the focused column.
    void setPrimarySplit(qreal ratio) override;
    qreal primarySplit() const override;

    // Width given to newly opened columns (fraction of the view). Does not
    // touch existing columns, so a reconfigure won't clobber user resizes.
    void setDefaultColumnWidth(qreal width) override;
    void setCenterFocusedColumn(viewportmath::CenterFocusedColumn mode) override;

    // Cycle/reverse visit only these fractions (kcfg ColumnWidthPresets).
    void setColumnWidthPresets(const QList<qreal> &presets) override;

    // QoL: reset every column to the default width; centre the active column in
    // the viewport; cycle the active column through width presets.
    void resetSizes() override;
    void centerActiveColumn() override;
    void cycleColumnWidth() override;
    void cycleColumnWidthReverse() override;
    // niri consume-into-column / expel-from-column. consumeWindow / expelWindow
    // alias these so Meta+Shift+[ / ] keep working (KGlobalAccel).
    void consumeWindow() override;
    void expelWindow() override;
    void consumeIntoColumn() override;
    void expelFromColumn() override;

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

    void cycleColumnWidthBy(int direction);

    QPointer<RootTile> m_root;
    QList<Column> m_columns;
    QPointer<Window> m_activeWindow;
    qreal m_scrollOffset = 0.0;   // in view-width fractions
    qreal m_defaultColWidth = 0.5;
    viewportmath::CenterFocusedColumn m_centerMode = viewportmath::CenterFocusedColumn::Never;
    // Previous active column for on-overflow; -1 = none. Consumed each reflow.
    int m_focusFromColumn = -1;
    QList<qreal> m_columnWidthPresets = {1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0};
    bool m_moveHasSource = false;
    int m_moveSourceColumn = -1;
};

} // namespace KWin
