/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// Centered layout-name popup for sessions without plasmashell. Mirrors the
// desktop-change OSD positioning model; does not fade on pointer hover.
PlasmaCore.Window {
    id: dialog
    visible: false
    flags: Qt.FramelessWindowHint | Qt.X11BypassWindowManagerHint

    width: mainItem.implicitWidth
    height: mainItem.implicitHeight

    function showAt(centerX, centerY, text, icon) {
        label.text = text
        iconItem.source = icon
        iconItem.visible = icon !== ""
        dialog.visible = true
        Qt.callLater(function() {
            dialog.x = centerX - mainItem.width / 2
            dialog.y = centerY - mainItem.height / 2
            hideTimer.restart()
        })
    }

    mainItem: RowLayout {
        id: mainItem
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            id: iconItem
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: implicitWidth
        }

        PlasmaComponents3.Label {
            id: label
        }
    }

    Timer {
        id: hideTimer
        interval: 1200
        repeat: false
        onTriggered: dialog.visible = false
    }
}