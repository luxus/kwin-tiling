/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace KWin
{

class TilingController;

/**
 * D-Bus façade on TilingController, published on the org.kde.KWin connection
 * at /Tiling as org.kde.KWin.Tiling.
 *
 * KineticWE-shaped where it helps shell widgets (currentLayout,
 * currentLayoutDisplay, enabledLayouts, setLayout, cycleLayout, signals).
 * Extra members wrap the same shortcut actions the controller already exposes.
 *
 * Registration is deliberately cautious: Qt can abort the compositor if
 * registerObject() is invoked on a path that is already claimed. See
 * tilingdbuspolicy.h and the README backlog note.
 */
class TilingDBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.Tiling")

    Q_PROPERTY(QString currentLayout READ currentLayout NOTIFY layoutChanged)
    Q_PROPERTY(QString currentLayoutDisplay READ currentLayoutDisplay NOTIFY layoutChanged)
    Q_PROPERTY(QStringList enabledLayouts READ enabledLayouts NOTIFY enabledLayoutsChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(double masterRatio READ masterRatio NOTIFY masterRatioChanged)
    Q_PROPERTY(int masterCount READ masterCount NOTIFY masterCountChanged)
    Q_PROPERTY(QStringList windows READ windows NOTIFY windowsChanged)

public:
    explicit TilingDBusInterface(TilingController *controller);
    ~TilingDBusInterface() override;

    QString currentLayout() const;
    QString currentLayoutDisplay() const;
    QStringList enabledLayouts() const;
    bool enabled() const;
    double masterRatio() const;
    int masterCount() const;
    QStringList windows() const;

public Q_SLOTS:
    void setLayout(const QString &kind);
    void cycleLayout();
    QString layoutFor(const QString &outputName, const QString &desktopId) const;

    void focusLeft();
    void focusRight();
    void focusUp();
    void focusDown();
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void toggleFloating();
    void resizePrimary(double delta);
    void adjustMasterCount(int delta);
    void toggleGaps();
    void toggleZoom();
    void retile();

Q_SIGNALS:
    void layoutChanged(const QString &layout);
    void enabledLayoutsChanged(const QStringList &layouts);
    void enabledChanged(bool enabled);
    void masterRatioChanged(double ratio);
    void masterCountChanged(int count);
    void windowsChanged(const QStringList &windowIds);

private:
    void tryRegister();
    void emitLayout();
    void emitEnabledLayouts();
    void emitWindows();
    void emitEnabled();
    void emitSizing();

    TilingController *m_controller = nullptr;
    int m_registerAttempts = 0;
    bool m_registered = false;
};

} // namespace KWin
