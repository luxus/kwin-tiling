/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilingcontroller.h"

#include "core/output.h"
#include "tiling/tilingreflow.h"
#include "core/rect.h"
#include "cursor.h"
#include "tiling/tilingosd.h"
#include "tiles/layoutengine.h"
#include "tiles/gridlayoutengine.h"
#include "tiles/masterstacklayoutengine.h"
#include "tiles/scrollinglayoutengine.h"
#include "tiles/stackedlayoutengine.h"
#include "tiles/tilemanager.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QStandardPaths>
#include <QtGlobal>

namespace KWin
{

namespace
{

std::unique_ptr<LayoutEngine> createLayoutEngine(LayoutEngine::LayoutKind kind, QObject *parent)
{
    switch (kind) {
    case LayoutEngine::LayoutKind::Stacked:
        return std::make_unique<StackedLayoutEngine>(parent);
    case LayoutEngine::LayoutKind::Centered:
        // Master-stack engine in centered mode: master centred, stack split
        // into a left and a right column.
        return std::make_unique<MasterStackLayoutEngine>(parent, LayoutEngine::LayoutKind::Centered);
    case LayoutEngine::LayoutKind::Scrolling:
        return std::make_unique<ScrollingLayoutEngine>(parent);
    case LayoutEngine::LayoutKind::Grid:
        return std::make_unique<GridLayoutEngine>(parent);
    case LayoutEngine::LayoutKind::MasterStack:
    default:
        return std::make_unique<MasterStackLayoutEngine>(parent);
    }
}

Workspace::Direction toWorkspaceDirection(LayoutEngine::FocusDirection direction)
{
    switch (direction) {
    case LayoutEngine::FocusDirection::Left:
        return Workspace::DirectionWest;
    case LayoutEngine::FocusDirection::Right:
        return Workspace::DirectionEast;
    case LayoutEngine::FocusDirection::Up:
        return Workspace::DirectionNorth;
    case LayoutEngine::FocusDirection::Down:
        return Workspace::DirectionSouth;
    }
    return Workspace::DirectionEast;
}

struct OutputSizing {
    qreal masterRatio;
    int masterCount;
    qreal defaultColumnWidth;
};

OutputSizing readOutputSizing(const KConfigGroup &tilingGroup, LogicalOutput *output)
{
    OutputSizing sizing{
        qBound(0.1, tilingGroup.readEntry("MasterRatio", 0.5), 0.9),
        qMax(1, tilingGroup.readEntry("MasterCount", 1)),
        qBound(0.1, tilingGroup.readEntry("DefaultColumnWidth", 0.5), 1.0),
    };
    if (!output) {
        return sizing;
    }
    const KConfigGroup outputGroup(&tilingGroup, QStringLiteral("Output %1").arg(output->name()));
    if (!outputGroup.exists()) {
        return sizing;
    }
    sizing.masterRatio = qBound(0.1, outputGroup.readEntry("MasterRatio", sizing.masterRatio), 0.9);
    sizing.masterCount = qMax(1, outputGroup.readEntry("MasterCount", sizing.masterCount));
    sizing.defaultColumnWidth = qBound(0.1, outputGroup.readEntry("DefaultColumnWidth", sizing.defaultColumnWidth), 1.0);
    return sizing;
}

KConfigGroup sizingWriteGroup(KConfigGroup &tilingGroup, LogicalOutput *output)
{
    if (output) {
        KConfigGroup outputGroup(&tilingGroup, QStringLiteral("Output %1").arg(output->name()));
        if (outputGroup.exists()) {
            return outputGroup;
        }
    }
    return tilingGroup;
}

} // namespace

TilingController::TilingController(Workspace *workspace)
    : QObject(workspace)
    , m_workspace(workspace)
    , m_rules(std::make_unique<TilingRules>())
{
    // Let viewport layouts (Scrolling) track the focused window so they can
    // keep its column on screen. No-op for the non-scrolling engines.
    if (m_workspace) {
        connect(m_workspace, &Workspace::windowActivated, this, [this](Window *window) {
            if (!window) {
                return;
            }
            if (window != m_lastFocused) {
                m_prevFocused = m_lastFocused;
                m_lastFocused = window;
            }
            if (LayoutEngine *engine = layoutEngineForWindow(window)) {
                engine->setActiveWindow(window);
            }
        });
        m_lastFocused = m_workspace->activeWindow();
    }
    reconfigure();
}

TilingController::~TilingController() = default;

void TilingController::reconfigure()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    KConfigGroup rulesGroup(config, QStringLiteral("TilingRules"));

    m_enabled = tilingGroup.readEntry("Enabled", true);
    m_defaultLayout = LayoutEngine::layoutKindFromString(
        tilingGroup.readEntry("DefaultLayout", QStringLiteral("MasterStack")));
    // Whitelist of layouts the user wants available. Order in the list also
    // defines the cycle order used by cycleLayout().
    m_enabledLayouts = tilingGroup.readEntry("EnabledLayouts",
        QStringList{QLatin1String("MasterStack"), QLatin1String("Stacked"), QLatin1String("Scrolling"), QLatin1String("Centered")});
    m_masterRatio = qBound(0.1, tilingGroup.readEntry("MasterRatio", 0.5), 0.9);
    m_defaultColumnWidth = qBound(0.1, tilingGroup.readEntry("DefaultColumnWidth", 0.5), 1.0);
    m_masterCount = qMax(1, tilingGroup.readEntry("MasterCount", 1));
    m_floatAbove = tilingGroup.readEntry("FloatAbove", true);
    m_layoutSwitchOsd = tilingGroup.readEntry("LayoutSwitchOsd", true);
    m_borderlessWhenTiled = tilingGroup.readEntry("BorderlessWhenTiled", false);
    m_rules->load(rulesGroup);

    // Push master count/ratio to live engines so changes in the KCM (or
    // direct kwinrc edit + reloadConfig) take effect immediately without
    // logout/restart. setPrimary* triggers reflow in MasterStack.
    if (m_workspace) {
        for (LogicalOutput *output : m_workspace->outputs()) {
            if (TileManager *manager = m_workspace->tileManager(output)) {
                for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
                    if (LayoutEngine *eng = manager->layoutEngine(desktop)) {
                        seedEngineSizing(output, eng, eng->layoutKind());
                    }
                }
            }
        }
    }

    initializeLayouts();
    reconcileLayoutKinds();

    if (m_workspace) {
        for (LogicalOutput *output : m_workspace->outputs()) {
            applyGapSettingsToOutput(output);
        }
    }

    // Re-apply float/ignore rules to already-open windows so the KCM "Apply"
    // button (which sends reloadConfig -> slotReconfigure -> here) takes effect
    // live, not just on the next new window or logout. We only float windows
    // that now match a rule and are still tiled; we never auto-tile a floating
    // window here, so manual Meta+W floats are never clobbered and an unrelated
    // reloadConfig stays a no-op.
    // Asymmetric by design (TilingState has no float-source flag). If
    // un-floating on rule *removal* must also be live, track why a window floats
    // (manual vs rule) and re-evaluate both directions.
    if (m_enabled && m_workspace) {
        for (Window *window : m_workspace->windows()) {
            if (!window || window->isDeleted()) {
                continue;
            }
            if (window->tilingState().mode == TilingState::Mode::Tiled
                && m_rules->initialMode(window) == TilingState::Mode::Floating) {
                setFloating(window, true);
            }
            if (window->tilingState().mode == TilingState::Mode::Tiled) {
                if (m_borderlessWhenTiled) {
                    forceNoBorder(window);
                } else {
                    restoreBorder(window);
                }
            }
        }
    }
}

void TilingController::initializeLayouts()
{
    if (!m_enabled || !m_workspace) {
        return;
    }

    for (LogicalOutput *output : m_workspace->outputs()) {
        onOutputAdded(output);
    }
}

void TilingController::onOutputAdded(LogicalOutput *output)
{
    if (!m_enabled || !m_workspace || !output) {
        return;
    }

    TileManager *manager = m_workspace->tileManager(output);
    if (!manager) {
        return;
    }
    for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
        setupLayoutEngine(output, manager, desktop, layoutKindFor(output, desktop));
    }
    applyGapSettingsToOutput(output);
}

void TilingController::setupLayoutEngine(LogicalOutput *output, TileManager *manager, VirtualDesktop *desktop,
                                         LayoutEngine::LayoutKind kind)
{
    if (!manager || !desktop) {
        return;
    }

    // If a layout engine is already attached, leave it alone. Switching the
    // layout on a live engine is handled by setLayout() / cycleLayout() so
    // that already-tiled windows can be migrated cleanly.
    if (manager->layoutEngine(desktop)) {
        return;
    }

    auto engine = createLayoutEngine(kind, manager);
    // Seed the configured sizing so new engines (and engines on
    // freshly-connected outputs / desktops) match the persisted layout.
    seedEngineSizing(output, engine.get(), kind);
    manager->setLayoutEngine(desktop, std::move(engine));
}

void TilingController::seedEngineSizing(LogicalOutput *output, LayoutEngine *engine, LayoutEngine::LayoutKind kind)
{
    if (!engine) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    const KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    const OutputSizing sizing = readOutputSizing(tilingGroup, output);
    engine->setPrimaryCount(sizing.masterCount);
    // Scrolling sizes new columns from DefaultColumnWidth; MasterStack/Stacked
    // use the master ratio. Routing both through here keeps the two settings
    // from overwriting one another (the master ratio used to seed scrolling
    // columns, so two columns no longer fit the screen).
    if (kind == LayoutEngine::LayoutKind::Scrolling) {
        engine->setDefaultColumnWidth(sizing.defaultColumnWidth);
    } else {
        engine->setPrimarySplit(sizing.masterRatio);
    }
}

LayoutEngine::LayoutKind TilingController::globalDefaultLayoutKind() const
{
    return m_defaultLayout;
}

LayoutEngine::LayoutKind TilingController::resolveLayoutKind(LogicalOutput *output, VirtualDesktop *desktop) const
{
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    LayoutEngine::LayoutKind kind = globalDefaultLayoutKind();

    if (desktop && output) {
        const KConfigGroup combinedGroup(&tilingGroup,
                                         QStringLiteral("DesktopOutput %1:%2").arg(desktop->x11DesktopNumber()).arg(output->name()));
        if (combinedGroup.hasKey("DefaultLayout")) {
            kind = LayoutEngine::layoutKindFromString(combinedGroup.readEntry("DefaultLayout", QString()));
        }
    }

    if (desktop && output && kind == globalDefaultLayoutKind()) {
        const KConfigGroup outputGroup(&tilingGroup, QStringLiteral("Output %1").arg(output->name()));
        if (outputGroup.hasKey("DefaultLayout")) {
            kind = LayoutEngine::layoutKindFromString(outputGroup.readEntry("DefaultLayout", QString()));
        }
    }
    // If the configured default is not currently enabled, fall back to the
    // first enabled layout so the monitor is always in a usable state.
    if (!isLayoutEnabled(kind)) {
        const QList<LayoutEngine::LayoutKind> enabled = enabledLayoutKinds();
        if (!enabled.isEmpty()) {
            kind = enabled.first();
        }
    }
    return kind;
}

LayoutEngine::LayoutKind TilingController::layoutKindFor(LogicalOutput *output, VirtualDesktop *desktop) const
{
    if (output && desktop) {
        KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
        KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
        const KConfigGroup mem(&tilingGroup, QStringLiteral("DesktopLayouts"));
        const QString key = output->name() + QLatin1Char('/') + desktop->id();
        if (mem.hasKey(key)) {
            const LayoutEngine::LayoutKind kind = LayoutEngine::layoutKindFromString(mem.readEntry(key, QString()));
            // Honour the remembered choice only while it is still an enabled
            // layout; otherwise fall through to the config default.
            if (isLayoutEnabled(kind)) {
                return kind;
            }
        }
    }
    return resolveLayoutKind(output, desktop);
}

void TilingController::persistLayoutChoice(LogicalOutput *output, VirtualDesktop *desktop, LayoutEngine::LayoutKind kind)
{
    if (!output || !desktop) {
        return;
    }
    // Stored in its own [Tiling][DesktopLayouts] group, not the per-output
    // "Output <name>" sub-group, which the KCM deletes and rewrites on save.
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    KConfigGroup mem(&tilingGroup, QStringLiteral("DesktopLayouts"));
    mem.writeEntry(output->name() + QLatin1Char('/') + desktop->id(), LayoutEngine::layoutKindToString(kind));
    config->sync();
}

QList<LayoutEngine::LayoutKind> TilingController::enabledLayoutKinds() const
{
    QList<LayoutEngine::LayoutKind> result;
    for (const QString &name : m_enabledLayouts) {
        // Only include kinds the controller actually knows how to build.
        if (name.compare(QLatin1String("MasterStack"), Qt::CaseInsensitive) == 0) {
            result.append(LayoutEngine::LayoutKind::MasterStack);
        } else if (name.compare(QLatin1String("Stacked"), Qt::CaseInsensitive) == 0) {
            result.append(LayoutEngine::LayoutKind::Stacked);
        } else if (name.compare(QLatin1String("Scrolling"), Qt::CaseInsensitive) == 0) {
            result.append(LayoutEngine::LayoutKind::Scrolling);
        } else if (name.compare(QLatin1String("Centered"), Qt::CaseInsensitive) == 0) {
            result.append(LayoutEngine::LayoutKind::Centered);
        } else if (name.compare(QLatin1String("Grid"), Qt::CaseInsensitive) == 0) {
            result.append(LayoutEngine::LayoutKind::Grid);
        }
    }
    if (result.isEmpty()) {
        // The user has disabled everything; fall back to the global default so
        // we always have at least one layout available.
        result.append(globalDefaultLayoutKind());
    }
    return result;
}

bool TilingController::isLayoutEnabled(LayoutEngine::LayoutKind kind) const
{
    return enabledLayoutKinds().contains(kind);
}

void TilingController::applyGapSettingsToOutput(LogicalOutput *output)
{
    if (!m_workspace || !output) {
        return;
    }

    const ReflowScope scope(this, output, ReflowContext::Reason::GapChange,
                            reflowScopeLayoutKind(output));

    TileManager *manager = m_workspace->tileManager(output);
    if (!manager) {
        return;
    }

    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));

    // Defaults from the [Tiling] group.
    const qreal defaultGapBetween = tilingGroup.readEntry("GapBetween", 0.0);
    const int defaultGapLeft = tilingGroup.readEntry("GapLeft", 0);
    const int defaultGapRight = tilingGroup.readEntry("GapRight", 0);
    const int defaultGapTop = tilingGroup.readEntry("GapTop", 0);
    const int defaultGapBottom = tilingGroup.readEntry("GapBottom", 0);

    // Per-output override in the [Tiling][Output "name"] sub-group, if any.
    // Entries fall back to the defaults above when not present in the override.
    const QString outputKey = QStringLiteral("Output %1").arg(output->name());
    KConfigGroup outputGroup(&tilingGroup, outputKey);

    const qreal gapBetween = outputGroup.readEntry("GapBetween", defaultGapBetween);
    const int gapLeft = outputGroup.readEntry("GapLeft", defaultGapLeft);
    const int gapRight = outputGroup.readEntry("GapRight", defaultGapRight);
    const int gapTop = outputGroup.readEntry("GapTop", defaultGapTop);
    const int gapBottom = outputGroup.readEntry("GapBottom", defaultGapBottom);
    const QMarginsF gapMargins(gapLeft, gapTop, gapRight, gapBottom);

    for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
        if (RootTile *root = manager->rootTile(desktop)) {
            LayoutEngine *eng = manager->layoutEngine(desktop);
            int n = eng ? eng->windows().count() : 0;
            if (m_gapsSuppressed || n <= 1) {
                // No gaps: either the user toggled them off, or smart gaps
                // (no indent/between for a single or empty layout).
                root->setGapBetween(0);
                root->setGapMargins({});
            } else {
                root->setGapBetween(gapBetween);
                root->setGapMargins(gapMargins);
            }
            if (eng) {
                eng->reflow();
            }
        }
    }
}

void TilingController::onWindowAdded(Window *window)
{
    if (!m_enabled || !window) {
        return;
    }

    // Watch for mouse-driven moves so we can swap with the window under the
    // cursor on release rather than treating the dragged window as a new one.
    connect(window, &Window::interactiveMoveResizeStarted,
            this, &TilingController::onInteractiveMoveResizeStarted,
            Qt::UniqueConnection);
    connect(window, &Window::interactiveMoveResizeFinished,
            this, &TilingController::onInteractiveMoveResizeFinished,
            Qt::UniqueConnection);

    // When the window is moved between desktops (e.g. via the
    // "Window to Next/Previous/Up/Down Desktop" shortcuts) the layout engines
    // need to migrate the window so it stays tiled on the new desktop and the
    // old layout reflows to fill the empty slot.
    //
    // No Qt::UniqueConnection: it only works with pointer-to-member-function
    // slots, not lambdas — Qt refuses the connection (with a runtime warning)
    // and the handler would never fire. onWindowAdded runs exactly once per
    // window (windowAdded signal), so the connection is single-shot anyway.
    connect(window, &Window::desktopsChanged, this,
            [this, window]() { onWindowDesktopsChanged(window); });

    // Whenever the window changes output (dragged or sent to another monitor),
    // purge it from the OLD output's engines (source reflows, no phantoms).
    // Non-interactive moves also get destination placement here; interactive
    // drags defer to onWindowMoveFinished for cursor-aware dropWindow.
    connect(window, &Window::outputChanged, this,
            [this, window](LogicalOutput *oldOutput) { onWindowOutputChanged(window, oldOutput); });

    // Minimizing a tiled window must drop it from its layout so the siblings
    // reflow to reclaim the space; restoring re-tiles it. Lambda (no
    // UniqueConnection) for the same reason as the connections above:
    // onWindowAdded runs once per window.
    connect(window, &Window::minimizedChanged, this,
            [this, window]() { onWindowMinimizedChanged(window); });

    // Don't touch already-managed windows (e.g. on-all-desktops already handled).
    if (window->tilingState().mode != TilingState::Mode::Floating) {
        return;
    }

    TilingState::Mode mode = m_rules->initialMode(window);
    window->tilingState().mode = mode;

    if (mode == TilingState::Mode::Tiled) {
        LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
        VirtualDesktop *desktop = window->desktops().isEmpty()
            ? VirtualDesktopManager::self()->currentDesktop(output)
            : window->desktops().constFirst();
        addWindowToLayout(window, output, desktop);
        if (output) {
            applyGapSettingsToOutput(output);
        }
    }
    applyFloatStacking(window);
}

void TilingController::onWindowRemoved(Window *window)
{
    if (!window) {
        return;
    }
    // Drop any in-flight move/resize context (a window destroyed mid-drag never
    // emits interactiveMoveResizeFinished, which would otherwise leave a stale
    // entry keyed by a dangling pointer).
    m_activeMoves.remove(window);
    m_activeResizes.remove(window);
    for (auto it = m_masterPins.begin(); it != m_masterPins.end();) {
        if (it.value().isNull() || it.value() == window) {
            it = m_masterPins.erase(it);
        } else {
            ++it;
        }
    }
    LogicalOutput *out = window->output();
    removeWindowFromLayouts(window);
    if (out) {
        applyGapSettingsToOutput(out);
        for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
            reassertMasterPin(out, desktop);
        }
    }
}

void TilingController::addWindowToLayout(Window *window, LogicalOutput *output, VirtualDesktop *desktop)
{
    if (!output || !desktop) {
        return;
    }

    TileManager *manager = m_workspace->tileManager(output);
    if (!manager) {
        return;
    }

    LayoutEngine::LayoutKind kind = layoutKindFor(output, desktop);
    if (m_rules && m_rules->prefersStacked(window)) {
        kind = LayoutEngine::LayoutKind::Stacked;
    }
    setupLayoutEngine(output, manager, desktop, kind);

    LayoutEngine *engine = manager->layoutEngine(desktop);
    if (!engine) {
        qWarning() << "TilingController: failed to obtain engine for output"
                   << output->name() << "desktop" << desktop->id();
        return;
    }

    const ReflowScope scope(this, output, ReflowContext::Reason::Add, kind);
    engine->addWindow(window);

    // If the window did not end up managed, surface it in logs but do NOT flip
    // the mode to Floating: the caller owns the mode, and the next
    // desktop/output change re-evaluates and snaps the tile correctly.
    if (!layoutEngineForWindow(window)) {
        qWarning() << "TilingController: window" << window->caption()
                   << "was not managed by any layout engine after addWindow; leaving mode untouched";
        return;
    }
    forceNoBorder(window);
    reassertMasterPin(output, desktop);
}

void TilingController::migrateWindow(Window *window, LogicalOutput *newOutput, VirtualDesktop *newDesktop)
{
    if (!m_workspace || !window || !newOutput || !newDesktop) {
        return;
    }

    // Find whichever engine currently owns this window, if any. The lookup is
    // O(outputs * desktops), acceptable because migrations happen on single
    // keypresses / drag releases, not in a hot path.
    LogicalOutput *oldOutput = nullptr;
    VirtualDesktop *oldDesktop = nullptr;
    LayoutEngine *oldEngine = layoutEngineForWindow(window, &oldOutput, &oldDesktop);

    // No-op if already in the destination engine; avoids spurious reflows.
    if (oldEngine && oldOutput == newOutput && oldDesktop == newDesktop) {
        return;
    }

    bool pinFollows = false;
    if (oldOutput && oldDesktop) {
        const QString oldKey = pinKeyFor(oldOutput, oldDesktop);
        if (m_masterPins.value(oldKey) == window) {
            m_masterPins.remove(oldKey);
            pinFollows = true;
        }
    }

    // Release the source (it reflows to fill the freed slot), then join the
    // destination — addWindowToLayout creates the engine if this is the first
    // window to land on that (output, desktop).
    if (oldEngine && oldOutput) {
        const ReflowScope removeScope(this, oldOutput, ReflowContext::Reason::Remove, oldEngine->layoutKind());
        oldEngine->removeWindow(window);
    }
    if (oldOutput && oldOutput != newOutput) {
        applyGapSettingsToOutput(oldOutput);
    }
    {
        const ReflowScope migrateScope(this, newOutput, ReflowContext::Reason::Migrate,
                                       layoutKindFor(newOutput, newDesktop));
        addWindowToLayout(window, newOutput, newDesktop);
    }
    applyGapSettingsToOutput(newOutput);

    if (pinFollows && shouldTile(window)) {
        m_masterPins.insert(pinKeyFor(newOutput, newDesktop), window);
    }
    if (oldOutput && oldDesktop) {
        reassertMasterPin(oldOutput, oldDesktop);
    }
    reassertMasterPin(newOutput, newDesktop);
}

void TilingController::removeWindowFromLayouts(Window *window)
{
    if (!m_workspace) {
        return;
    }

    restoreBorder(window);

    for (LogicalOutput *output : m_workspace->outputs()) {
        TileManager *manager = m_workspace->tileManager(output);
        if (!manager) {
            continue;
        }
        const ReflowScope scope(this, output, ReflowContext::Reason::Remove,
                                reflowScopeLayoutKind(output));
        for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
            if (LayoutEngine *engine = manager->layoutEngine(desktop)) {
                engine->removeWindow(window);
            }
        }
    }
}

bool TilingController::shouldTile(const Window *window) const
{
    return window && window->tilingState().mode == TilingState::Mode::Tiled;
}

LayoutEngine *TilingController::activeLayoutEngine() const
{
    if (!m_workspace) {
        return nullptr;
    }

    LogicalOutput *output = m_workspace->activeOutput();
    if (!output) {
        return nullptr;
    }

    TileManager *manager = m_workspace->tileManager(output);
    if (!manager) {
        return nullptr;
    }

    return manager->layoutEngine();
}

LayoutEngine *TilingController::layoutEngineForWindow(Window *window, LogicalOutput **output, VirtualDesktop **desktop) const
{
    if (!m_workspace || !window) {
        return nullptr;
    }

    for (LogicalOutput *out : m_workspace->outputs()) {
        TileManager *manager = m_workspace->tileManager(out);
        if (!manager) {
            continue;
        }
        for (VirtualDesktop *desk : VirtualDesktopManager::self()->desktops()) {
            if (LayoutEngine *engine = manager->layoutEngine(desk)) {
                if (engine->windows().contains(window)) {
                    if (output) {
                        *output = out;
                    }
                    if (desktop) {
                        *desktop = desk;
                    }
                    return engine;
                }
            }
        }
    }

    return nullptr;
}

Window *TilingController::activeTiledWindow() const
{
    Window *window = m_workspace ? m_workspace->activeWindow() : nullptr;
    if (window && shouldTile(window)) {
        return window;
    }
    return nullptr;
}

void TilingController::focusLeft()
{
    focusInDirection(LayoutEngine::FocusDirection::Left);
}

void TilingController::focusRight()
{
    focusInDirection(LayoutEngine::FocusDirection::Right);
}

void TilingController::focusUp()
{
    focusInDirection(LayoutEngine::FocusDirection::Up);
}

void TilingController::focusDown()
{
    focusInDirection(LayoutEngine::FocusDirection::Down);
}

void TilingController::focusInDirection(LayoutEngine::FocusDirection direction)
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (!engine) {
        return;
    }

    Window *target = engine->windowInDirection(window, direction);
    if (!target) {
        // At the layout's edge: continue focus onto the adjacent monitor.
        target = windowOnAdjacentOutput(direction);
    }
    if (target && m_workspace) {
        m_workspace->activateWindow(target);
    }
}

Window *TilingController::windowOnAdjacentOutput(LayoutEngine::FocusDirection direction) const
{
    if (!m_workspace) {
        return nullptr;
    }
    Window *active = activeTiledWindow();
    LogicalOutput *current = (active && active->output()) ? active->output() : m_workspace->activeOutput();
    if (!current) {
        return nullptr;
    }
    LogicalOutput *adjacent = m_workspace->findOutput(current, toWorkspaceDirection(direction), false);
    if (!adjacent || adjacent == current) {
        return nullptr;
    }
    TileManager *manager = m_workspace->tileManager(adjacent);
    if (!manager) {
        return nullptr;
    }
    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop(adjacent);
    LayoutEngine *engine = manager->layoutEngine(desktop);
    if (!engine) {
        return nullptr;
    }
    const QList<Window *> ws = engine->windows();
    if (ws.isEmpty()) {
        return nullptr;
    }
    // Enter from the side we crossed: moving left/up lands on the far window,
    // moving right/down on the near one.
    const bool enterNear = (direction == LayoutEngine::FocusDirection::Right
                            || direction == LayoutEngine::FocusDirection::Down);
    return enterNear ? ws.first() : ws.last();
}

void TilingController::toggleFloating()
{
    Window *window = activeTiledWindow();
    if (!window) {
        window = m_workspace ? m_workspace->activeWindow() : nullptr;
    }
    if (!window) {
        return;
    }

    TilingState &state = window->tilingState();
    if (state.mode == TilingState::Mode::Tiled) {
        state.mode = TilingState::Mode::Floating;
        removeWindowFromLayouts(window);
        LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
        const ReflowScope floatScope(this, output, ReflowContext::Reason::Float,
                                     reflowScopeLayoutKind(output));
        // Float at a default size centered under the cursor, respecting min/max.
        constexpr qreal defaultWidth = 800.0;
        constexpr qreal defaultHeight = 600.0;
        const QSizeF min = window->minSize();
        const QSizeF max = window->maxSize();
        qreal w = std::max(defaultWidth, min.width());
        qreal h = std::max(defaultHeight, min.height());
        if (max.width() > 0) {
            w = std::min(w, max.width());
        }
        if (max.height() > 0) {
            h = std::min(h, max.height());
        }
        const QPointF cursorPos = Cursors::self()->mouse()->pos();
        RectF geom(cursorPos.x() - w / 2, cursorPos.y() - h / 2, w, h);
        if (m_workspace) {
            const RectF screenArea = m_workspace->clientArea(PlacementArea, window);
            geom = window->keepInArea(geom, screenArea);
        }
        ReflowHint hint = ReflowHint::build(window, geom, reflowContextFor(output));
        window->tilingMoveResize(geom, hint);
    } else {
        state.mode = TilingState::Mode::Tiled;
        LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
        VirtualDesktop *desktop = window->desktops().isEmpty()
            ? VirtualDesktopManager::self()->currentDesktop(output)
            : window->desktops().constFirst();
        addWindowToLayout(window, output, desktop);
    }
    applyFloatStacking(window);
}

void TilingController::onInteractiveMoveResizeStarted()
{
    Window *window = qobject_cast<Window *>(sender());
    if (!window) {
        return;
    }

    // The signal covers both moves and resizes. Treat it as a resize only when
    // the window is explicitly resizing; otherwise default to a move. This
    // direction matters: a move misclassified as a resize skips the cross-output
    // cleanup and leaves a phantom tile on the source monitor, whereas a resize
    // misclassified as a move just restores the window harmlessly.
    if (window->isInteractiveResize()) {
        if (layoutEngineForWindow(window)) {
            m_activeResizes.insert(window, window->frameGeometry());
        }
        return;
    }

    LayoutEngine *engine = layoutEngineForWindow(window);
    if (!engine) {
        return;
    }

    // Remember where the window came from so we can restore/swap/clean up correctly.
    MoveContext context;
    context.engine = engine;
    context.output = window->output();
    context.originalGeometryRestore = window->geometryRestore();
    m_activeMoves[window] = context;

    // Make the dragged preview smaller (600x800) so it doesn't obscure the
    // whole screen, while still respecting the window's own min/max size.
    constexpr qreal previewWidth = 600.0;
    constexpr qreal previewHeight = 800.0;
    const QSizeF min = window->minSize();
    const QSizeF max = window->maxSize();
    qreal w = std::max(previewWidth, min.width());
    qreal h = std::max(previewHeight, min.height());
    if (max.width() > 0) {
        w = std::min(w, max.width());
    }
    if (max.height() > 0) {
        h = std::min(h, max.height());
    }
    window->setGeometryRestore(RectF(0, 0, w, h));

    engine->beginMoveWindow(window);
}

void TilingController::onInteractiveMoveResizeFinished()
{
    Window *window = qobject_cast<Window *>(sender());
    if (window) {
        auto resizeIt = m_activeResizes.find(window);
        if (resizeIt != m_activeResizes.end()) {
            const RectF startGeometry = resizeIt.value();
            m_activeResizes.erase(resizeIt);
            onWindowResizeFinished(window, startGeometry);
            return;
        }
    }
    onWindowMoveFinished(window);
}

void TilingController::onWindowResizeFinished(Window *window, const RectF &startGeometry)
{
    if (!window || !m_workspace) {
        return;
    }
    if (window->tilingState().mode != TilingState::Mode::Tiled) {
        return;
    }
    LayoutEngine *engine = layoutEngineForWindow(window);
    if (!engine) {
        return;
    }

    // Let the engine reinterpret the new geometry as a split change. For
    // master-stack this turns a drag of the master/stack divider into master
    // ratio, and horizontal drags within a column into height weights.
    // Unsupported drags (e.g. in stacked) reflow/snap.
    const RectF area = m_workspace->clientArea(PlacementArea, window);
    if (!engine->endResizeWindow(window, area, startGeometry)) {
        return;
    }

    // Persist the resulting split if the engine has one and it actually changed.
    const qreal ratio = engine->primarySplit();
    if (ratio <= 0.0) {
        return;
    }
    LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    const OutputSizing sizing = readOutputSizing(tilingGroup, output);
    if (qFuzzyCompare(ratio, sizing.masterRatio)) {
        return;
    }
    sizingWriteGroup(tilingGroup, output).writeEntry("MasterRatio", ratio);
    if (!output || !tilingGroup.group(QStringLiteral("Output %1").arg(output->name())).exists()) {
        m_masterRatio = ratio;
    }
    config->sync();
}

void TilingController::onWindowMoveFinished(Window *window)
{
    if (!window || !m_workspace) {
        return;
    }
    // If the user explicitly floated the window, discard any move context.
    if (window->tilingState().mode != TilingState::Mode::Tiled) {
        m_activeMoves.remove(window);
        return;
    }

    auto it = m_activeMoves.find(window);
    if (it != m_activeMoves.end()) {
        MoveContext context = it.value();
        m_activeMoves.erase(it);

        LogicalOutput *currentOutput = window->output() ? window->output() : m_workspace->activeOutput();
        const bool movedToOtherOutput = context.output && currentOutput && context.output != currentOutput;

        const QPointF cursorPos = Cursors::self()->mouse()->pos();
        const RectF area = m_workspace->clientArea(PlacementArea, window);

        if (movedToOtherOutput && context.engine) {
            // The window left its original output: clean up the empty source
            // tile, then insert it into the destination at the drop position
            // (next to the window under the cursor, else by column) instead of
            // blindly appending.
            context.engine->cancelMoveWindow(window);
            VirtualDesktop *desktop = window->desktops().isEmpty()
                ? VirtualDesktopManager::self()->currentDesktop(currentOutput)
                : window->desktops().constFirst();
            if (TileManager *destManager = m_workspace->tileManager(currentOutput)) {
                setupLayoutEngine(currentOutput, destManager, desktop, resolveLayoutKind(currentOutput, desktop));
                if (LayoutEngine *destEngine = destManager->layoutEngine(desktop)) {
                    Window *target = windowUnderCursorInEngine(destEngine);
                    if (target == window) {
                        target = nullptr;
                    }
                    destEngine->dropWindow(window, target, cursorPos, area);
                    destEngine->pruneEmpty();
                }
            }
            // Defensive: if dropWindow did not result in the window being
            // managed (e.g. manage() rejected), fall back to plain add so the
            // destination layout always incorporates the moved window.
            if (!layoutEngineForWindow(window)) {
                addWindowToLayout(window, currentOutput, desktop);
            }
            applyGapSettingsToOutput(currentOutput);
            window->setGeometryRestore(context.originalGeometryRestore);
            return;
        }

        if (context.engine) {
            Window *target = windowUnderCursorInEngine(context.engine);
            if (target == window) {
                target = nullptr;
            }
            if (target) {
                // Dropped onto another tiled window: swap places.
                if (context.engine->endMoveWindow(window, target)) {
                    context.engine->pruneEmpty();
                    window->setGeometryRestore(context.originalGeometryRestore);
                    return;
                }
            } else {
                // Dropped on empty space (including "same spot" releases where
                // the cursor is still over the preview or no other managed
                // window is hit): clean the recorded source slot via
                // cancelMoveWindow (handles the case where KWin untiled the
                // window from its leaf at drag start, leaving an empty holder
                // behind) then insert at the cursor position.
                context.engine->cancelMoveWindow(window);
                context.engine->dropWindow(window, nullptr, cursorPos, area);
                context.engine->pruneEmpty();
                window->setGeometryRestore(context.originalGeometryRestore);
                return;
            }
        }
        // Engine couldn't handle it (e.g. source tile destroyed); fall through.
    }

    // No move context or engine couldn't handle it: fall back to legacy snap-back.
    if (layoutEngineForWindow(window)) {
        return;
    }
    LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
    VirtualDesktop *desktop = window->desktops().isEmpty()
        ? VirtualDesktopManager::self()->currentDesktop(output)
        : window->desktops().constFirst();
    addWindowToLayout(window, output, desktop);
    if (LayoutEngine *eng = layoutEngineForWindow(window)) {
        eng->pruneEmpty(); // any path that touched layout should not leave phantoms
    }
}

void TilingController::onWindowDesktopsChanged(Window *window)
{
    if (!m_enabled || !m_workspace || !window) {
        return;
    }

    // Floating windows are not part of any layout engine; nothing to migrate.
    if (window->tilingState().mode != TilingState::Mode::Tiled) {
        return;
    }

    // On-all-desktops or multi-desktop windows stay managed by whatever
    // layout engine they were already in; the new desktop set is not a
    // request to re-tile.
    if (window->isOnAllDesktops() || window->desktops().size() != 1) {
        return;
    }

    // During an interactive move or resize the destination output/desktop will be
    // resolved on release (onWindowMoveFinished/endInteractiveResize); don't
    // migrate to an intermediate output/desktop while the user is still dragging.
    if (m_activeMoves.contains(window) || m_activeResizes.contains(window)) {
        return;
    }

    VirtualDesktop *newDesktop = window->desktops().constFirst();
    if (!newDesktop) {
        return;
    }

    LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
    if (!output) {
        return;
    }

    // Same engine-swap path as a monitor move: release the source engine, join
    // the destination (output, desktop).
    migrateWindow(window, output, newDesktop);
}

void TilingController::onWindowMinimizedChanged(Window *window)
{
    if (!m_enabled || !m_workspace || !window) {
        return;
    }

    // Only tiled windows belong to a layout; floating ones are never in an
    // engine, so minimizing them is none of our business.
    if (window->tilingState().mode != TilingState::Mode::Tiled) {
        return;
    }

    if (window->isMinimized()) {
        // Drop it from whatever engine holds it; the siblings reflow to fill
        // the freed slot (same path as closing the window). Capture the output
        // first so the smart-gaps update targets the right monitor.
        LogicalOutput *out = window->output();
        removeWindowFromLayouts(window);
        if (out) {
            applyGapSettingsToOutput(out);
        }
    } else if (!layoutEngineForWindow(window)) {
        // Restored and not already tiled: re-tile it on its own output/desktop,
        // resolved exactly like onWindowAdded. Re-appends at the end of the
        // layout order (same as the close/reopen path), not its pre-minimize
        // slot. Add slot memory only if users actually miss it.
        LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
        VirtualDesktop *desktop = window->desktops().isEmpty()
            ? VirtualDesktopManager::self()->currentDesktop(output)
            : window->desktops().constFirst();
        addWindowToLayout(window, output, desktop);
        if (output) {
            applyGapSettingsToOutput(output);
        }
    }
}

Window *TilingController::windowUnderCursorInEngine(LayoutEngine *engine) const
{
    if (!m_workspace || !engine) {
        return nullptr;
    }
    const QPointF pos = Cursors::self()->mouse()->pos();
    const QList<Window *> &stacking = m_workspace->stackingOrder();
    for (auto it = stacking.rbegin(); it != stacking.rend(); ++it) {
        Window *window = *it;
        if (window->isDeleted()) {
            continue;
        }
        if (!window->isOnCurrentActivity() || !window->isOnCurrentDesktop()
            || window->isMinimized() || window->isHidden() || window->isHiddenByShowDesktop()) {
            continue;
        }
        if (window->hitTest(pos) && engine->windows().contains(window)) {
            return window;
        }
    }
    return nullptr;
}

void TilingController::focusLast()
{
    if (!m_workspace) {
        return;
    }
    if (m_prevFocused && m_prevFocused != m_workspace->activeWindow()) {
        m_workspace->activateWindow(m_prevFocused);
    }
}

bool TilingController::promoteToMaster(Window *window)
{
    if (!window) {
        return false;
    }

    LayoutEngine *engine = layoutEngineForWindow(window);
    if (!engine) {
        return false;
    }

    const int idx = engine->windows().indexOf(window);
    if (idx <= 0) {
        return false;
    }
    engine->moveWindow(window, -idx);
    return true;
}

void TilingController::promoteToMaster()
{
    promoteToMaster(activeTiledWindow());
}

QString TilingController::pinKeyFor(LogicalOutput *output, VirtualDesktop *desktop) const
{
    if (!output || !desktop) {
        return {};
    }
    return output->name() + QLatin1Char('/') + desktop->id();
}

void TilingController::reassertMasterPin(LogicalOutput *output, VirtualDesktop *desktop)
{
    if (!m_workspace || !output || !desktop) {
        return;
    }
    Window *pinned = m_masterPins.value(pinKeyFor(output, desktop));
    if (!pinned || !shouldTile(pinned)) {
        return;
    }
    TileManager *manager = m_workspace->tileManager(output);
    if (!manager) {
        return;
    }
    LayoutEngine *engine = manager->layoutEngine(desktop);
    if (!engine || !engine->windows().contains(pinned)) {
        return;
    }
    if (engine->primaryWindow() == pinned) {
        return;
    }
    promoteToMaster(pinned);
}

void TilingController::toggleMasterPin()
{
    Window *window = activeTiledWindow();
    if (!window) {
        return;
    }
    LogicalOutput *output = nullptr;
    VirtualDesktop *desktop = nullptr;
    if (!layoutEngineForWindow(window, &output, &desktop)) {
        return;
    }
    const QString key = pinKeyFor(output, desktop);
    if (m_masterPins.value(key) == window) {
        m_masterPins.remove(key);
    } else {
        m_masterPins.insert(key, window);
        reassertMasterPin(output, desktop);
    }
}

void TilingController::moveWindowNext()
{
    Window *window = activeTiledWindow();
    if (!window) {
        return;
    }
    LayoutEngine *engine = layoutEngineForWindow(window);
    if (!engine) {
        return;
    }
    engine->moveWindow(window, +1);
}

void TilingController::moveWindowPrevious()
{
    Window *window = activeTiledWindow();
    if (!window) {
        return;
    }
    LayoutEngine *engine = layoutEngineForWindow(window);
    if (!engine) {
        return;
    }
    engine->moveWindow(window, -1);
}

void TilingController::moveInDirection(LayoutEngine::FocusDirection direction)
{
    Window *window = activeTiledWindow();
    if (!window) {
        return;
    }
    LayoutEngine *engine = layoutEngineForWindow(window);
    if (!engine) {
        return;
    }
    Window *target = engine->windowInDirection(window, direction);
    if (target) {
        const auto &wins = engine->windows();
        int cur = wins.indexOf(window);
        int tgt = wins.indexOf(target);
        if (cur >= 0 && tgt >= 0) {
            engine->moveWindow(window, tgt - cur);
        }
        return;
    }
    // At the layout's edge: push the window onto the adjacent monitor.
    switch (direction) {
    case LayoutEngine::FocusDirection::Left:
        moveWindowToOutput(TilingDirection::West);
        break;
    case LayoutEngine::FocusDirection::Right:
        moveWindowToOutput(TilingDirection::East);
        break;
    case LayoutEngine::FocusDirection::Up:
        moveWindowToOutput(TilingDirection::North);
        break;
    case LayoutEngine::FocusDirection::Down:
        moveWindowToOutput(TilingDirection::South);
        break;
    }
}

void TilingController::moveLeft() { moveInDirection(LayoutEngine::FocusDirection::Left); }
void TilingController::moveRight() { moveInDirection(LayoutEngine::FocusDirection::Right); }
void TilingController::moveUp() { moveInDirection(LayoutEngine::FocusDirection::Up); }
void TilingController::moveDown() { moveInDirection(LayoutEngine::FocusDirection::Down); }

void TilingController::setLayout(LayoutEngine::LayoutKind kind)
{
    if (!m_workspace) {
        return;
    }

    LogicalOutput *output = m_workspace->activeOutput();
    if (!output) {
        return;
    }

    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop(output);
    if (!desktop) {
        return;
    }

    setLayoutOn(output, desktop, kind);
    // Remember the actual applied kind (setLayoutOn may have substituted a
    // fallback) so this manual choice survives a reconfigure and restart.
    if (TileManager *manager = m_workspace->tileManager(output)) {
        if (LayoutEngine *engine = manager->layoutEngine(desktop)) {
            persistLayoutChoice(output, desktop, engine->layoutKind());
            showLayoutNotification(engine->layoutKind());
        }
    }
}

void TilingController::setLayoutOn(LogicalOutput *output, VirtualDesktop *desktop, LayoutEngine::LayoutKind kind)
{
    if (!m_workspace || !output || !desktop) {
        return;
    }

    // If the user disabled this kind in the kcm, fall back to the first
    // enabled layout so setLayout / cycleLayout never silently do nothing.
    if (!isLayoutEnabled(kind)) {
        const QList<LayoutEngine::LayoutKind> enabled = enabledLayoutKinds();
        if (enabled.isEmpty()) {
            return;
        }
        kind = enabled.first();
    }

    TileManager *manager = m_workspace->tileManager(output);
    if (!manager) {
        return;
    }

    LayoutEngine *existing = manager->layoutEngine(desktop);
    if (!existing) {
        // No engine yet — just create one in the requested kind.
        setupLayoutEngine(output, manager, desktop, kind);
        return;
    }

    if (existing->layoutKind() == kind) {
        // Already the desired layout; nothing to do.
        return;
    }

    // Take ownership of the current windows so we can re-add them in the same
    // order once the new engine is in place.
    QList<Window *> carriedWindows = existing->windows();

    // Keep the primary/master window first so it stays master in the new layout.
    if (Window *primary = existing->primaryWindow()) {
        if (carriedWindows.size() > 1 && carriedWindows.first() != primary) {
            carriedWindows.removeOne(primary);
            carriedWindows.prepend(primary);
        }
    }

    auto engine = createLayoutEngine(kind, manager);
    seedEngineSizing(output, engine.get(), kind);
    manager->setLayoutEngine(desktop, std::move(engine));

    LayoutEngine *fresh = manager->layoutEngine(desktop);
    if (!fresh) {
        return;
    }

    const ReflowScope scope(this, output, ReflowContext::Reason::LayoutSwitch, kind);
    for (Window *w : carriedWindows) {
        if (!w || w->isDeleted()) {
            continue;
        }
        fresh->addWindow(w);
    }
}

void TilingController::reconcileLayoutKinds()
{
    if (!m_enabled || !m_workspace) {
        return;
    }

    for (LogicalOutput *output : m_workspace->outputs()) {
        if (!output) {
            continue;
        }
        for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
            setLayoutOn(output, desktop, layoutKindFor(output, desktop));
        }
    }
}

void TilingController::cycleLayout()
{
    if (!m_workspace) {
        return;
    }
    LogicalOutput *output = m_workspace->activeOutput();
    if (!output) {
        return;
    }
    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop(output);
    if (!desktop) {
        return;
    }
    TileManager *manager = m_workspace->tileManager(output);
    if (!manager) {
        return;
    }

    const QList<LayoutEngine::LayoutKind> enabled = enabledLayoutKinds();
    if (enabled.size() < 2) {
        // Nothing to cycle through.
        return;
    }

    // Detect the kind of the engine currently attached to this (output, desktop)
    // pair so the cycle picks the *next* one rather than always the first.
    LayoutEngine::LayoutKind currentKind = globalDefaultLayoutKind();
    if (LayoutEngine *current = manager->layoutEngine(desktop)) {
        currentKind = current->layoutKind();
    }

    int currentIndex = enabled.indexOf(currentKind);
    if (currentIndex < 0) {
        currentIndex = 0;
    }
    const int nextIndex = (currentIndex + 1) % enabled.size();
    setLayout(enabled.at(nextIndex));
}

void TilingController::showLayoutNotification(LayoutEngine::LayoutKind kind)
{
    if (!m_layoutSwitchOsd || QStandardPaths::isTestModeEnabled()) {
        return;
    }

    const QString text = LayoutEngine::layoutDisplayName(kind);
    const QString iconName = QStringLiteral("kwin");

    // Prefer the Plasma shell OSD (centered, sized to content) when available.
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.plasmashell"))) {
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                                QStringLiteral("/org/kde/osdService"),
                                                                QStringLiteral("org.kde.osdService"),
                                                                QStringLiteral("showText"));
        message.setArguments({iconName, text});
        QDBusConnection::sessionBus().asyncCall(message);
        return;
    }

    // KWin-only sessions (e.g. Noctalia): use a centered tiling OSD. KWin's
    // built-in OnScreenNotification uses a floating dialog that truncates text
    // and fades out when the pointer enters its geometry.
    TilingOsd::show(m_workspace, text, iconName);
}

void TilingController::moveWindowToOutput(TilingDirection direction)
{
    Window *window = activeTiledWindow();
    if (!window || !m_workspace) {
        return;
    }

    LogicalOutput *currentOutput = window->output();
    if (!currentOutput) {
        currentOutput = m_workspace->activeOutput();
    }

    Workspace::Direction workspaceDirection = Workspace::DirectionEast;
    switch (direction) {
    case TilingDirection::West:
        workspaceDirection = Workspace::DirectionWest;
        break;
    case TilingDirection::East:
        workspaceDirection = Workspace::DirectionEast;
        break;
    case TilingDirection::North:
        workspaceDirection = Workspace::DirectionNorth;
        break;
    case TilingDirection::South:
        workspaceDirection = Workspace::DirectionSouth;
        break;
    }
    LogicalOutput *targetOutput = m_workspace->findOutput(currentOutput, workspaceDirection, true);
    if (!targetOutput || targetOutput == currentOutput) {
        return;
    }

    // Capture the desktop before sendToOutput: the window keeps its desktop
    // membership, just on the new output. Fall back to the target output's
    // current desktop for on-all-desktops / desktop-less windows.
    VirtualDesktop *desktop = window->desktops().isEmpty()
        ? VirtualDesktopManager::self()->currentDesktop(targetOutput)
        : window->desktops().constFirst();
    if (!desktop) {
        return;
    }

    // Move window to target output, preserving desktop membership.
    window->sendToOutput(targetOutput);

    // Shared migration path with desktop moves; idempotent on no-op.
    migrateWindow(window, targetOutput, desktop);
}

void TilingController::resizePrimary(qreal delta)
{
    if (!m_workspace) {
        return;
    }
    LayoutEngine *engine = activeLayoutEngine();
    LogicalOutput *output = m_workspace->activeOutput();
    if (!engine || !output) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    OutputSizing sizing = readOutputSizing(tilingGroup, output);
    sizing.masterRatio = qBound(0.1, sizing.masterRatio + delta, 0.9);
    engine->setPrimarySplit(sizing.masterRatio);

    // Persist so the split survives a reconfigure / restart (engines are
    // recreated from config). Other engines pick it up on their next rebuild.
    sizingWriteGroup(tilingGroup, output).writeEntry("MasterRatio", sizing.masterRatio);
    if (!tilingGroup.group(QStringLiteral("Output %1").arg(output->name())).exists()) {
        m_masterRatio = sizing.masterRatio;
    }
    config->sync();
}

void TilingController::adjustMasterCount(int delta)
{
    if (!m_workspace) {
        return;
    }
    LayoutEngine *engine = activeLayoutEngine();
    LogicalOutput *output = m_workspace->activeOutput();
    if (!engine || !output) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    OutputSizing sizing = readOutputSizing(tilingGroup, output);
    sizing.masterCount = qMax(1, sizing.masterCount + delta);
    engine->setPrimaryCount(sizing.masterCount);

    sizingWriteGroup(tilingGroup, output).writeEntry("MasterCount", sizing.masterCount);
    if (!tilingGroup.group(QStringLiteral("Output %1").arg(output->name())).exists()) {
        m_masterCount = sizing.masterCount;
    }
    config->sync();
}

void TilingController::resizeActiveWindowHeight(qreal delta)
{
    Window *window = activeTiledWindow();
    if (!window) {
        return;
    }
    LayoutEngine *engine = layoutEngineForWindow(window);
    if (!engine) {
        return;
    }
    engine->adjustWindowHeight(window, delta);
}

void TilingController::resetSizes()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (!engine) {
        return;
    }
    engine->resetSizes();
    // Persist the reset master ratio so MasterStack engines stay reset across
    // rebuilds. (Scrolling resets live column widths; it reads DefaultColumnWidth,
    // which this leaves untouched.)
    if (!m_workspace) {
        return;
    }
    LogicalOutput *output = m_workspace->activeOutput();
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    constexpr qreal kResetRatio = 0.5;
    sizingWriteGroup(tilingGroup, output).writeEntry("MasterRatio", kResetRatio);
    if (!output || !tilingGroup.group(QStringLiteral("Output %1").arg(output->name())).exists()) {
        m_masterRatio = kResetRatio;
    }
    config->sync();
}

void TilingController::centerColumn()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->centerActiveColumn();
    }
}

void TilingController::cycleColumnWidth()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->cycleColumnWidth();
    }
}

void TilingController::toggleZoom()
{
    Window *window = activeTiledWindow();
    if (!window) {
        return;
    }
    LayoutEngine *engine = layoutEngineForWindow(window);
    if (!engine) {
        return;
    }
    // Toggle: zoom the active window, or un-zoom if it is already the monocle.
    engine->setZoomedWindow(engine->zoomedWindow() == window ? nullptr : window);
}

void TilingController::consumeWindow()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->consumeWindow();
    }
}

void TilingController::expelWindow()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->expelWindow();
    }
}

void TilingController::flipMaster()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->flipMaster();
    }
}

void TilingController::toggleGaps()
{
    m_gapsSuppressed = !m_gapsSuppressed;
    if (!m_workspace) {
        return;
    }
    for (LogicalOutput *output : m_workspace->outputs()) {
        applyGapSettingsToOutput(output);
    }
}

void TilingController::onWindowOutputChanged(Window *window, LogicalOutput *oldOutput)
{
    if (!m_workspace || !window || !oldOutput || oldOutput == window->output()) {
        return;
    }
    // The window left oldOutput: drop it from every engine there so the source
    // layout reflows. removeWindow is a no-op when the window isn't present, so
    // this is safe even if the drag/shortcut path already cleaned up.
    TileManager *manager = m_workspace->tileManager(oldOutput);
    if (manager) {
        for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
            if (LayoutEngine *engine = manager->layoutEngine(desktop)) {
                engine->removeWindow(window); // if the window is still in a leaf
                engine->pruneEmpty();         // if KWin left an empty leaf behind
            }
        }
        applyGapSettingsToOutput(oldOutput);
    }

    // For non-interactive output changes (e.g. menu "move to screen", direct
    // sendToOutput, or other actions), place the window into the destination
    // layout if it is tiled. Interactive drags are handled at move-finish with
    // dropWindow for insertion position.
    if (m_activeMoves.contains(window)) {
        return;
    }
    if (window->tilingState().mode != TilingState::Mode::Tiled) {
        return;
    }

    LogicalOutput *newOutput = window->output();
    if (!newOutput) {
        return;
    }

    VirtualDesktop *desktop = window->desktops().isEmpty()
        ? VirtualDesktopManager::self()->currentDesktop(newOutput)
        : window->desktops().constFirst();
    if (!desktop) {
        return;
    }

    migrateWindow(window, newOutput, desktop);
    applyGapSettingsToOutput(newOutput);
}

void TilingController::retile()
{
    if (!m_enabled || !m_workspace) {
        return;
    }
    LogicalOutput *output = m_workspace->activeOutput();
    if (!output) {
        return;
    }
    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop(output);
    if (!desktop) {
        return;
    }
    TileManager *manager = m_workspace->tileManager(output);
    if (!manager) {
        return;
    }

    // Keep the current layout kind but force a fresh engine, so any stale or
    // phantom leaves are discarded, then re-add the windows that actually
    // belong on this output+desktop. This is the manual recovery hatch.
    LayoutEngine::LayoutKind kind = resolveLayoutKind(output, desktop);
    if (LayoutEngine *existing = manager->layoutEngine(desktop)) {
        kind = existing->layoutKind();
    }
    auto engine = createLayoutEngine(kind, manager);
    seedEngineSizing(output, engine.get(), kind);
    manager->setLayoutEngine(desktop, std::move(engine));

    LayoutEngine *fresh = manager->layoutEngine(desktop);
    if (!fresh) {
        return;
    }
    const ReflowScope scope(this, output, ReflowContext::Reason::Retile, kind);
    for (Window *w : m_workspace->windows()) {
        if (!w || w->isDeleted() || w->tilingState().mode != TilingState::Mode::Tiled) {
            continue;
        }
        LogicalOutput *wout = w->output() ? w->output() : output;
        if (wout != output) {
            continue;
        }
        if (!w->isOnAllDesktops() && !w->desktops().contains(desktop)) {
            continue;
        }
        fresh->addWindow(w);
    }
}

ReflowContext &TilingController::reflowContextFor(LogicalOutput *output)
{
    static ReflowContext s_default;
    if (!output) {
        return s_default;
    }
    auto &stack = m_reflowContextStacks[output];
    if (stack.isEmpty()) {
        stack.append(ReflowContext{});
    }
    return stack.last();
}

ReflowContext TilingController::reflowContextFor(LogicalOutput *output) const
{
    if (!output) {
        return {};
    }
    const auto it = m_reflowContextStacks.constFind(output);
    if (it == m_reflowContextStacks.cend() || it->isEmpty()) {
        return {};
    }
    return it->last();
}

void TilingController::pushReflowContext(LogicalOutput *output, const ReflowContext &ctx)
{
    if (!output) {
        return;
    }
    m_reflowContextStacks[output].append(ctx);
}

void TilingController::popReflowContext(LogicalOutput *output)
{
    if (!output) {
        return;
    }
    auto it = m_reflowContextStacks.find(output);
    if (it == m_reflowContextStacks.end()) {
        return;
    }
    if (!it->isEmpty()) {
        it->removeLast();
    }
    if (it->isEmpty()) {
        m_reflowContextStacks.erase(it);
    }
}

int TilingController::nextReflowGroupId()
{
    return m_nextReflowGroupId++;
}

LayoutEngine::LayoutKind TilingController::reflowScopeLayoutKind(LogicalOutput *output,
                                                                 VirtualDesktop *desktop) const
{
    if (!output) {
        return LayoutEngine::LayoutKind::MasterStack;
    }
    if (!desktop) {
        desktop = VirtualDesktopManager::self()->currentDesktop(output);
    }
    return layoutKindFor(output, desktop);
}

void TilingController::forceNoBorder(Window *window)
{
    if (!m_borderlessWhenTiled || !window || !window->userCanSetNoBorder()) {
        return;
    }
    TilingState &state = window->tilingState();
    if (state.borderForced) {
        return;
    }
    state.originalNoBorder = window->noBorder();
    window->setNoBorder(true);
    state.borderForced = true;
}

void TilingController::restoreBorder(Window *window)
{
    if (!window) {
        return;
    }
    TilingState &state = window->tilingState();
    if (!state.borderForced) {
        return;
    }
    window->setNoBorder(state.originalNoBorder);
    state.borderForced = false;
}

void TilingController::applyFloatStacking(Window *window)
{
    if (!window || !m_floatAbove) {
        return;
    }
    // Float => keep above tiled windows; tile => release the override.
    window->setKeepAbove(window->tilingState().mode == TilingState::Mode::Floating);
}

void TilingController::setFloating(Window *window, bool floating)
{
    if (!window || !m_workspace) {
        return;
    }
    TilingState &state = window->tilingState();
    if ((state.mode == TilingState::Mode::Floating) == floating) {
        return;
    }
    if (floating) {
        state.mode = TilingState::Mode::Floating;
        removeWindowFromLayouts(window);
    } else {
        state.mode = TilingState::Mode::Tiled;
        LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
        VirtualDesktop *desktop = window->desktops().isEmpty()
            ? VirtualDesktopManager::self()->currentDesktop(output)
            : window->desktops().constFirst();
        addWindowToLayout(window, output, desktop);
    }
    applyFloatStacking(window);
}

bool TilingController::isFloatAppRule(const Window *window) const
{
    if (!window) {
        return false;
    }
    const QString cls = window->resourceClass().toLower();
    if (cls.isEmpty()) {
        return false;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    const QStringList classes = KConfigGroup(config, QStringLiteral("TilingRules")).readEntry("FloatingClass", QStringList());
    for (const QString &s : classes) {
        if (s.trimmed().toLower() == cls) {
            return true;
        }
    }
    return false;
}

void TilingController::setFloatAppRule(Window *window, bool floatApp)
{
    if (!window || !m_workspace) {
        return;
    }
    const QString cls = window->resourceClass().toLower();
    if (cls.isEmpty()) {
        return;
    }

    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup group(config, QStringLiteral("TilingRules"));
    QStringList classes = group.readEntry("FloatingClass", QStringList());

    const bool present = isFloatAppRule(window);
    if (floatApp == present) {
        return;
    }
    if (floatApp) {
        classes.append(cls);
    } else {
        QStringList kept;
        for (const QString &s : classes) {
            if (s.trimmed().toLower() != cls) {
                kept.append(s);
            }
        }
        classes = kept;
    }
    group.writeEntry("FloatingClass", classes);
    config->sync();

    // Reload rules and apply immediately to every open window of this class.
    KConfigGroup rulesGroup(config, QStringLiteral("TilingRules"));
    m_rules->load(rulesGroup);
    for (Window *w : m_workspace->windows()) {
        if (w && !w->isDeleted() && w->resourceClass().toLower() == cls) {
            setFloating(w, floatApp);
        }
    }
}

} // namespace KWin
