/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"
#include "insertpolicy.h"
#include "kwin_export.h"
#include "viewportmath.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QPointF>

namespace KWin
{

class CustomTile;
class RootTile;
class Window;

/**
 * Abstract interface for tiling layout algorithms.
 *
 * A LayoutEngine is attached to a RootTile. It owns the creation and destruction
 * of the leaf tiles under that root, and it decides their geometries. It does not
 * touch Window objects directly; TilingController adds/removes windows to/from the
 * engine, and the engine routes the windows into Tile leaves.
 */
class KWIN_EXPORT LayoutEngine : public QObject
{
    Q_OBJECT

public:
    /**
     * Identifies a layout engine implementation. The integer value is what is
     * stored in kwinrc; append new kinds to the end and never reorder.
     */
    enum class LayoutKind {
        MasterStack = 0,
        Stacked = 1,
        Scrolling = 2,
        Centered = 3,
        Grid = 4,
        Columns = 5,
    };

    static QString layoutKindToString(LayoutKind kind);
    static QString layoutDisplayName(LayoutKind kind);
    static LayoutKind layoutKindFromString(const QString &name, LayoutKind fallback = LayoutKind::MasterStack);

    explicit LayoutEngine(QObject *parent = nullptr);
    ~LayoutEngine() override;

    /**
     * Returns the kind of layout implemented by this engine.
     */
    virtual LayoutKind layoutKind() const = 0;

    /**
     * Called once when the engine is assigned to a RootTile.
     */
    virtual void attach(RootTile *root) = 0;

    /**
     * Window lifecycle hooks.
     */
    virtual void addWindow(Window *window) = 0;
    virtual void removeWindow(Window *window) = 0;

    /**
     * Move a window by a delta in the layout order.
     * Positive delta moves forward in the order.
     *
     * StackColumn-based engines pairwise-swap (swapByDelta) so directional
     * Meta+Alt moves trade places with the neighbour. Scrolling's moveWindow
     * slides the column (Left/Right); Up/Down uses moveWindowInColumn().
     * For a rotate that shifts the windows in between, use reorderWindow().
     */
    virtual void moveWindow(Window *window, int delta) = 0;

    /**
     * Re-order a window by a delta in the layout order: the window is moved
     * to index+delta (clamped) and neighbours between the two positions shift
     * to fill the gap. Used by promoteToMaster so a stack window becomes
     * master without swapping with only the current master.
     *
     * Default forwards to moveWindow() (engines whose move is already a
     * column-step, e.g. Scrolling). StackColumn-based engines override with
     * StackColumn::moveByDelta.
     */
    virtual void reorderWindow(Window *window, int delta)
    {
        moveWindow(window, delta);
    }

    /**
     * Reorder @p window inside its column by @p delta (positive = down).
     * Default forwards to moveWindow() (single-column / layout-order engines).
     * Scrolling overrides so Meta+Alt+Up/Down does not slide the column.
     */
    virtual void moveWindowInColumn(Window *window, int delta)
    {
        moveWindow(window, delta);
    }

    /**
     * Called when an interactive move of a tiled window starts.
     * The engine should remember the window's source position so it can be
     * restored or swapped on release.
     */
    virtual void beginMoveWindow(Window *window) { Q_UNUSED(window) }

    /**
     * Called when an interactive move of a tiled window ends.
     * If @p target is non-null, the dragged @p window should take the target's
     * place and the target should take the window's original place.
     * If @p target is null, the window should be restored to its original place.
     * Returns true if the engine handled the window.
     */
    virtual bool endMoveWindow(Window *window, Window *target) { Q_UNUSED(window) Q_UNUSED(target) return false; }

    /**
     * Where on the target window an interactive drag was released.
     * Swap — middle of the target (classic trade-places).
     * InsertAbove / InsertBelow — join the target's column above or below it.
     */
    using DropZone = insertpolicy::DropZone;

    /**
     * Drop-zone variant of endMoveWindow(). Engines that do not distinguish
     * zones fall back to a swap. Returns true if the engine handled the window.
     */
    virtual bool endMoveWindowOnZone(Window *window, Window *target, DropZone zone)
    {
        Q_UNUSED(zone)
        return endMoveWindow(window, target);
    }

    /**
     * Called when a dragged tiled window was moved to a different output and
     * should be removed from this engine's layout. The engine should clean up
     * the empty source tile and reflow the remaining windows.
     */
    virtual void cancelMoveWindow(Window *window) { Q_UNUSED(window) }

    /**
     * Insert @p window into this engine at the drop location. @p target is the
     * window under the cursor (may be null), @p pos the cursor position and
     * @p area the work area, both in screen coordinates. Used for drag-drops
     * onto empty space and cross-output drops so the window lands where the
     * user dropped it instead of always appending. Default appends.
     */
    virtual void dropWindow(Window *window, Window *target, const QPointF &pos, const RectF &area)
    {
        Q_UNUSED(target)
        Q_UNUSED(pos)
        Q_UNUSED(area)
        addWindow(window);
    }

    /**
     * When true, a same-output drop onto @p target should consume/insert
     * (cancelMove + dropWindow) rather than swap (endMoveWindow). Default
     * false keeps swap-on-drop. Scrolling returns true when the target lives
     * in another column so top/bottom-half drops join that column (niri-style);
     * same-column drops still swap. Distinct from Columns InsertAbove/Below (#32).
     */
    virtual bool dropConsumesIntoTarget(Window *window, Window *target) const
    {
        Q_UNUSED(window)
        Q_UNUSED(target)
        return false;
    }

    /**
     * Recompute all tile geometries. Called after config changes, output resize,
     * or when the engine's internal order changes.
     */
    virtual void reflow() = 0;

    /**
     * Drop any leaf tiles that no longer hold a window (e.g. KWin unmanaged the
     * window from its tile when it moved to another output, leaving an empty
     * leaf) and reflow. Prevents phantom/empty tiles.
     */
    virtual void pruneEmpty() {}

    /**
     * True when this engine holds an empty (or still-occupied) source leaf for
     * @p window's in-progress interactive move. KWin untiles the window for
     * the drag, so windows() no longer contains it; a contains() guard would
     * skip removeWindow and leak a phantom tile. Default: no open move.
     */
    virtual bool ownsGhostLeaf(Window *window) const
    {
        Q_UNUSED(window)
        return false;
    }

    /**
     * Whether removeWindow should run on this engine: the window is still in
     * a leaf (contains()), or this engine owns that window's drag ghost leaf.
     * Uses contains() rather than windows().contains() so the hot path does
     * not allocate; ghost leaves are not in windows() after KWin untile-for-drag.
     */
    bool shouldHandleRemove(Window *window) const
    {
        return window && (contains(window) || ownsGhostLeaf(window));
    }

    /**
     * Returns all tiled windows managed by this engine in layout order.
     */
    virtual QList<Window *> windows() const = 0;

    /**
     * True when @p window is in a leaf of this engine. Non-allocating; prefer
     * this over windows().contains() on hot paths (focus, drop hit-test).
     */
    virtual bool contains(Window *window) const;

    /**
     * Returns the primary/master window for this layout, or nullptr if empty.
     */
    virtual Window *primaryWindow() const
    {
        const QList<Window *> ws = windows();
        return ws.isEmpty() ? nullptr : ws.first();
    }

    /**
     * Primary-split control: the master/stack ratio and master-window count.
     * No-op for layouts without a primary area (e.g. Stacked). primarySplit()
     * returns a negative value when the concept does not apply.
     */
    virtual void setPrimarySplit(qreal ratio) { Q_UNUSED(ratio) }
    virtual qreal primarySplit() const { return -1.0; }
    virtual void setPrimaryCount(int count) { Q_UNUSED(count) }
    virtual int primaryCount() const { return 1; }

    /**
     * Width of newly opened columns for viewport layouts (Scrolling), as a
     * fraction of the view. No-op for layouts without a column concept.
     */
    virtual void setDefaultColumnWidth(qreal width) { Q_UNUSED(width) }

    /**
     * Scrolling viewport policy: when to center the focused column (niri
     * center-focused-column). No-op for layouts without a viewport.
     */
    virtual void setCenterFocusedColumn(viewportmath::CenterFocusedColumn mode) { Q_UNUSED(mode) }

    /**
     * Width presets cycleColumnWidth() / cycleColumnWidthReverse() walk through
     * (fractions of the view). No-op for layouts without a column concept.
     */
    virtual void setColumnWidthPresets(const QList<qreal> &presets) { Q_UNUSED(presets) }

    /**
     * Maximum number of side-by-side columns (Columns layout). No-op otherwise.
     */
    virtual void setMaxColumns(int maxColumns) { Q_UNUSED(maxColumns) }

    /**
     * Grow (delta > 0) or shrink (delta < 0) @p window's height relative to the
     * other windows sharing its column. No-op when the column has < 2 windows or
     * the layout has no vertical sharing.
     */
    virtual void adjustWindowHeight(Window *window, qreal delta)
    {
        Q_UNUSED(window)
        Q_UNUSED(delta)
    }

    /**
     * Interactive-resize support. The user finished resizing @p window; the
     * engine reinterprets its new geometry (within work area @p area) as a
     * change to its own splits and reflows. @p startGeometry is the window's
     * frame geometry at resize-start. Returns true if it adjusted a split
     * (false lets the caller fall back to a plain reflow / snap-back).
     */
    bool endResizeWindow(Window *window, const RectF &area, const RectF &startGeometry);

    /**
     * Directional focus support. Returns the window in the requested direction
     * relative to @p from, or nullptr if there is no window in that direction.
     * If @p from is nullptr, returns the primary/first window.
     */
    enum class FocusDirection {
        Left,
        Right,
        Up,
        Down,
    };
    virtual Window *windowInDirection(Window *from, FocusDirection direction) const { Q_UNUSED(from) Q_UNUSED(direction) return nullptr; }

    /**
     * Tell the engine which window is currently active/focused. Layouts that
     * scroll a viewport (e.g. Scrolling) use this to keep the focused window's
     * column on screen. No-op for layouts where every window is always visible.
     */
    virtual void setActiveWindow(Window *window) { Q_UNUSED(window) }

    /**
     * Reset user-adjusted sizing to defaults (master ratio + per-window height
     * weights for MasterStack; column widths for Scrolling). No-op otherwise.
     */
    virtual void resetSizes() {}

    /**
     * Scrolling-only viewport actions: centre the active column, cycle the
     * active column through configured width presets (forward = next larger,
     * reverse = next smaller, wrapping), and grow the focused column into
     * unused visible space (niri expand-column-to-available-width).
     * No-op for other layouts.
     */
    virtual void centerActiveColumn() {}
    virtual void cycleColumnWidth() {}
    virtual void cycleColumnWidthReverse() {}
    virtual void expandColumnToAvailableWidth() {}

    /**
     * Scrolling: merge the active window into the column on its left
     * (consume), or split it out into its own column (expel).
     * Columns: merge into the column on the right (else left), or split out
     * a new column if under the max-column cap. No-op otherwise.
     * consumeIntoColumn / expelFromColumn are niri's named actions (pull first
     * of next; push last of focused to the right). Defaults alias consume/expel.
     * Distinct from consume-or-expel so Meta+Shift+[ ] keep their action ids.
     */
    virtual void consumeWindow() {}
    virtual void expelWindow() {}
    virtual void consumeIntoColumn() { consumeWindow(); }
    virtual void expelFromColumn() { expelWindow(); }

    /**
     * Scrolling-only niri consume-or-expel-window-left/right: solo window
     * merges into the neighbour on that side; stacked column expels the active
     * window into a new column on that side. No-op at the first/last column
     * (and on other layouts).
     */
    virtual void consumeOrExpelWindowLeft() {}
    virtual void consumeOrExpelWindowRight() {}

    /**
     * MasterStack-only: swap the master column to the other side of the screen.
     * No-op for layouts without a master area.
     */
    virtual void flipMaster() {}

    /**
     * Monocle/zoom: when set to a window managed by this engine, reflow lays
     * that window out full-screen and hides the rest; nullptr restores the
     * normal layout. The QPointer self-clears if the window closes.
     */
    void setZoomedWindow(Window *window);
    Window *zoomedWindow() const { return m_zoomedWindow; }

Q_SIGNALS:
    /**
     * Emitted when the layout geometry changes and the scene may need update.
     */
    void layoutChanged();

protected:
    /**
     * Shared monocle/zoom reflow. If a zoomed window is set and is present in
     * @p allLeaves, give it the full root, hide every other window, emit
     * layoutChanged(), and return true — the engine's reflow() should then
     * return early. Returns false when no zoom is active for these leaves, so
     * the engine proceeds with its normal layout.
     */
    bool reflowZoomed(const QList<CustomTile *> &allLeaves);

    /**
     * Take full ownership of @p root: drop any pre-existing default-layout
     * children and make it a plain floating container the engine drives.
     */
    void takeOwnershipOfRoot(RootTile *root);

    /**
     * Shared geometric direction-finder over window/relative-geometry pairs.
     */
    Window *windowInDirectionFromRects(const QList<QPair<Window *, RectF>> &entries,
                                       Window *from,
                                       FocusDirection direction) const;

    /**
     * Engine-specific resize back-solve. Called by endResizeWindow() after
     * shared guards and per-axis change detection (@p widthChanged /
     * @p heightChanged, 2px threshold vs. @p startGeometry).
     */
    virtual bool applyResize(Window *window, const RectF &area, bool widthChanged, bool heightChanged)
    {
        Q_UNUSED(window)
        Q_UNUSED(area)
        Q_UNUSED(widthChanged)
        Q_UNUSED(heightChanged)
        return false;
    }

    QPointer<Window> m_zoomedWindow;
};

} // namespace KWin
