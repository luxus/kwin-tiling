/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilingosd.h"

#include "core/output.h"
#include "scripting/scripting.h"
#include "workspace.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QVariant>

namespace KWin
{

namespace
{

QObject *tilingOsdDialog()
{
    static QObject *s_dialog = nullptr;
    if (s_dialog) {
        return s_dialog;
    }

    QQmlEngine *engine = Scripting::self()->qmlEngine();
    if (!engine) {
        return nullptr;
    }

    const QString qmlPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                   QStringLiteral("kwin-wayland/onscreennotification/tiling/layoutswitch.qml"));
    if (qmlPath.isEmpty()) {
        return nullptr;
    }

    QQmlComponent component(engine, QUrl::fromLocalFile(qmlPath));
    if (component.isError()) {
        return nullptr;
    }

    s_dialog = component.create();
    if (!s_dialog) {
        return nullptr;
    }

    s_dialog->setParent(engine);
    return s_dialog;
}

} // namespace

void TilingOsd::show(Workspace *workspace, const QString &text, const QString &iconName)
{
    if (!workspace) {
        return;
    }

    QObject *dialog = tilingOsdDialog();
    if (!dialog) {
        return;
    }

    LogicalOutput *output = workspace->activeOutput();
    if (!output) {
        return;
    }

    const RectF area = workspace->clientArea(FullScreenArea, output);
    const qreal centerX = area.x() + area.width() / 2.0;
    const qreal centerY = area.y() + area.height() / 2.0;

    QMetaObject::invokeMethod(dialog,
                              "showAt",
                              Q_ARG(QVariant, centerX),
                              Q_ARG(QVariant, centerY),
                              Q_ARG(QVariant, text),
                              Q_ARG(QVariant, iconName));
}

} // namespace KWin