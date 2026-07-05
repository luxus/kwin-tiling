/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"
#include "kwin_export.h"

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
     */
    virtual void moveWindow(Window *window, int delta) = 0;

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
     * Returns all tiled windows managed by this engine in layout order.
     */
    virtual QList<Window *> windows() const = 0;

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

    /**
     * Width of newly opened columns for viewport layouts (Scrolling), as a
     * fraction of the view. No-op for layouts without a column concept.
     */
    virtual void setDefaultColumnWidth(qreal width) { Q_UNUSED(width) }

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
     * Scrolling-only viewport actions: centre the active column, and cycle the
     * active column through a set of width presets. No-op for other layouts.
     */
    virtual void centerActiveColumn() {}
    virtual void cycleColumnWidth() {}

    /**
     * Scrolling-only: merge the active window into the column on its left
     * (consume), or split it out into its own column (expel). No-op otherwise.
     */
    virtual void consumeWindow() {}
    virtual void expelWindow() {}

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
