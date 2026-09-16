/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilingcontroller.h"

#include "core/output.h"
#include "tiling/tilingconfig.h"
#include "tiling/tilingreflow.h"
#include "core/rect.h"
#include "cursor.h"
#include "tiling/movefsm.h"
#include "tiling/sizingpolicy.h"
#include "tiling/suspendpolicy.h"
#include "tiling/tilingosd.h"
#include "tiles/directionmath.h"
#include "tiles/columnwidthpresets.h"
#include "tiles/layoutengine.h"
#include "tiles/gridlayoutengine.h"
#include "tiles/masterstacklayoutengine.h"
#include "tiles/masterstackmath.h"
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
#include <string>
#include <vector>

#include <optional>
#include <string>
#include <vector>

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

QString desktopOutputCacheKey(VirtualDesktop *desktop, LogicalOutput *output)
{
    if (!desktop || !output) {
        return {};
    }
    return QStringLiteral("%1:%2").arg(desktop->x11DesktopNumber()).arg(output->name());
}

KConfigGroup sizingWriteGroup(KConfigGroup &tilingGroup, LogicalOutput *output, VirtualDesktop *desktop,
                                tilingconfig::SizingWriteTarget target)
{
    switch (target) {
    case tilingconfig::SizingWriteTarget::DesktopOutput:
        return KConfigGroup(&tilingGroup, QStringLiteral("DesktopOutput %1:%2")
                                                 .arg(desktop->x11DesktopNumber())
                                                 .arg(output->name()));
    case tilingconfig::SizingWriteTarget::Output:
        return KConfigGroup(&tilingGroup, QStringLiteral("Output %1").arg(output->name()));
    case tilingconfig::SizingWriteTarget::Global:
        break;
    }
    return tilingGroup;
}

tilingconfig::LayoutKind toCfgKind(LayoutEngine::LayoutKind kind)
{
    return static_cast<tilingconfig::LayoutKind>(kind);
}

LayoutEngine::LayoutKind fromCfgKind(tilingconfig::LayoutKind kind)
{
    return static_cast<LayoutEngine::LayoutKind>(kind);
}

std::vector<tilingconfig::LayoutKind> toCfgKinds(const QList<LayoutEngine::LayoutKind> &kinds)
{
    std::vector<tilingconfig::LayoutKind> out;
    out.reserve(static_cast<size_t>(kinds.size()));
    for (LayoutEngine::LayoutKind k : kinds) {
        out.push_back(toCfgKind(k));
    }
    return out;
}

tilingconfig::LayoutKindInputs layoutKindInputs(LayoutEngine::LayoutKind globalDefault,
                                                 const QList<LayoutEngine::LayoutKind> &enabled,
                                                 const QHash<QString, LayoutEngine::LayoutKind> &desktopLayoutMemory,
                                                 const QHash<QString, LayoutEngine::LayoutKind> &desktopOutputLayouts,
                                                 const QHash<QString, LayoutEngine::LayoutKind> &outputDefaultLayouts,
                                                 LogicalOutput *output, VirtualDesktop *desktop)
{
    tilingconfig::LayoutKindInputs in;
    in.globalDefault = toCfgKind(globalDefault);
    in.enabled = toCfgKinds(enabled);
    if (!output || !desktop) {
        return in;
    }
    const QString memKey = output->name() + QLatin1Char('/') + desktop->id();
    if (desktopLayoutMemory.contains(memKey)) {
        in.remembered = toCfgKind(desktopLayoutMemory.value(memKey));
    }
    const QString combinedKey = QStringLiteral("%1:%2").arg(desktop->x11DesktopNumber()).arg(output->name());
    if (desktopOutputLayouts.contains(combinedKey)) {
        in.desktopOutput = toCfgKind(desktopOutputLayouts.value(combinedKey));
    }
    if (outputDefaultLayouts.contains(output->name())) {
        in.outputDefault = toCfgKind(outputDefaultLayouts.value(output->name()));
    }
    return in;
}

QStringList defaultColumnWidthPresetStrings()
{
    return {QStringLiteral("1/3"), QStringLiteral("1/2"), QStringLiteral("2/3"), QStringLiteral("1")};
}

QList<qreal> parseColumnWidthPresets(const QStringList &raw)
{
    std::vector<std::string> tokens;
    tokens.reserve(size_t(raw.size()));
    for (const QString &s : raw) {
        tokens.push_back(s.toStdString());
    }
    const std::vector<double> parsed = columnwidthpresets::parse(tokens);
    QList<qreal> out;
    out.reserve(int(parsed.size()));
    for (const double v : parsed) {
        out.append(qreal(v));
    }
    return out;
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
        // Drop per-output state when a monitor is unplugged (see onOutputRemoved).
        connect(m_workspace, &Workspace::outputRemoved, this, &TilingController::onOutputRemoved);
        m_lastFocused = m_workspace->activeWindow();
    }

    // Coalesce kwinrc writes from rapid interactive sizing changes into a single
    // delayed sync (see schedulePersist).
    m_persistTimer = new QTimer(this);
    m_persistTimer->setSingleShot(true);
    m_persistTimer->setInterval(400);
    connect(m_persistTimer, &QTimer::timeout, this, [] {
        KSharedConfig::openConfig(KWIN_CONFIG)->sync();
    });

    reconfigure();
}

TilingController::~TilingController() = default;

void TilingController::reconfigure()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    KConfigGroup rulesGroup(config, QStringLiteral("TilingRules"));

    const bool wasEnabled = m_enabled;
    m_enabled = tilingGroup.readEntry("Enabled", true);
    m_defaultLayout = LayoutEngine::layoutKindFromString(
        tilingGroup.readEntry("DefaultLayout", QStringLiteral("MasterStack")));
    m_masterRatio = tilingconfig::clampMasterRatio(tilingGroup.readEntry("MasterRatio", 0.5));
    m_defaultColumnWidth = tilingconfig::clampColumnWidth(tilingGroup.readEntry("DefaultColumnWidth", 0.5));
    m_centerFocusedColumn = viewportmath::parseCenterFocusedColumn(
        tilingGroup.readEntry("CenterFocusedColumn", QStringLiteral("never")).toStdString());
    m_columnWidthPresets = parseColumnWidthPresets(
        tilingGroup.readEntry("ColumnWidthPresets", defaultColumnWidthPresetStrings()));
    m_masterCount = tilingconfig::clampMasterCount(tilingGroup.readEntry("MasterCount", 1));
    m_layoutSwitchOsd = tilingGroup.readEntry("LayoutSwitchOsd", true);
    m_borderlessWhenTiled = tilingGroup.readEntry("BorderlessWhenTiled", false);
    // "master" promotes new windows to master; anything else (default "end")
    // keeps the historical append-at-tail behaviour.
    m_newWindowMaster = tilingGroup.readEntry("NewWindowPlacement", QStringLiteral("end"))
                            .compare(QLatin1String("master"), Qt::CaseInsensitive) == 0;
    m_rules->load(rulesGroup);
    loadConfigCache(tilingGroup);

    const auto enabledTransition = suspendpolicy::classifyEnabledChange(wasEnabled, m_enabled);
    if (!m_enabled) {
        // DisableNow or StayDisabled: detach every tiled window, restore borders.
        suspendAllTiledWindows();
        return;
    }
    if (enabledTransition == suspendpolicy::EnabledTransition::EnableNow) {
        // Live re-enable: only windows marked suspendedByDisable (not manual floats).
        resumeSuspendedWindows();
    }

    // Push master count/ratio to live engines so changes in the KCM (or
    // direct kwinrc edit + reloadConfig) take effect immediately without
    // logout/restart. setPrimary* triggers reflow in MasterStack.
    if (m_workspace) {
        for (LogicalOutput *output : m_workspace->outputs()) {
            if (TileManager *manager = m_workspace->tileManager(output)) {
                for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
                    if (LayoutEngine *eng = manager->layoutEngine(desktop)) {
                        seedEngineSizing(output, desktop, eng, eng->layoutKind());
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

void TilingController::onOutputRemoved(LogicalOutput *output)
{
    if (!output) {
        return;
    }
    // The TileManager (and its engines) for this output are torn down by KWin.
    // Drop the per-output reflow-context stack so a future LogicalOutput that
    // reuses this heap address cannot inherit a stale context in the hot path.
    m_reflowContextStacks.remove(output);
    unbindOutputWindows(output);

    // Master pins are keyed by "<output name>/<desktop id>"; drop the ones for
    // this output so we never try to reassert a pin onto a disconnected monitor.
    const QString prefix = output->name() + QLatin1Char('/');
    for (auto it = m_masterPins.begin(); it != m_masterPins.end();) {
        if (it.key().startsWith(prefix)) {
            it = m_masterPins.erase(it);
        } else {
            ++it;
        }
    }
}

void TilingController::schedulePersist()
{
    if (m_persistTimer) {
        m_persistTimer->start();
    }
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
    seedEngineSizing(output, desktop, engine.get(), kind);
    manager->setLayoutEngine(desktop, std::move(engine));
}

void TilingController::seedEngineSizing(LogicalOutput *output, VirtualDesktop *desktop, LayoutEngine *engine,
                                         LayoutEngine::LayoutKind kind)
{
    if (!engine) {
        return;
    }
    const CachedSizing sizing = resolvedSizing(output, desktop);
    engine->setPrimaryCount(sizing.masterCount);
    // Scrolling sizes new columns from DefaultColumnWidth; MasterStack/Stacked
    // use the master ratio. Routing both through here keeps the two settings
    // from overwriting one another (the master ratio used to seed scrolling
    // columns, so two columns no longer fit the screen).
    if (kind == LayoutEngine::LayoutKind::Scrolling) {
        engine->setDefaultColumnWidth(sizing.defaultColumnWidth);
        engine->setCenterFocusedColumn(m_centerFocusedColumn);
        engine->setColumnWidthPresets(m_columnWidthPresets);
    } else {
        engine->setPrimarySplit(sizing.masterRatio);
    }
}

TilingController::CachedSizing TilingController::resolvedSizing(LogicalOutput *output, VirtualDesktop *desktop) const
{
    const tilingconfig::OutputSizing global{m_masterRatio, m_masterCount, m_defaultColumnWidth};
    tilingconfig::OutputSizingOverride outputLayer;
    tilingconfig::OutputSizingOverride desktopLayer;
    if (output) {
        const auto it = m_outputSizing.constFind(output->name());
        if (it != m_outputSizing.cend()) {
            if (it->hasMasterRatio) {
                outputLayer.masterRatio = it->masterRatio;
            }
            if (it->hasMasterCount) {
                outputLayer.masterCount = it->masterCount;
            }
            if (it->hasDefaultColumnWidth) {
                outputLayer.defaultColumnWidth = it->defaultColumnWidth;
            }
        }
    }
    if (output && desktop) {
        const auto it = m_desktopOutputSizing.constFind(desktopOutputCacheKey(desktop, output));
        if (it != m_desktopOutputSizing.cend()) {
            if (it->hasMasterRatio) {
                desktopLayer.masterRatio = it->masterRatio;
            }
            if (it->hasMasterCount) {
                desktopLayer.masterCount = it->masterCount;
            }
            if (it->hasDefaultColumnWidth) {
                desktopLayer.defaultColumnWidth = it->defaultColumnWidth;
            }
        }
    }
    const tilingconfig::OutputSizing resolved = tilingconfig::resolveSizing(global, outputLayer, desktopLayer);
    return {resolved.masterRatio, resolved.masterCount, resolved.defaultColumnWidth};
}

void TilingController::persistMasterRatio(LogicalOutput *output, VirtualDesktop *desktop, qreal ratio)
{
    ratio = tilingconfig::clampMasterRatio(ratio);
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    const bool hasOutput = output != nullptr;
    const bool outputExists = hasOutput && m_outputGaps.contains(output->name());
    const bool hasDesktop = desktop != nullptr;
    const auto target = tilingconfig::sizingWriteTarget(hasOutput, outputExists, hasDesktop);
    sizingWriteGroup(tilingGroup, output, desktop, target).writeEntry("MasterRatio", ratio);
    switch (target) {
    case tilingconfig::SizingWriteTarget::DesktopOutput: {
        CachedSizingOverride &layer = m_desktopOutputSizing[desktopOutputCacheKey(desktop, output)];
        layer.hasMasterRatio = true;
        layer.masterRatio = ratio;
        break;
    }
    case tilingconfig::SizingWriteTarget::Output: {
        CachedSizingOverride &layer = m_outputSizing[output->name()];
        layer.hasMasterRatio = true;
        layer.masterRatio = ratio;
        break;
    }
    case tilingconfig::SizingWriteTarget::Global:
        m_masterRatio = ratio;
        break;
    }
    schedulePersist();
}

void TilingController::persistMasterCount(LogicalOutput *output, VirtualDesktop *desktop, int count)
{
    count = tilingconfig::clampMasterCount(count);
    KSharedConfigPtr config = KSharedConfig::openConfig(KWIN_CONFIG);
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    const bool hasOutput = output != nullptr;
    const bool outputExists = hasOutput && m_outputGaps.contains(output->name());
    const bool hasDesktop = desktop != nullptr;
    const auto target = tilingconfig::sizingWriteTarget(hasOutput, outputExists, hasDesktop);
    sizingWriteGroup(tilingGroup, output, desktop, target).writeEntry("MasterCount", count);
    switch (target) {
    case tilingconfig::SizingWriteTarget::DesktopOutput: {
        CachedSizingOverride &layer = m_desktopOutputSizing[desktopOutputCacheKey(desktop, output)];
        layer.hasMasterCount = true;
        layer.masterCount = count;
        break;
    }
    case tilingconfig::SizingWriteTarget::Output: {
        CachedSizingOverride &layer = m_outputSizing[output->name()];
        layer.hasMasterCount = true;
        layer.masterCount = count;
        break;
    }
    case tilingconfig::SizingWriteTarget::Global:
        m_masterCount = count;
        break;
    }
    schedulePersist();
}

LayoutEngine::LayoutKind TilingController::globalDefaultLayoutKind() const
{
    return m_defaultLayout;
}

void TilingController::loadConfigCache(const KConfigGroup &tilingGroup)
{
    // EnabledLayouts: parse once so isLayoutEnabled / cycleLayout don't rebuild
    // the list with case-insensitive compares on every window add/remove.
    const QStringList enabledNames = tilingGroup.readEntry("EnabledLayouts",
        QStringList{QLatin1String("MasterStack"), QLatin1String("Stacked"), QLatin1String("Scrolling"), QLatin1String("Centered")});
    std::vector<std::string> names;
    names.reserve(static_cast<size_t>(enabledNames.size()));
    for (const QString &name : enabledNames) {
        names.push_back(name.toStdString());
    }
    const std::vector<tilingconfig::LayoutKind> parsed =
        tilingconfig::parseEnabledKinds(names, toCfgKind(m_defaultLayout));
    m_enabledLayoutKinds.clear();
    m_enabledLayoutKinds.reserve(int(parsed.size()));
    for (tilingconfig::LayoutKind k : parsed) {
        m_enabledLayoutKinds.append(fromCfgKind(k));
    }

    m_gapDefaults.gapBetween = tilingGroup.readEntry("GapBetween", 0.0);
    m_gapDefaults.gapLeft = tilingGroup.readEntry("GapLeft", 0);
    m_gapDefaults.gapRight = tilingGroup.readEntry("GapRight", 0);
    m_gapDefaults.gapTop = tilingGroup.readEntry("GapTop", 0);
    m_gapDefaults.gapBottom = tilingGroup.readEntry("GapBottom", 0);

    m_outputGaps.clear();
    m_outputDefaultLayouts.clear();
    m_desktopOutputLayouts.clear();
    m_desktopLayoutMemory.clear();
    m_outputSizing.clear();
    m_desktopOutputSizing.clear();

    auto readSizingOverride = [](const KConfigGroup &group) {
        CachedSizingOverride o;
        if (group.hasKey(QStringLiteral("MasterRatio"))) {
            o.hasMasterRatio = true;
            o.masterRatio = tilingconfig::clampMasterRatio(group.readEntry("MasterRatio", 0.5));
        }
        if (group.hasKey(QStringLiteral("MasterCount"))) {
            o.hasMasterCount = true;
            o.masterCount = tilingconfig::clampMasterCount(group.readEntry("MasterCount", 1));
        }
        if (group.hasKey(QStringLiteral("DefaultColumnWidth"))) {
            o.hasDefaultColumnWidth = true;
            o.defaultColumnWidth = tilingconfig::clampColumnWidth(group.readEntry("DefaultColumnWidth", 0.5));
        }
        return o;
    };

    const QString outputPrefix = QStringLiteral("Output ");
    const QString desktopOutputPrefix = QStringLiteral("DesktopOutput ");
    for (const QString &sub : tilingGroup.groupList()) {
        if (sub.startsWith(outputPrefix)) {
            const QString outputName = sub.mid(outputPrefix.size());
            if (outputName.isEmpty()) {
                continue;
            }
            const KConfigGroup outputGroup = tilingGroup.group(sub);
            CachedGaps gaps = m_gapDefaults;
            gaps.gapBetween = outputGroup.readEntry("GapBetween", m_gapDefaults.gapBetween);
            gaps.gapLeft = outputGroup.readEntry("GapLeft", m_gapDefaults.gapLeft);
            gaps.gapRight = outputGroup.readEntry("GapRight", m_gapDefaults.gapRight);
            gaps.gapTop = outputGroup.readEntry("GapTop", m_gapDefaults.gapTop);
            gaps.gapBottom = outputGroup.readEntry("GapBottom", m_gapDefaults.gapBottom);
            m_outputGaps.insert(outputName, gaps);
            if (outputGroup.hasKey("DefaultLayout")) {
                m_outputDefaultLayouts.insert(outputName,
                    LayoutEngine::layoutKindFromString(outputGroup.readEntry("DefaultLayout", QString())));
            }
            const CachedSizingOverride sizing = readSizingOverride(outputGroup);
            if (sizing.hasMasterRatio || sizing.hasMasterCount || sizing.hasDefaultColumnWidth) {
                m_outputSizing.insert(outputName, sizing);
            }
        } else if (sub.startsWith(desktopOutputPrefix)) {
            const KConfigGroup combinedGroup = tilingGroup.group(sub);
            const QString key = sub.mid(desktopOutputPrefix.size());
            if (combinedGroup.hasKey("DefaultLayout")) {
                m_desktopOutputLayouts.insert(key,
                    LayoutEngine::layoutKindFromString(combinedGroup.readEntry("DefaultLayout", QString())));
            }
            const CachedSizingOverride sizing = readSizingOverride(combinedGroup);
            if (sizing.hasMasterRatio || sizing.hasMasterCount || sizing.hasDefaultColumnWidth) {
                m_desktopOutputSizing.insert(key, sizing);
            }
        }
    }

    const KConfigGroup mem(&tilingGroup, QStringLiteral("DesktopLayouts"));
    for (const QString &key : mem.keyList()) {
        m_desktopLayoutMemory.insert(key,
            LayoutEngine::layoutKindFromString(mem.readEntry(key, QString())));
    }
}

LayoutEngine::LayoutKind TilingController::resolveLayoutKind(LogicalOutput *output, VirtualDesktop *desktop) const
{
    tilingconfig::LayoutKindInputs in = layoutKindInputs(m_defaultLayout, m_enabledLayoutKinds,
                                                          m_desktopLayoutMemory, m_desktopOutputLayouts,
                                                          m_outputDefaultLayouts, output, desktop);
    in.remembered.reset();
    return fromCfgKind(tilingconfig::resolveConfigDefault(in.globalDefault, in.enabled,
                                                           in.desktopOutput, in.outputDefault));
}

LayoutEngine::LayoutKind TilingController::layoutKindFor(LogicalOutput *output, VirtualDesktop *desktop) const
{
    return fromCfgKind(tilingconfig::layoutKindFor(
        layoutKindInputs(m_defaultLayout, m_enabledLayoutKinds, m_desktopLayoutMemory,
                          m_desktopOutputLayouts, m_outputDefaultLayouts, output, desktop)));
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
    const QString key = output->name() + QLatin1Char('/') + desktop->id();
    mem.writeEntry(key, LayoutEngine::layoutKindToString(kind));
    m_desktopLayoutMemory.insert(key, kind);
    config->sync();
}

QList<LayoutEngine::LayoutKind> TilingController::enabledLayoutKinds() const
{
    return m_enabledLayoutKinds;
}

bool TilingController::isLayoutEnabled(LayoutEngine::LayoutKind kind) const
{
    return m_enabledLayoutKinds.contains(kind);
}

void TilingController::applyGapSettingsToOutput(LogicalOutput *output, VirtualDesktop *desktop)
{
    if (!m_workspace || !output) {
        return;
    }

    const ReflowScope scope(this, output, ReflowContext::Reason::GapChange,
                            reflowScopeLayoutKind(output, desktop));

    TileManager *manager = m_workspace->tileManager(output);
    if (!manager) {
        return;
    }

    const CachedGaps gaps = m_outputGaps.value(output->name(), m_gapDefaults);
    const QMarginsF gapMargins(gaps.gapLeft, gaps.gapTop, gaps.gapRight, gaps.gapBottom);

    auto applyToDesktop = [&](VirtualDesktop *desk) {
        if (!desk) {
            return;
        }
        if (RootTile *root = manager->rootTile(desk)) {
            LayoutEngine *eng = manager->layoutEngine(desk);
            const int n = eng ? eng->windows().count() : 0;
            if (tilingconfig::shouldSuppressGaps(m_gapsSuppressed, n)) {
                // No gaps: either the user toggled them off, or smart gaps
                // (no indent/between for a single or empty layout).
                root->setGapBetween(0);
                root->setGapMargins({});
            } else {
                root->setGapBetween(gaps.gapBetween);
                root->setGapMargins(gapMargins);
            }
            if (eng) {
                eng->reflow();
            }
        }
    };

    if (desktop) {
        applyToDesktop(desktop);
        return;
    }
    for (VirtualDesktop *desk : VirtualDesktopManager::self()->desktops()) {
        applyToDesktop(desk);
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

    // Maximizing a tiled window must drop it from its layout (ghost-tile),
    // mirroring minimize. Maximize goes through Window::exitQuickTileMode(),
    // which detaches the leaf via Tile::forget(); vacateLayout then
    // pruneEmpty()s only the home engine (shouldHandleRemove is false after
    // forget, so we must not walk every engine). Mode stays Tiled so
    // unmaximize re-joins. Fullscreen is NOT handled here: it keeps tile
    // membership so geometry restores on exit.
    connect(window, &Window::maximizedChanged, this,
            [this, window]() {
                onWindowMaximizedChanged(window);
                sanitizeVideoBridgeSurface(window);
            });

    // XWayland clients set WM_CLASS asynchronously after map. initialMode()
    // below can miss IgnoreClass (e.g. xwaylandvideobridge) and tile anyway.
    // Re-evaluate when the class arrives.
    connect(window, &Window::windowClassChanged, this,
            [this, window]() { onWindowClassChanged(window); });

    // The bridge capture surface is documented transparent; a boot-time
    // _NET_WM_WINDOW_OPACITY race can leave Window::opacity() at 1.0 (black
    // box). Re-sanitize on opacity changes. isVideoBridgeSurface() is
    // evaluated at signal time so a late WM_CLASS is still honored.
    connect(window, &Window::opacityChanged, this,
            [this, window]() { sanitizeVideoBridgeSurface(window); });

    // Defensive: if a window is ever torn down without routing through
    // Workspace::removeWindow -> onWindowRemoved, still scrub the per-window
    // state maps keyed by its (now-dangling) pointer. The pointer is only used
    // as a hash key here, never dereferenced.
    connect(window, &QObject::destroyed, this, [this, window]() {
        m_activeMoves.remove(window);
        m_activeResizes.remove(window);
        m_preTileGeometry.remove(window);
        m_engineByWindow.remove(window);
    });

    // Don't touch already-managed windows (e.g. on-all-desktops already handled).
    if (window->tilingState().mode != TilingState::Mode::Floating) {
        return;
    }

    const TilingState::Mode mode = m_rules->initialMode(window);

    if (mode == TilingState::Mode::Tiled) {
        LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();

        // Per-app output assignment ([TilingRules] AssignOutput): pin this
        // window's class to a specific monitor if a rule matches and that
        // output is connected. Falls back to the normal output otherwise.
        //
        // Do this while the window is still Floating: sendToOutput emits
        // outputChanged synchronously, and onWindowOutputChanged only migrates
        // windows that are already Tiled — so doing it before we mark the mode
        // avoids a double add (migrate + addWindowToLayout below).
        const QString assigned = m_rules->outputForWindow(window);
        if (!assigned.isEmpty()) {
            if (LogicalOutput *target = outputByName(assigned)) {
                if (target != output) {
                    window->sendToOutput(target);
                    output = target;
                }
            }
        }

        window->tilingState().mode = TilingState::Mode::Tiled;
        // Created already minimized or maximized must not take a tile: that
        // would leave a ghost slot. Re-join on unminimize/unmaximize.
        if (window->isMinimized() || window->maximizeMode() != MaximizeRestore) {
            return;
        }
        VirtualDesktop *desktop = window->desktops().isEmpty()
            ? VirtualDesktopManager::self()->currentDesktop(output)
            : window->desktops().constFirst();
        addWindowToLayout(window, output, desktop);
        // Opt-in ([Tiling] NewWindowPlacement=master): make the freshly opened
        // window the master instead of appending it at the tail. Skip when a
        // master pin is active on this (output, desktop) so opening a window
        // doesn't steal master from the pinned one. (No-op on layouts without a
        // master concept, e.g. Scrolling.)
        if (m_newWindowMaster && output && desktop) {
            const bool pinActive = shouldTile(m_masterPins.value(pinKeyFor(output, desktop)));
            if (!pinActive) {
                promoteToMaster(window);
            }
        }
        if (output) {
            applyGapSettingsToOutput(output, desktop);
        }
    } else {
        window->tilingState().mode = mode;
        // Floated at map time (ignore/float rule, including the video
        // bridge): still pin opacity if this is the capture surface.
        sanitizeVideoBridgeSurface(window);
    }
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
    m_preTileGeometry.remove(window);
    for (auto it = m_masterPins.begin(); it != m_masterPins.end();) {
        if (it.value().isNull() || it.value() == window) {
            it = m_masterPins.erase(it);
        } else {
            ++it;
        }
    }
    LogicalOutput *out = window->output();
    VirtualDesktop *desktop = nullptr;
    layoutEngineForWindow(window, nullptr, &desktop);
    removeWindowFromLayouts(window);
    if (out) {
        applyGapSettingsToOutput(out, desktop);
        for (VirtualDesktop *desk : VirtualDesktopManager::self()->desktops()) {
            reassertMasterPin(out, desk);
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

    const LayoutEngine::LayoutKind kind = layoutKindFor(output, desktop);
    setupLayoutEngine(output, manager, desktop, kind);

    LayoutEngine *engine = manager->layoutEngine(desktop);
    if (!engine) {
        qWarning() << "TilingController: failed to obtain engine for output"
                   << output->name() << "desktop" << desktop->id();
        return;
    }

    const ReflowScope scope(this, output, ReflowContext::Reason::Add, kind);
    // Remember geometry before the engine snaps it to a tile. If we later
    // bail an ignored system surface (xwaylandvideobridge) after WM_CLASS
    // arrives, restore this instead of leaving it stretched as a black box.
    m_preTileGeometry.insert(window, window->moveResizeGeometry());
    engine->addWindow(window);

    // If the window did not end up managed, surface it in logs but do NOT flip
    // the mode to Floating: the caller owns the mode, and the next
    // desktop/output change re-evaluates and snaps the tile correctly.
    if (!engine->contains(window)) {
        qWarning() << "TilingController: window" << window->caption()
                   << "was not managed by any layout engine after addWindow; leaving mode untouched";
        return;
    }
    bindWindowToEngine(window, engine, output, desktop);
    forceNoBorder(window);
    reassertMasterPin(output, desktop);
}

void TilingController::migrateWindow(Window *window, LogicalOutput *newOutput, VirtualDesktop *newDesktop)
{
    if (!m_workspace || !window || !newOutput || !newDesktop) {
        return;
    }

    // Find whichever engine currently owns this window, if any (O(1) reverse index).
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
        unbindWindowFromEngine(window);
    }
    // Smart-gaps on the source desktop even when the output did not change
    // (desktop-only migrate). Previously applyGapSettingsToOutput(newOutput)
    // reflowed every desktop on the destination, which covered the source as a
    // side effect; now each call is scoped to one desktop.
    if (oldOutput && oldDesktop) {
        applyGapSettingsToOutput(oldOutput, oldDesktop);
    }
    {
        const ReflowScope migrateScope(this, newOutput, ReflowContext::Reason::Migrate,
                                       layoutKindFor(newOutput, newDesktop));
        addWindowToLayout(window, newOutput, newDesktop);
    }
    applyGapSettingsToOutput(newOutput, newDesktop);

    if (pinFollows && shouldTile(window)) {
        m_masterPins.insert(pinKeyFor(newOutput, newDesktop), window);
    }
    if (oldOutput && oldDesktop) {
        reassertMasterPin(oldOutput, oldDesktop);
    }
    reassertMasterPin(newOutput, newDesktop);
}

void TilingController::bindWindowToEngine(Window *window, LayoutEngine *engine, LogicalOutput *output, VirtualDesktop *desktop)
{
    if (!window || !engine) {
        return;
    }
    m_engineByWindow.insert(window, {engine, output, desktop});
}

void TilingController::unbindWindowFromEngine(Window *window)
{
    if (!window) {
        return;
    }
    m_engineByWindow.remove(window);
}

void TilingController::unbindEngineWindows(LayoutEngine *engine)
{
    if (!engine) {
        return;
    }
    for (auto it = m_engineByWindow.begin(); it != m_engineByWindow.end();) {
        if (it->engine == engine) {
            it = m_engineByWindow.erase(it);
        } else {
            ++it;
        }
    }
}

void TilingController::unbindOutputWindows(LogicalOutput *output)
{
    if (!output) {
        return;
    }
    for (auto it = m_engineByWindow.begin(); it != m_engineByWindow.end();) {
        if (it->output == output) {
            it = m_engineByWindow.erase(it);
        } else {
            ++it;
        }
    }
}

void TilingController::removeWindowFromLayouts(Window *window)
{
    if (!m_workspace) {
        return;
    }

    restoreBorder(window);

    LayoutEngine *engine = nullptr;
    LogicalOutput *output = nullptr;
    const auto it = m_engineByWindow.find(window);
    if (it != m_engineByWindow.end()) {
        engine = it->engine;
        output = it->output;
        m_engineByWindow.erase(it);
    }
    if (engine) {
        // Reverse-index hit includes ghost leaves (index stays bound through
        // interactive move). shouldHandleRemove still gates reflow so a stale
        // index cannot reflow an engine that already dropped the window.
        if (engine->shouldHandleRemove(window)) {
            const ReflowScope scope(this, output, ReflowContext::Reason::Remove,
                                    reflowScopeLayoutKind(output));
            engine->removeWindow(window);
            // Maximize forget() (and similar) can leave an empty leaf that
            // removeWindow cannot see. pruneEmpty on this owning engine only.
            engine->pruneEmpty();
        }
        return;
    }

    // Index miss (shouldn't happen): scan with the ownership gate so a
    // desynced window still leaves the owning engine without reflowing others.
    for (LogicalOutput *out : m_workspace->outputs()) {
        TileManager *manager = m_workspace->tileManager(out);
        if (!manager) {
            continue;
        }
        // contains() alone misses mid-drag ghost leaves (KWin untiles the
        // window). Only engines that still hold the window or own its ghost
        // leaf should remove + reflow; others stay untouched (#11).
        QList<LayoutEngine *> engines;
        for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
            if (LayoutEngine *eng = manager->layoutEngine(desktop)) {
                if (eng->shouldHandleRemove(window)) {
                    engines.append(eng);
                }
            }
        }
        if (engines.isEmpty()) {
            continue;
        }
        const ReflowScope scope(this, out, ReflowContext::Reason::Remove,
                                reflowScopeLayoutKind(out));
        for (LayoutEngine *eng : engines) {
            eng->removeWindow(window);
            // Maximize forget() (and similar) can leave an empty leaf that
            // removeWindow cannot see. pruneEmpty on this owning engine only
            // — not on foreign engines (#11 + #30).
            eng->pruneEmpty();
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

LogicalOutput *TilingController::outputByName(const QString &name) const
{
    if (!m_workspace || name.isEmpty()) {
        return nullptr;
    }
    for (LogicalOutput *output : m_workspace->outputs()) {
        if (output && output->name().compare(name, Qt::CaseInsensitive) == 0) {
            return output;
        }
    }
    return nullptr;
}

LayoutEngine *TilingController::layoutEngineForWindow(Window *window, LogicalOutput **output, VirtualDesktop **desktop) const
{
    if (!window) {
        return nullptr;
    }

    const auto it = m_engineByWindow.constFind(window);
    if (it != m_engineByWindow.cend()) {
        if (LayoutEngine *engine = it->engine) {
            if (output) {
                *output = it->output;
            }
            if (desktop) {
                *desktop = it->desktop;
            }
            return engine;
        }
    }

    if (!m_workspace) {
        return nullptr;
    }

    // Index miss or destroyed engine: fall back to a contains() scan (no QList).
    for (LogicalOutput *out : m_workspace->outputs()) {
        TileManager *manager = m_workspace->tileManager(out);
        if (!manager) {
            continue;
        }
        for (VirtualDesktop *desk : VirtualDesktopManager::self()->desktops()) {
            if (LayoutEngine *engine = manager->layoutEngine(desk)) {
                if (engine->contains(window)) {
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

    // Prefer the window nearest the shared edge (geometry), not layout-order
    // first/last — wrong on Grid/Scrolling and after flipMaster.
    std::vector<masterstackmath::Rect> rects;
    rects.reserve(static_cast<size_t>(ws.size()));
    const RectF adjGeom = adjacent->geometryF();
    for (Window *w : ws) {
        if (!w) {
            rects.push_back({0, 0, 0, 0});
            continue;
        }
        const RectF g = w->frameGeometry();
        // Output-relative 0..1 for pure entry helper.
        const double rw = adjGeom.width() > 0 ? adjGeom.width() : 1.0;
        const double rh = adjGeom.height() > 0 ? adjGeom.height() : 1.0;
        rects.push_back({
            (g.x() - adjGeom.x()) / rw,
            (g.y() - adjGeom.y()) / rh,
            g.width() / rw,
            g.height() / rh,
        });
    }
    directionmath::Direction entry = directionmath::Direction::Right;
    switch (direction) {
    case LayoutEngine::FocusDirection::Left:
        entry = directionmath::Direction::Left;
        break;
    case LayoutEngine::FocusDirection::Right:
        entry = directionmath::Direction::Right;
        break;
    case LayoutEngine::FocusDirection::Up:
        entry = directionmath::Direction::Up;
        break;
    case LayoutEngine::FocusDirection::Down:
        entry = directionmath::Direction::Down;
        break;
    }
    const int idx = masterstackmath::entryWindowIndex(rects, entry);
    if (idx < 0 || idx >= ws.size()) {
        return ws.first();
    }
    return ws.at(idx);
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

    const qreal ratio = engine->primarySplit();
    const auto kind = static_cast<sizingpolicy::LayoutKind>(engine->layoutKind());
    if (!sizingpolicy::shouldWriteMasterRatio(kind, ratio)) {
        return;
    }
    LogicalOutput *output = nullptr;
    VirtualDesktop *desktop = nullptr;
    layoutEngineForWindow(window, &output, &desktop);
    if (!output) {
        output = window->output() ? window->output() : m_workspace->activeOutput();
    }
    if (!desktop && output) {
        desktop = VirtualDesktopManager::self()->currentDesktop(output);
    }
    const CachedSizing sizing = resolvedSizing(output, desktop);
    if (qFuzzyCompare(ratio, sizing.masterRatio)) {
        return;
    }
    persistMasterRatio(output, desktop, ratio);
}

void TilingController::onWindowMoveFinished(Window *window)
{
    if (!window || !m_workspace) {
        return;
    }

    auto it = m_activeMoves.find(window);
    const bool hasContext = it != m_activeMoves.end();
    LogicalOutput *currentOutput = window->output() ? window->output() : m_workspace->activeOutput();
    const bool outputChanged = hasContext && it.value().output && currentOutput
        && it.value().output != currentOutput;
    const movefsm::FinishKind finish = movefsm::classifyFinish({
        hasContext,
        window->tilingState().mode == TilingState::Mode::Tiled,
        outputChanged,
    });

    // Branching is pure (movefsm); only Workspace/engine effects live here.
    if (finish == movefsm::FinishKind::FloatedAway) {
        m_activeMoves.remove(window);
        return;
    }

    if (finish == movefsm::FinishKind::CrossOutputDrop || finish == movefsm::FinishKind::SameOutputDrop) {
        MoveContext context = it.value();
        m_activeMoves.erase(it);

        const QPointF cursorPos = Cursors::self()->mouse()->pos();
        const RectF area = m_workspace->clientArea(PlacementArea, window);

        if (finish == movefsm::FinishKind::CrossOutputDrop && context.engine) {
            // Left original output: destroy empty source leaf, drop on destination
            // at cursor (not always append).
            unbindWindowFromEngine(window);
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
                    if (destEngine->contains(window)) {
                        bindWindowToEngine(window, destEngine, currentOutput, desktop);
                    }
                }
            }
            // If manage() rejected the drop, still place the window.
            if (!layoutEngineForWindow(window)) {
                addWindowToLayout(window, currentOutput, desktop);
            }
            applyGapSettingsToOutput(currentOutput, desktop);
            window->setGeometryRestore(context.originalGeometryRestore);
            return;
        }

        if (context.engine) {
            Window *target = windowUnderCursorInEngine(context.engine);
            if (target == window) {
                target = nullptr;
            }
            if (target) {
                // Scrolling: drop on another column consumes into it (top/bottom
                // half picks the leaf index). Same-column and other layouts keep
                // swap-on-drop via endMoveWindow. Distinct from Columns #32.
                if (context.engine->dropConsumesIntoTarget(window, target)) {
                    context.engine->cancelMoveWindow(window);
                    context.engine->dropWindow(window, target, cursorPos, area);
                    context.engine->pruneEmpty();
                    window->setGeometryRestore(context.originalGeometryRestore);
                    return;
                }
                if (context.engine->endMoveWindow(window, target)) {
                    context.engine->pruneEmpty();
                    window->setGeometryRestore(context.originalGeometryRestore);
                    return;
                }
            } else {
                // Empty-space drop (or same-spot release): cancelMove clears the
                // ghost source leaf KWin left after untile-for-drag, then insert.
                context.engine->cancelMoveWindow(window);
                context.engine->dropWindow(window, nullptr, cursorPos, area);
                context.engine->pruneEmpty();
                if (context.engine->contains(window)) {
                    VirtualDesktop *desktop = window->desktops().isEmpty()
                        ? VirtualDesktopManager::self()->currentDesktop(currentOutput)
                        : window->desktops().constFirst();
                    bindWindowToEngine(window, context.engine, currentOutput, desktop);
                }
                window->setGeometryRestore(context.originalGeometryRestore);
                return;
            }
        }
        // Engine could not finish the move; fall through to snap-back.
    }

    // NotOurs, or engine fallthrough: re-tile only if not actually in a leaf.
    // The reverse index stays bound through interactive-move ghost leaves, so
    // membership here is contains() (in a leaf), not merely "owned by an engine".
    if (LayoutEngine *eng = layoutEngineForWindow(window)) {
        if (eng->contains(window)) {
            return;
        }
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
        vacateLayout(window);
    } else {
        rejoinLayout(window);
    }
}

void TilingController::onWindowMaximizedChanged(Window *window)
{
    if (!m_enabled || !m_workspace || !window) {
        return;
    }

    // Only tiled windows belong to a layout; floating ones are never in an
    // engine, so maximizing them is none of our business.
    if (window->tilingState().mode != TilingState::Mode::Tiled) {
        return;
    }

    if (window->maximizeMode() != MaximizeRestore) {
        // Leave the layout (ghost-tile) like minimize. Mode stays Tiled so
        // unmaximize re-joins. Always vacate: maximize already forgot the
        // leaf, so layoutEngineForWindow / shouldHandleRemove may already be
        // false; vacateLayout pruneEmpty()s the home engine in that case.
        vacateLayout(window);
    } else {
        rejoinLayout(window);
    }
}

void TilingController::onWindowClassChanged(Window *window)
{
    if (!m_enabled || !m_workspace || !window) {
        return;
    }

    // Only windows tiled before their class was known need a correction.
    if (window->tilingState().mode != TilingState::Mode::Tiled) {
        return;
    }

    if (!m_rules->isIgnored(window)) {
        return;
    }

    LogicalOutput *out = window->output();
    VirtualDesktop *desktop = nullptr;
    layoutEngineForWindow(window, nullptr, &desktop);
    removeWindowFromLayouts(window);
    window->tilingState().mode = TilingState::Mode::Floating;
    if (out) {
        applyGapSettingsToOutput(out, desktop);
    }

    // Unmaximize + opacity 0 before restoring pre-snap geometry:
    // setMaximize(false, false) applies the geometry-restore rect itself.
    sanitizeVideoBridgeSurface(window);

    auto it = m_preTileGeometry.find(window);
    if (it != m_preTileGeometry.end()) {
        const RectF preTileGeometry = it.value();
        m_preTileGeometry.erase(it);
        if (!preTileGeometry.isEmpty()) {
            window->moveResize(preTileGeometry);
        }
    }
}

void TilingController::sanitizeVideoBridgeSurface(Window *window)
{
    if (!window || !m_rules || !m_rules->isVideoBridgeSurface(window)) {
        return;
    }

    // Maximized windows are direct-scanout candidates; scanout bypasses
    // opacity blending, so even opacity 0 would show as opaque black.
    if (window->maximizeMode() != MaximizeRestore) {
        window->setMaximize(false, false);
    }

    // KWin reads _NET_WM_WINDOW_OPACITY once at map time. If the bridge set
    // the property after that, Window::opacity() stays at 1.0. Idempotent:
    // setOpacity early-returns on no change, so opacityChanged cannot recurse.
    if (window->opacity() != 0.0) {
        window->setOpacity(0.0);
    }
}

void TilingController::vacateLayout(Window *window)
{
    LogicalOutput *out = window ? window->output() : nullptr;
    VirtualDesktop *desktop = nullptr;
    LayoutEngine *home = layoutEngineForWindow(window, &out, &desktop);

    removeWindowFromLayouts(window);

    // Maximize already forgot the leaf (exitQuickTileMode), so the window is
    // neither in windows() nor a drag ghost — shouldHandleRemove is false and
    // removeWindowFromLayouts skipped every engine. Prune only the home
    // (output, desktop) engine so that orphaned empty leaf is destroyed
    // without reflowing foreign engines (#11 + #30).
    if (!home) {
        if (!out && window) {
            out = window->output();
        }
        if (!desktop && window && out) {
            desktop = window->desktops().isEmpty()
                ? VirtualDesktopManager::self()->currentDesktop(out)
                : window->desktops().constFirst();
        }
        if (out && desktop && m_workspace) {
            if (TileManager *manager = m_workspace->tileManager(out)) {
                home = manager->layoutEngine(desktop);
            }
        }
    }
    if (home) {
        home->pruneEmpty();
    }
    if (out) {
        applyGapSettingsToOutput(out, desktop);
    }
}

void TilingController::rejoinLayout(Window *window)
{
    if (!shouldTile(window) || layoutEngineForWindow(window)) {
        return;
    }
    LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
    VirtualDesktop *desktop = window->desktops().isEmpty()
        ? VirtualDesktopManager::self()->currentDesktop(output)
        : window->desktops().constFirst();
    addWindowToLayout(window, output, desktop);
    if (output) {
        applyGapSettingsToOutput(output, desktop);
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
        if (window->hitTest(pos) && engine->contains(window)) {
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
    // Rotate to index 0 so intermediates shift down. moveWindow is a pairwise
    // swap — with masterCount > 1 or a window deep in the stack that would
    // leave the order wrong (issue #16).
    engine->reorderWindow(window, -idx);
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
    if (!engine || !engine->contains(pinned)) {
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
            // Scrolling: Up/Down reorder inside the column; Left/Right slide it.
            // Other engines' moveWindowInColumn defaults to moveWindow.
            if (direction == LayoutEngine::FocusDirection::Up
                || direction == LayoutEngine::FocusDirection::Down) {
                engine->moveWindowInColumn(window, tgt - cur);
            } else {
                engine->moveWindow(window, tgt - cur);
            }
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
    seedEngineSizing(output, desktop, engine.get(), kind);
    unbindEngineWindows(existing);
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
        if (fresh->contains(w)) {
            bindWindowToEngine(w, fresh, output, desktop);
        }
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
    const qreal split = engine->primarySplit();
    if (!sizingpolicy::canResizePrimary(split)) {
        return;
    }
    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop(output);
    CachedSizing sizing = resolvedSizing(output, desktop);
    sizing.masterRatio = tilingconfig::clampMasterRatio(sizing.masterRatio + delta);
    engine->setPrimarySplit(sizing.masterRatio);

    // Persist MasterRatio only for master-style layouts (policy unit-tested).
    const auto kind = static_cast<sizingpolicy::LayoutKind>(engine->layoutKind());
    if (sizingpolicy::shouldWriteMasterRatio(kind, engine->primarySplit())) {
        persistMasterRatio(output, desktop, sizing.masterRatio);
    }
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
    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop(output);
    CachedSizing sizing = resolvedSizing(output, desktop);
    sizing.masterCount = tilingconfig::clampMasterCount(sizing.masterCount + delta);
    engine->setPrimaryCount(sizing.masterCount);

    persistMasterCount(output, desktop, sizing.masterCount);
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
    VirtualDesktop *desktop = output ? VirtualDesktopManager::self()->currentDesktop(output) : nullptr;
    constexpr qreal kResetRatio = 0.5;
    persistMasterRatio(output, desktop, kResetRatio);
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

void TilingController::cycleColumnWidthReverse()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->cycleColumnWidthReverse();
    }
}

void TilingController::expandColumnToAvailableWidth()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->expandColumnToAvailableWidth();
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
    consumeIntoColumn();
}

void TilingController::expelWindow()
{
    expelFromColumn();
}

void TilingController::consumeIntoColumn()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->consumeIntoColumn();
    }
}

void TilingController::expelFromColumn()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->expelFromColumn();
    }
}

void TilingController::consumeOrExpelWindowLeft()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->consumeOrExpelWindowLeft();
    }
}

void TilingController::consumeOrExpelWindowRight()
{
    Window *window = activeTiledWindow();
    LayoutEngine *engine = window ? layoutEngineForWindow(window) : activeLayoutEngine();
    if (engine) {
        engine->consumeOrExpelWindowRight();
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
    // The window left oldOutput: drop it from the engine that owns it (or
    // every engine on that output if the index missed) so the source layout
    // reflows. shouldHandleRemove is true for the owning engine even after
    // KWin untiles the window for a drag; a contains() guard would skip that
    // cleanup and leak a phantom tile.
    VirtualDesktop *oldDesktop = nullptr;
    const auto bound = m_engineByWindow.find(window);
    if (bound != m_engineByWindow.end() && bound->output == oldOutput) {
        oldDesktop = bound->desktop;
        if (LayoutEngine *engine = bound->engine) {
            if (engine->shouldHandleRemove(window)) {
                engine->removeWindow(window); // window in a leaf or its ghost
            }
            engine->pruneEmpty();
        }
        m_engineByWindow.erase(bound);
        applyGapSettingsToOutput(oldOutput, oldDesktop);
    } else if (TileManager *manager = m_workspace->tileManager(oldOutput)) {
        if (!window->isOnAllDesktops() && window->desktops().size() == 1) {
            oldDesktop = window->desktops().constFirst();
        }
        for (VirtualDesktop *desktop : VirtualDesktopManager::self()->desktops()) {
            if (LayoutEngine *engine = manager->layoutEngine(desktop)) {
                // contains() misses the untile-for-drag ghost; ownsGhostLeaf
                // keeps removeWindow on the source engine so cancelMove/prune
                // still run. Engines that never held this window do not reflow.
                if (engine->shouldHandleRemove(window)) {
                    engine->removeWindow(window); // window in a leaf or its ghost
                }
                engine->pruneEmpty(); // KWin may still have left an empty leaf
            }
        }
        applyGapSettingsToOutput(oldOutput, oldDesktop);
    }

    // For non-interactive output changes (e.g. menu "move to screen", direct
    // sendToOutput, or other actions), place the window into the destination
    // layout if it is tiled. Interactive drags are handled at move-finish with
    // dropWindow for insertion position.
    if (movefsm::deferMigrateOnOutputChange(m_activeMoves.contains(window),
                                            m_activeResizes.contains(window))) {
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
    applyGapSettingsToOutput(newOutput, desktop);
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
        unbindEngineWindows(existing);
    }
    auto engine = createLayoutEngine(kind, manager);
    seedEngineSizing(output, desktop, engine.get(), kind);
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
        // Do not give minimized/maximized windows a slot (ghost tile). They
        // re-join via onWindowMinimizedChanged / onWindowMaximizedChanged.
        if (w->isMinimized() || w->maximizeMode() != MaximizeRestore) {
            continue;
        }
        fresh->addWindow(w);
        if (fresh->contains(w)) {
            bindWindowToEngine(w, fresh, output, desktop);
        }
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
        state.suspendedByDisable = false;
        removeWindowFromLayouts(window);
    } else {
        if (!m_enabled) {
            return;
        }
        state.mode = TilingState::Mode::Tiled;
        state.suspendedByDisable = false;
        LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
        VirtualDesktop *desktop = window->desktops().isEmpty()
            ? VirtualDesktopManager::self()->currentDesktop(output)
            : window->desktops().constFirst();
        addWindowToLayout(window, output, desktop);
    }
}

void TilingController::suspendAllTiledWindows()
{
    if (!m_workspace) {
        return;
    }
    for (Window *window : m_workspace->windows()) {
        if (!window || window->isDeleted()) {
            continue;
        }
        TilingState &state = window->tilingState();
        suspendpolicy::WindowFlags in{
            state.mode == TilingState::Mode::Tiled ? suspendpolicy::Mode::Tiled
                                                   : suspendpolicy::Mode::Floating,
            state.suspendedByDisable,
        };
        const suspendpolicy::SuspendResult r = suspendpolicy::onSuspend(in);
        if (!r.removeFromLayout && !r.restoreBorder) {
            continue;
        }
        if (r.removeFromLayout) {
            removeWindowFromLayouts(window);
        }
        if (r.restoreBorder) {
            restoreBorder(window);
        }
        state.mode = (r.mode == suspendpolicy::Mode::Tiled) ? TilingState::Mode::Tiled
                                                            : TilingState::Mode::Floating;
        state.suspendedByDisable = r.suspendedByDisable;
    }
    m_activeMoves.clear();
    m_activeResizes.clear();
    m_masterPins.clear();
}

void TilingController::resumeSuspendedWindows()
{
    if (!m_workspace || !m_enabled) {
        return;
    }
    initializeLayouts();
    for (Window *window : m_workspace->windows()) {
        if (!window || window->isDeleted()) {
            continue;
        }
        TilingState &state = window->tilingState();
        suspendpolicy::WindowFlags in{
            state.mode == TilingState::Mode::Tiled ? suspendpolicy::Mode::Tiled
                                                   : suspendpolicy::Mode::Floating,
            state.suspendedByDisable,
        };
        const bool rulesWantFloat = m_rules->initialMode(window) == TilingState::Mode::Floating;
        const suspendpolicy::ResumeResult r = suspendpolicy::onResume(in, rulesWantFloat);
        state.suspendedByDisable = r.suspendedByDisable;
        state.mode = (r.mode == suspendpolicy::Mode::Tiled) ? TilingState::Mode::Tiled
                                                            : TilingState::Mode::Floating;
        if (!r.addToLayout) {
            continue;
        }
        if (window->isMinimized() || window->maximizeMode() != MaximizeRestore) {
            continue;
        }
        LogicalOutput *output = window->output() ? window->output() : m_workspace->activeOutput();
        VirtualDesktop *desktop = window->desktops().isEmpty()
            ? VirtualDesktopManager::self()->currentDesktop(output)
            : window->desktops().constFirst();
        addWindowToLayout(window, output, desktop);
    }
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
