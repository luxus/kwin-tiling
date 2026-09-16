/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include "tilingreflow.h"
#include "tilingrules.h"
#include "tilingstate.h"
#include "tiles/layoutengine.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

class KConfigGroup;

namespace KWin
{

class LayoutEngine;
class LogicalOutput;
class TileManager;
class VirtualDesktop;
class Window;
class Workspace;

/**
 * Singleton controller that manages the native tiling state.
 *
 * It decides which windows tile, reacts to window creation/destruction,
 * and handles keyboard-driven tiling commands.
 */
class KWIN_EXPORT TilingController : public QObject
{
    Q_OBJECT

public:
    explicit TilingController(Workspace *workspace);
    ~TilingController() override;

    /**
     * Called by Workspace when a window is added.
     */
    void onWindowAdded(Window *window);

    /**
     * Called by Workspace when a window is removed.
     */
    void onWindowRemoved(Window *window);

    /**
     * Re-read configuration from kwinrc.
     */
    void reconfigure();
    bool isEnabled() const { return m_enabled; }

    /**
     * Initialize default layout engines for all existing outputs/desktops.
     */
    void initializeLayouts();

    /**
     * Initialize default layout engines for a newly added output.
     */
    void onOutputAdded(LogicalOutput *output);

    enum class TilingDirection {
        West,
        East,
        North,
        South,
    };

    // Keyboard actions
    void focusLeft();
    void focusRight();
    void focusUp();
    void focusDown();
    void toggleFloating();
    void promoteToMaster();
    void toggleMasterPin();
    void moveWindowToOutput(TilingDirection direction);
    void moveWindowNext();
    void moveWindowPrevious();

    // Directional move/swap within layout (reuses windowInDirection + moveWindow).
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();

    // Resize the master/stack split of the active engine by @p delta (e.g.
    // +/-0.05), and change the master-window count by @p delta. Both persist
    // the new value to [Tiling] in kwinrc.
    void resizePrimary(qreal delta);
    void adjustMasterCount(int delta);

    // Grow (delta > 0) / shrink (delta < 0) the active window's height within
    // its column (master or stack). Transient — not persisted.
    void resizeActiveWindowHeight(qreal delta);

    // QoL actions: reset sizing to defaults; Scrolling centre/width-preset; and
    // monocle (zoom the active window full-screen, toggle to restore).
    void resetSizes();
    void centerColumn();
    void cycleColumnWidth();
    void toggleZoom();
    // Scrolling: consume/expel the active window into/out of a column.
    void consumeWindow();
    void expelWindow();
    // MasterStack: swap the master column side. All layouts: toggle gaps on/off.
    void flipMaster();
    void toggleGaps();

    // Float/tile a specific window (used by the window context menu).
    void setFloating(Window *window, bool floating);
    // Add/remove the window's class to the [TilingRules] FloatingClass list
    // (persisted) and apply to all open windows of that class.
    void setFloatAppRule(Window *window, bool floatApp);
    bool isFloatAppRule(const Window *window) const;

    /**
     * Switch the layout of the active monitor's current desktop to @p kind,
     * rebuilding the engine in place and re-adding the windows it was
     * managing so the visual change is immediate.
     */
    void setLayout(LayoutEngine::LayoutKind kind);

    /**
     * Cycle the active monitor's current desktop through the list of layouts
     * currently enabled in kwinrc.
     */
    void cycleLayout();

    // Rebuild the active output+desktop layout from scratch and re-add the
    // windows that belong on it. Manual recovery from a desynced/phantom state.
    void retile();

    // Toggle focus back to the previously-active window (any window, tiled or floating).
    void focusLast();

    TilingRules *rules() const { return m_rules.get(); }

    ReflowContext &reflowContextFor(LogicalOutput *output);
    ReflowContext reflowContextFor(LogicalOutput *output) const;
    void pushReflowContext(LogicalOutput *output, const ReflowContext &ctx);
    void popReflowContext(LogicalOutput *output);
    int nextReflowGroupId();

private Q_SLOTS:
    void onInteractiveMoveResizeStarted();
    void onInteractiveMoveResizeFinished();
    void onWindowDesktopsChanged(Window *window);
    // A monitor was unplugged: drop per-output state keyed by that raw
    // LogicalOutput* so a later reused address can't return a stale context.
    void onOutputRemoved(LogicalOutput *output);

private:
    bool shouldTile(const Window *window) const;
    void onWindowMoveFinished(Window *window);
    void onWindowResizeFinished(Window *window, const RectF &startGeometry);
    void onWindowOutputChanged(Window *window, LogicalOutput *oldOutput);
    // A minimized tiled window leaves its layout (siblings reflow to fill) and
    // re-tiles on restore. Without this the minimized window keeps its slot.
    void onWindowMinimizedChanged(Window *window);
    // Same ghost-tile leave/rejoin as minimize, for maximize. Fullscreen is
    // left attached (core KWin does not forget the tile on that path).
    void onWindowMaximizedChanged(Window *window);
    // Re-evaluate ignore rules when WM_CLASS arrives late (XWayland).
    void onWindowClassChanged(Window *window);
    // Pin the xwaylandvideobridge capture surface to opacity 0 and undo
    // maximize so it cannot present as an opaque black box. No-op for every
    // other window. Idempotent (setOpacity/setMaximize no-op on no change).
    void sanitizeVideoBridgeSurface(Window *window);
    // Shared leave/rejoin used by minimize and maximize. vacate always
    // remove+prune on the home engine (maximize may already have forgotten
    // the leaf, so shouldHandleRemove is false and we must not walk others).
    void vacateLayout(Window *window);
    void rejoinLayout(Window *window);
    void focusInDirection(LayoutEngine::FocusDirection direction);
    void moveInDirection(LayoutEngine::FocusDirection direction);
    // The first tiled window on the output adjacent to the active window in
    // @p direction, or nullptr at the edge of the screen arrangement. Used so
    // directional focus continues onto the next monitor.
    Window *windowOnAdjacentOutput(LayoutEngine::FocusDirection direction) const;
    Window *windowUnderCursorInEngine(LayoutEngine *engine) const;
    void addWindowToLayout(Window *window, LogicalOutput *output, VirtualDesktop *desktop);
    void removeWindowFromLayouts(Window *window);
    void forceNoBorder(Window *window);
    void restoreBorder(Window *window);
    void migrateWindow(Window *window, LogicalOutput *newOutput, VirtualDesktop *newDesktop);
    bool promoteToMaster(Window *window);
    QString pinKeyFor(LogicalOutput *output, VirtualDesktop *desktop) const;
    void reassertMasterPin(LogicalOutput *output, VirtualDesktop *desktop);
    LayoutEngine *activeLayoutEngine() const;
    // The connected output whose name matches @p name (case-insensitive), or
    // nullptr. Used to honour the [TilingRules] AssignOutput per-app rule.
    LogicalOutput *outputByName(const QString &name) const;
    LayoutEngine *layoutEngineForWindow(Window *window, LogicalOutput **output = nullptr, VirtualDesktop **desktop = nullptr) const;
    Window *activeTiledWindow() const;

    void setupLayoutEngine(LogicalOutput *output, TileManager *manager, VirtualDesktop *desktop,
                           LayoutEngine::LayoutKind kind);
    // Seed a (new or live) engine's sizing from config: master ratio/count for
    // MasterStack, default column width for Scrolling. Per-output overrides
    // in [Tiling][Output name] win over the global [Tiling] defaults.
    void seedEngineSizing(LogicalOutput *output, LayoutEngine *engine, LayoutEngine::LayoutKind kind);
    LayoutEngine::LayoutKind resolveLayoutKind(LogicalOutput *output, VirtualDesktop *desktop = nullptr) const;
    // Per-(output, desktop) layout: a remembered manual choice (see
    // persistLayoutChoice) wins over resolveLayoutKind's config default, so a
    // Cycle/Switch survives a reconfigure and restart. Both this and
    // resolveLayoutKind read the cache filled by loadConfigCache(), not KConfig.
    LayoutEngine::LayoutKind layoutKindFor(LogicalOutput *output, VirtualDesktop *desktop) const;
    void persistLayoutChoice(LogicalOutput *output, VirtualDesktop *desktop, LayoutEngine::LayoutKind kind);
    LayoutEngine::LayoutKind globalDefaultLayoutKind() const;
    QList<LayoutEngine::LayoutKind> enabledLayoutKinds() const;
    bool isLayoutEnabled(LayoutEngine::LayoutKind kind) const;
    // Apply cached gap settings. When @p desktop is null, every desktop on the
    // output is updated (reconfigure / toggleGaps / new output). Window
    // add/remove/migrate pass the affected desktop so sibling desktops are not
    // reflowed.
    void applyGapSettingsToOutput(LogicalOutput *output, VirtualDesktop *desktop = nullptr);
    // Snapshot [Tiling] layout/gap entries so the hot path never reopens kwinrc.
    void loadConfigCache(const KConfigGroup &tilingGroup);

    void setLayoutOn(LogicalOutput *output, VirtualDesktop *desktop, LayoutEngine::LayoutKind kind);
    void reconcileLayoutKinds();
    void showLayoutNotification(LayoutEngine::LayoutKind kind);
    LayoutEngine::LayoutKind reflowScopeLayoutKind(LogicalOutput *output,
                                                   VirtualDesktop *desktop = nullptr) const;
    // Live [Tiling] Enabled=false / re-enable: detach or restore tiled windows.
    void suspendAllTiledWindows();
    void resumeSuspendedWindows();

    // Coalesce kwinrc writes from interactive resize / master ratio+count edits:
    // callers writeEntry() then schedule this instead of a blocking sync() per
    // keypress; the shared config is flushed once the drag/keypress storm settles.
    void schedulePersist();

    QPointer<Workspace> m_workspace;
    std::unique_ptr<TilingRules> m_rules;
    bool m_enabled = true;
    LayoutEngine::LayoutKind m_defaultLayout = LayoutEngine::LayoutKind::MasterStack;
    // Parsed EnabledLayouts (cycle order), rebuilt in loadConfigCache().
    QList<LayoutEngine::LayoutKind> m_enabledLayoutKinds;
    qreal m_masterRatio = 0.5;
    qreal m_defaultColumnWidth = 0.5;
    int m_masterCount = 1;
    bool m_layoutSwitchOsd = true;
    bool m_borderlessWhenTiled = false;
    // [Tiling] NewWindowPlacement: when true ("master"), a newly tiled window is
    // promoted to master on open; default false ("end") appends it (unchanged).
    bool m_newWindowMaster = false;
    // Live "gaps off" toggle (toggleGaps); transient, resets on restart.
    bool m_gapsSuppressed = false;

    // Config cache filled by loadConfigCache() / persistLayoutChoice().
    struct CachedGaps {
        qreal gapBetween = 0.0;
        int gapLeft = 0;
        int gapRight = 0;
        int gapTop = 0;
        int gapBottom = 0;
    };
    CachedGaps m_gapDefaults;
    QHash<QString, CachedGaps> m_outputGaps; // output name → merged gaps
    QHash<QString, LayoutEngine::LayoutKind> m_outputDefaultLayouts; // output name
    QHash<QString, LayoutEngine::LayoutKind> m_desktopOutputLayouts; // "N:output"
    QHash<QString, LayoutEngine::LayoutKind> m_desktopLayoutMemory; // "output/desktopId"

    struct MoveContext {
        QPointer<LayoutEngine> engine;
        QPointer<LogicalOutput> output;
        RectF originalGeometryRestore;
    };
    QHash<Window *, MoveContext> m_activeMoves;
    // Windows with an interactive resize in flight (distinguishes resize from
    // move on the shared interactiveMoveResizeFinished signal).
    QHash<Window *, RectF> m_activeResizes;
    QPointer<Window> m_lastFocused;
    QPointer<Window> m_prevFocused;
    QHash<QString, QPointer<Window>> m_masterPins;
    QHash<LogicalOutput *, QVector<ReflowContext>> m_reflowContextStacks;
    int m_nextReflowGroupId = 1;
    // Debounces kwinrc sync() on the interactive sizing paths (see schedulePersist).
    QTimer *m_persistTimer = nullptr;
    // Geometry immediately before the engine snapped the window into a tile.
    // Restored in onWindowClassChanged() for ignored system surfaces whose
    // WM_CLASS arrived after they were already adopted. Cleared on remove.
    QHash<Window *, RectF> m_preTileGeometry;
};

} // namespace KWin
