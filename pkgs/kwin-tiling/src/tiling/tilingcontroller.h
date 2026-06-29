/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include "tilingrules.h"
#include "tilingstate.h"
#include "tiles/layoutengine.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

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
    void moveWindowToOutput(TilingDirection direction);
    void moveWindowNext();
    void moveWindowPrevious();

    // #118 directional move/swap within layout (ponytail: reuse windowInDirection + move)
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

    TilingRules *rules() const { return m_rules.get(); }

private Q_SLOTS:
    void onInteractiveMoveResizeStarted();
    void onInteractiveMoveResizeFinished();
    void onWindowDesktopsChanged(Window *window);

private:
    bool shouldTile(const Window *window) const;
    void onWindowMoveFinished(Window *window);
    void onWindowResizeFinished(Window *window);
    void onWindowOutputChanged(Window *window, LogicalOutput *oldOutput);
    // A minimized tiled window leaves its layout (siblings reflow to fill) and
    // re-tiles on restore. Without this the minimized window keeps its slot.
    void onWindowMinimizedChanged(Window *window);
    void focusInDirection(LayoutEngine::FocusDirection direction);
    void moveInDirection(LayoutEngine::FocusDirection direction);
    // The first tiled window on the output adjacent to the active window in
    // @p direction, or nullptr at the edge of the screen arrangement. Used so
    // directional focus continues onto the next monitor.
    Window *windowOnAdjacentOutput(LayoutEngine::FocusDirection direction) const;
    Window *windowUnderCursorInEngine(LayoutEngine *engine) const;
    void addWindowToLayout(Window *window, LogicalOutput *output, VirtualDesktop *desktop);
    void removeWindowFromLayouts(Window *window);
    // Keep floating windows stacked above tiled ones (when FloatAbove is set).
    void applyFloatStacking(Window *window);
    void migrateWindow(Window *window, LogicalOutput *newOutput, VirtualDesktop *newDesktop);
    LayoutEngine *activeLayoutEngine() const;
    LayoutEngine *layoutEngineForWindow(Window *window, LogicalOutput **output = nullptr, VirtualDesktop **desktop = nullptr) const;
    Window *activeTiledWindow() const;

    void setupLayoutEngine(TileManager *manager, VirtualDesktop *desktop, LayoutEngine::LayoutKind kind);
    // Seed a (new or live) engine's sizing from config: master ratio/count for
    // MasterStack, default column width for Scrolling. Keeps the MasterRatio
    // and DefaultColumnWidth settings from clobbering each other.
    void seedEngineSizing(LayoutEngine *engine, LayoutEngine::LayoutKind kind);
    LayoutEngine::LayoutKind resolveLayoutKind(LogicalOutput *output) const;
    // Per-(output, desktop) layout: a remembered manual choice (see
    // persistLayoutChoice) wins over resolveLayoutKind's config default, so a
    // Cycle/Switch survives a reconfigure and restart.
    LayoutEngine::LayoutKind layoutKindFor(LogicalOutput *output, VirtualDesktop *desktop) const;
    void persistLayoutChoice(LogicalOutput *output, VirtualDesktop *desktop, LayoutEngine::LayoutKind kind);
    LayoutEngine::LayoutKind globalDefaultLayoutKind() const;
    QList<LayoutEngine::LayoutKind> enabledLayoutKinds() const;
    bool isLayoutEnabled(LayoutEngine::LayoutKind kind) const;
    void applyGapSettingsToOutput(LogicalOutput *output);

    void setLayoutOn(LogicalOutput *output, VirtualDesktop *desktop, LayoutEngine::LayoutKind kind);
    void reconcileLayoutKinds();

    QPointer<Workspace> m_workspace;
    std::unique_ptr<TilingRules> m_rules;
    bool m_enabled = true;
    LayoutEngine::LayoutKind m_defaultLayout = LayoutEngine::LayoutKind::MasterStack;
    QStringList m_enabledLayouts;
    qreal m_masterRatio = 0.5;
    qreal m_defaultColumnWidth = 0.5;
    int m_masterCount = 1;
    bool m_floatAbove = true;
    // Live "gaps off" toggle (toggleGaps); transient, resets on restart.
    bool m_gapsSuppressed = false;

    struct MoveContext {
        QPointer<LayoutEngine> engine;
        QPointer<LogicalOutput> output;
        RectF originalGeometryRestore;
    };
    QHash<Window *, MoveContext> m_activeMoves;
    // Windows with an interactive resize in flight (distinguishes resize from
    // move on the shared interactiveMoveResizeFinished signal).
    QSet<Window *> m_activeResizes;
};

} // namespace KWin
