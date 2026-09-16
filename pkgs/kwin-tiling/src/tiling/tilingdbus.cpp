/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilingdbus.h"
#include "tilingdbuspolicy.h"
#include "tilingcontroller.h"

#include "tiles/layoutengine.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <QDBusConnection>
#include <QStandardPaths>
#include <QTimer>

namespace KWin
{

namespace
{

QString dbusPath()
{
    return QStringLiteral("/Tiling");
}

QString dbusInterface()
{
    return QStringLiteral("org.kde.KWin.Tiling");
}

} // namespace

TilingDBusInterface::TilingDBusInterface(TilingController *controller)
    : QObject(controller)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &TilingController::layoutChanged, this, &TilingDBusInterface::emitLayout);
        connect(m_controller, &TilingController::enabledLayoutsChanged, this, &TilingDBusInterface::emitEnabledLayouts);
        connect(m_controller, &TilingController::tiledWindowsChanged, this, &TilingDBusInterface::emitWindows);
        connect(m_controller, &TilingController::enabledChanged, this, &TilingDBusInterface::emitEnabled);
        connect(m_controller, &TilingController::sizingChanged, this, &TilingDBusInterface::emitSizing);
    }

    // "Current" layout is per (active output, current desktop). Desktop and
    // output switches must notify listeners even when the engine kind is unchanged.
    if (VirtualDesktopManager *vds = VirtualDesktopManager::self()) {
        connect(vds, &VirtualDesktopManager::currentChanged, this, [this]() {
            emitLayout();
            emitWindows();
            emitSizing();
        });
    }
    if (Workspace *ws = Workspace::self()) {
        connect(ws, &Workspace::activeOutputChanged, this, [this]() {
            emitLayout();
            emitWindows();
            emitSizing();
        });
    }

    // Defer off Workspace::init so D-Bus setup (org.kde.KWin service) can
    // finish first. Never registerObject from this constructor.
    QTimer::singleShot(0, this, &TilingDBusInterface::tryRegister);
}

TilingDBusInterface::~TilingDBusInterface()
{
    if (m_registered) {
        QDBusConnection::sessionBus().unregisterObject(dbusPath());
        m_registered = false;
    }
}

void TilingDBusInterface::tryRegister()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const tilingdbus::RegisterDecision decision = tilingdbus::classifyRegister(
        QStandardPaths::isTestModeEnabled(),
        bus.isConnected(),
        bus.objectRegisteredAt(dbusPath()) != nullptr);

    if (tilingdbus::shouldRetry(decision, m_registerAttempts)) {
        ++m_registerAttempts;
        QTimer::singleShot(tilingdbus::kRegisterRetryMs, this, &TilingDBusInterface::tryRegister);
        return;
    }

    switch (decision) {
    case tilingdbus::RegisterDecision::SkipTestMode:
        return;
    case tilingdbus::RegisterDecision::SkipAlreadyRegistered:
        qWarning() << "Tiling D-Bus: /Tiling is already registered; skipping to avoid a compositor abort";
        return;
    case tilingdbus::RegisterDecision::SkipBusDisconnected:
        qWarning() << "Tiling D-Bus: session bus never connected; /Tiling not published";
        return;
    case tilingdbus::RegisterDecision::Register:
        break;
    }

    const auto options = QDBusConnection::ExportAllSlots
        | QDBusConnection::ExportAllProperties
        | QDBusConnection::ExportAllSignals;
    if (!bus.registerObject(dbusPath(), dbusInterface(), this, options)) {
        // Do not retry: a failed registerObject on an occupied path can abort
        // the compositor on some Qt builds.
        qWarning() << "Tiling D-Bus: failed to register /Tiling:" << bus.lastError().message();
        return;
    }
    m_registered = true;
}

QString TilingDBusInterface::currentLayout() const
{
    return m_controller ? m_controller->currentLayoutName() : QString();
}

QString TilingDBusInterface::currentLayoutDisplay() const
{
    return m_controller ? m_controller->currentLayoutDisplayName() : QString();
}

QStringList TilingDBusInterface::enabledLayouts() const
{
    return m_controller ? m_controller->enabledLayoutNames() : QStringList();
}

bool TilingDBusInterface::enabled() const
{
    return m_controller && m_controller->isEnabled();
}

double TilingDBusInterface::masterRatio() const
{
    return m_controller ? m_controller->currentMasterRatio() : 0.5;
}

int TilingDBusInterface::masterCount() const
{
    return m_controller ? m_controller->currentMasterCount() : 1;
}

QStringList TilingDBusInterface::windows() const
{
    return m_controller ? m_controller->tiledWindowIds() : QStringList();
}

void TilingDBusInterface::setLayout(const QString &kind)
{
    if (!m_controller || !tilingdbus::isKnownLayoutName(kind.toStdString())) {
        return;
    }
    m_controller->setLayout(LayoutEngine::layoutKindFromString(kind));
}

void TilingDBusInterface::cycleLayout()
{
    if (m_controller) {
        m_controller->cycleLayout();
    }
}

QString TilingDBusInterface::layoutFor(const QString &outputName, const QString &desktopId) const
{
    return m_controller ? m_controller->layoutNameFor(outputName, desktopId) : QString();
}

void TilingDBusInterface::focusLeft()
{
    if (m_controller) {
        m_controller->focusLeft();
    }
}

void TilingDBusInterface::focusRight()
{
    if (m_controller) {
        m_controller->focusRight();
    }
}

void TilingDBusInterface::focusUp()
{
    if (m_controller) {
        m_controller->focusUp();
    }
}

void TilingDBusInterface::focusDown()
{
    if (m_controller) {
        m_controller->focusDown();
    }
}

void TilingDBusInterface::moveLeft()
{
    if (m_controller) {
        m_controller->moveLeft();
    }
}

void TilingDBusInterface::moveRight()
{
    if (m_controller) {
        m_controller->moveRight();
    }
}

void TilingDBusInterface::moveUp()
{
    if (m_controller) {
        m_controller->moveUp();
    }
}

void TilingDBusInterface::moveDown()
{
    if (m_controller) {
        m_controller->moveDown();
    }
}

void TilingDBusInterface::toggleFloating()
{
    if (m_controller) {
        m_controller->toggleFloating();
    }
}

void TilingDBusInterface::resizePrimary(double delta)
{
    if (m_controller) {
        m_controller->resizePrimary(delta);
    }
}

void TilingDBusInterface::adjustMasterCount(int delta)
{
    if (m_controller) {
        m_controller->adjustMasterCount(delta);
    }
}

void TilingDBusInterface::toggleGaps()
{
    if (m_controller) {
        m_controller->toggleGaps();
    }
}

void TilingDBusInterface::toggleZoom()
{
    if (m_controller) {
        m_controller->toggleZoom();
    }
}

void TilingDBusInterface::retile()
{
    if (m_controller) {
        m_controller->retile();
    }
}

void TilingDBusInterface::emitLayout()
{
    Q_EMIT layoutChanged(currentLayout());
}

void TilingDBusInterface::emitEnabledLayouts()
{
    Q_EMIT enabledLayoutsChanged(enabledLayouts());
}

void TilingDBusInterface::emitWindows()
{
    Q_EMIT windowsChanged(windows());
}

void TilingDBusInterface::emitEnabled()
{
    Q_EMIT enabledChanged(enabled());
}

void TilingDBusInterface::emitSizing()
{
    Q_EMIT masterRatioChanged(masterRatio());
    Q_EMIT masterCountChanged(masterCount());
}

} // namespace KWin
