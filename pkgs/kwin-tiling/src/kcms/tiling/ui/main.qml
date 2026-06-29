/*
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kwin.kcm.tiling

KCM.SimpleKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 42
    implicitHeight: Kirigami.Units.gridUnit * 34

    // Layout choices shared by the default and per-monitor dropdowns.
    readonly property var layoutOptions: [
        { text: i18n("MasterStack"), value: "MasterStack" },
        { text: i18n("Stacked"), value: "Stacked" },
        { text: i18n("Scrolling"), value: "Scrolling" },
        { text: i18n("Centered"), value: "Centered" }
    ]

    // Tabs across the top of the module.
    header: QQC2.TabBar {
        id: tabBar
        QQC2.TabButton { text: i18n("General") }
        QQC2.TabButton { text: i18n("Layout & Gaps") }
        QQC2.TabButton { text: i18n("Rules") }
    }

    StackLayout {
        currentIndex: tabBar.currentIndex
        Layout.fillWidth: true

        // ====================================================================
        // General
        // ====================================================================
        Kirigami.FormLayout {
            QQC2.CheckBox {
                id: enableTiling
                Kirigami.FormData.label: i18n("Tiling:")
                text: i18n("Enable tiling")
                checked: kcm.settings.enabled
                onToggled: kcm.settings.enabled = checked
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "enabled"
                }
            }

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18n("Available Layouts")
            }

            QQC2.CheckBox {
                id: enableMasterStack
                Kirigami.FormData.label: i18n("Enabled:")
                text: i18n("MasterStack")
                checked: kcm.settings.enabledLayouts.indexOf("MasterStack") !== -1
                onToggled: {
                    let layouts = kcm.settings.enabledLayouts.slice();
                    if (checked && layouts.indexOf("MasterStack") === -1) {
                        layouts.push("MasterStack");
                    } else if (!checked) {
                        layouts = layouts.filter(l => l !== "MasterStack");
                    }
                    kcm.settings.enabledLayouts = layouts;
                }
            }

            QQC2.CheckBox {
                id: enableStacked
                text: i18n("Stacked")
                checked: kcm.settings.enabledLayouts.indexOf("Stacked") !== -1
                onToggled: {
                    let layouts = kcm.settings.enabledLayouts.slice();
                    if (checked && layouts.indexOf("Stacked") === -1) {
                        layouts.push("Stacked");
                    } else if (!checked) {
                        layouts = layouts.filter(l => l !== "Stacked");
                    }
                    kcm.settings.enabledLayouts = layouts;
                }
            }

            QQC2.CheckBox {
                id: enableScrolling
                text: i18n("Scrolling")
                checked: kcm.settings.enabledLayouts.indexOf("Scrolling") !== -1
                onToggled: {
                    let layouts = kcm.settings.enabledLayouts.slice();
                    if (checked && layouts.indexOf("Scrolling") === -1) {
                        layouts.push("Scrolling");
                    } else if (!checked) {
                        layouts = layouts.filter(l => l !== "Scrolling");
                    }
                    kcm.settings.enabledLayouts = layouts;
                }
            }

            QQC2.CheckBox {
                id: enableCentered
                text: i18n("Centered")
                checked: kcm.settings.enabledLayouts.indexOf("Centered") !== -1
                onToggled: {
                    let layouts = kcm.settings.enabledLayouts.slice();
                    if (checked && layouts.indexOf("Centered") === -1) {
                        layouts.push("Centered");
                    } else if (!checked) {
                        layouts = layouts.filter(l => l !== "Centered");
                    }
                    kcm.settings.enabledLayouts = layouts;
                }
            }

            QQC2.Label {
                visible: kcm.settings.enabledLayouts.length < 2
                Kirigami.FormData.label: i18nc("@info", "Note:")
                text: i18nc("@info", "Enable at least one layout. If none are enabled, the default layout is used.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                opacity: 0.7
            }

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18n("Floating Windows")
            }

            QQC2.CheckBox {
                id: floatAbove
                Kirigami.FormData.label: i18n("Floating windows:")
                text: i18n("Keep above tiled windows")
                checked: kcm.settings.floatAbove
                onToggled: kcm.settings.floatAbove = checked
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "floatAbove"
                }
            }
        }

        // ====================================================================
        // Layout & Gaps (defaults + per-monitor overrides)
        // ====================================================================
        Kirigami.FormLayout {
            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18n("Defaults")
            }

            QQC2.Label {
                Kirigami.FormData.label: i18nc("@info", "Applies to:")
                text: i18nc("@info", "These defaults apply to every monitor that has no custom settings below.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 30
                opacity: 0.7
            }

            QQC2.ComboBox {
                id: defaultLayout
                Kirigami.FormData.label: i18n("Default layout:")
                model: root.layoutOptions
                textRole: "text"
                valueRole: "value"
                currentIndex: {
                    const idx = model.findIndex(item => item.value === kcm.settings.defaultLayout);
                    return idx >= 0 ? idx : 0;
                }
                onActivated: kcm.settings.defaultLayout = currentValue
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "defaultLayout"
                }
            }

            QQC2.SpinBox {
                id: masterRatio
                Kirigami.FormData.label: i18n("Master / centre width (%):")
                from: 10
                to: 90
                stepSize: 5
                value: Math.round(kcm.settings.masterRatio * 100)
                onValueModified: kcm.settings.masterRatio = value / 100
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "masterRatio"
                }
            }

            QQC2.SpinBox {
                id: masterCount
                Kirigami.FormData.label: i18n("Master count:")
                from: 1
                to: 10
                value: kcm.settings.masterCount
                onValueModified: kcm.settings.masterCount = value
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "masterCount"
                }
            }

            QQC2.SpinBox {
                id: defaultColumnWidth
                Kirigami.FormData.label: i18n("Scrolling column width (%):")
                from: 10
                to: 100
                stepSize: 5
                value: Math.round(kcm.settings.defaultColumnWidth * 100)
                onValueModified: kcm.settings.defaultColumnWidth = value / 100
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "defaultColumnWidth"
                }
            }

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18n("Default Gaps")
            }

            QQC2.SpinBox {
                id: gapLeft
                Kirigami.FormData.label: i18n("Left:")
                from: 0
                to: 1000
                value: kcm.settings.gapLeft
                onValueModified: kcm.settings.gapLeft = value
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "gapLeft"
                }
            }

            QQC2.SpinBox {
                id: gapRight
                Kirigami.FormData.label: i18n("Right:")
                from: 0
                to: 1000
                value: kcm.settings.gapRight
                onValueModified: kcm.settings.gapRight = value
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "gapRight"
                }
            }

            QQC2.SpinBox {
                id: gapTop
                Kirigami.FormData.label: i18n("Top:")
                from: 0
                to: 1000
                value: kcm.settings.gapTop
                onValueModified: kcm.settings.gapTop = value
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "gapTop"
                }
            }

            QQC2.SpinBox {
                id: gapBottom
                Kirigami.FormData.label: i18n("Bottom:")
                from: 0
                to: 1000
                value: kcm.settings.gapBottom
                onValueModified: kcm.settings.gapBottom = value
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "gapBottom"
                }
            }

            QQC2.SpinBox {
                id: gapBetween
                Kirigami.FormData.label: i18n("Between tiles:")
                from: 0
                to: 1000
                value: kcm.settings.gapBetween
                onValueModified: kcm.settings.gapBetween = value
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "gapBetween"
                }
            }

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18n("Per-Monitor Overrides")
            }

            QQC2.Label {
                Kirigami.FormData.label: i18nc("@info", "How it works:")
                text: i18nc("@info", "Add custom layout and gaps for a specific monitor. Monitors not listed here follow the defaults above.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 30
                opacity: 0.7
            }

            RowLayout {
                Kirigami.FormData.label: i18n("Add for:")
                spacing: Kirigami.Units.smallSpacing

                QQC2.ComboBox {
                    id: monitorPicker
                    Layout.fillWidth: true
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 12
                    model: kcm.gapOverridesModel.availableMonitors
                    textRole: "description"
                    valueRole: "name"
                    enabled: count > 0
                    displayText: count > 0 ? currentText : i18n("All monitors already customized")
                }
                QQC2.Button {
                    text: i18n("Add custom settings")
                    icon.name: "list-add"
                    enabled: monitorPicker.count > 0
                    onClicked: kcm.gapOverridesModel.addMonitor(monitorPicker.currentValue, kcm.settings)
                }
            }

            Repeater {
                id: perMonitorRepeater
                model: kcm.gapOverridesModel

                delegate: QQC2.Frame {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing

                    property var entry: model.entry
                    property string monitorName: model.name
                    property string monitorDescription: model.description

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            QQC2.Label {
                                text: monitorDescription + (monitorName ? "  (" + monitorName + ")" : "")
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            QQC2.ToolButton {
                                icon.name: "list-remove"
                                display: QQC2.AbstractButton.IconOnly
                                text: i18n("Remove custom settings (use defaults)")
                                QQC2.ToolTip.text: text
                                QQC2.ToolTip.visible: hovered
                                onClicked: kcm.gapOverridesModel.removeMonitor(index)
                            }
                        }

                        GridLayout {
                            columns: 2
                            columnSpacing: Kirigami.Units.largeSpacing
                            rowSpacing: Kirigami.Units.smallSpacing
                            Layout.fillWidth: true

                            QQC2.Label {
                                text: i18n("Layout:")
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            }
                            QQC2.ComboBox {
                                model: root.layoutOptions
                                textRole: "text"
                                valueRole: "value"
                                currentIndex: {
                                    const cur = entry ? entry.defaultLayout : kcm.settings.defaultLayout;
                                    const idx = root.layoutOptions.findIndex(item => item.value === cur);
                                    return idx >= 0 ? idx : 0;
                                }
                                onActivated: if (entry) entry.defaultLayout = currentValue
                            }

                            QQC2.Label {
                                text: i18n("Left gap:")
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            }
                            QQC2.SpinBox {
                                from: 0
                                to: 1000
                                value: entry ? entry.gapLeft : 0
                                onValueModified: if (entry) entry.gapLeft = value
                            }

                            QQC2.Label {
                                text: i18n("Right gap:")
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            }
                            QQC2.SpinBox {
                                from: 0
                                to: 1000
                                value: entry ? entry.gapRight : 0
                                onValueModified: if (entry) entry.gapRight = value
                            }

                            QQC2.Label {
                                text: i18n("Top gap:")
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            }
                            QQC2.SpinBox {
                                from: 0
                                to: 1000
                                value: entry ? entry.gapTop : 0
                                onValueModified: if (entry) entry.gapTop = value
                            }

                            QQC2.Label {
                                text: i18n("Bottom gap:")
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            }
                            QQC2.SpinBox {
                                from: 0
                                to: 1000
                                value: entry ? entry.gapBottom : 0
                                onValueModified: if (entry) entry.gapBottom = value
                            }

                            QQC2.Label {
                                text: i18n("Between tiles gap:")
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            }
                            QQC2.SpinBox {
                                from: 0
                                to: 1000
                                value: entry ? entry.gapBetween : 0
                                onValueModified: if (entry) entry.gapBetween = value
                            }
                        }
                    }
                }
            }

            QQC2.Button {
                text: i18n("Reset (use defaults on every monitor)")
                icon.name: "edit-undo"
                visible: kcm.gapOverridesModel.count > 0
                onClicked: kcm.gapOverridesModel.clearAll()
                Layout.alignment: Qt.AlignLeft
            }
        }

        // ====================================================================
        // Rules (float / ignore)
        // ====================================================================
        Kirigami.FormLayout {
            QQC2.Label {
                Kirigami.FormData.label: i18nc("@info", "Rules:")
                text: i18nc("@info", "Force matching windows to float (kept above the tiles; re-tile with Meta+W) or be ignored by tiling entirely. Match an application <b>class</b> to affect every window of an app, or a window <b>title</b> to target a single window such as a browser's Picture-in-Picture.")
                textFormat: Text.StyledText
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 30
                opacity: 0.7
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                QQC2.Button {
                    text: i18n("Add window…")
                    icon.name: "edit-find"
                    onClicked: kcm.pickWindow()
                }
                QQC2.Button {
                    text: i18n("Add rule")
                    icon.name: "list-add"
                    onClicked: kcm.rulesModel.addRule("class", "", "float")
                }
            }

            QQC2.Label {
                visible: kcm.rulesModel.count === 0
                text: i18nc("@info", "No rules yet. Click \"Add window…\", then click any open window to exclude it from tiling.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 30
                opacity: 0.7
            }

            Repeater {
                id: rulesRepeater
                model: kcm.rulesModel

                delegate: QQC2.Frame {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing / 2
                    Layout.bottomMargin: Kirigami.Units.smallSpacing / 2

                    property var entry: model.entry

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.ComboBox {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                            model: [
                                { text: i18n("Class"), value: "class" },
                                { text: i18n("Title"), value: "title" }
                            ]
                            textRole: "text"
                            valueRole: "value"
                            currentIndex: entry && entry.field === "title" ? 1 : 0
                            onActivated: if (entry) entry.field = currentValue
                        }

                        QQC2.TextField {
                            Layout.fillWidth: true
                            text: entry ? entry.pattern : ""
                            placeholderText: i18n("text to match (substring, case-insensitive)")
                            onTextEdited: if (entry) entry.pattern = text
                        }

                        QQC2.ComboBox {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                            model: [
                                { text: i18n("Float"), value: "float" },
                                { text: i18n("Ignore"), value: "ignore" }
                            ]
                            textRole: "text"
                            valueRole: "value"
                            currentIndex: entry && entry.action === "ignore" ? 1 : 0
                            onActivated: if (entry) entry.action = currentValue
                        }

                        QQC2.ToolButton {
                            icon.name: "list-remove"
                            display: QQC2.AbstractButton.IconOnly
                            text: i18n("Remove rule")
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.visible: hovered
                            onClicked: kcm.rulesModel.removeRule(index)
                        }
                    }
                }
            }

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18n("Automatic Floating")
            }

            QQC2.CheckBox {
                id: floatUtility
                Kirigami.FormData.label: i18n("Always float:")
                text: i18n("Utility windows")
                checked: kcm.settings.floatUtility
                onToggled: kcm.settings.floatUtility = checked
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "floatUtility"
                }
            }

            QQC2.CheckBox {
                id: floatDialog
                text: i18n("Dialog windows")
                checked: kcm.settings.floatDialog
                onToggled: kcm.settings.floatDialog = checked
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "floatDialog"
                }
            }

            QQC2.CheckBox {
                id: floatTransient
                text: i18n("Transient windows")
                checked: kcm.settings.floatTransient
                onToggled: kcm.settings.floatTransient = checked
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "floatTransient"
                }
            }
        }
    }

    // Shown after "Add window…" picks a window: choose whether to match its
    // application class (every window of that app) or just this window's title.
    QQC2.Menu {
        id: pickMenu
        property string pickClass: ""
        property string pickCaption: ""

        QQC2.MenuItem {
            text: i18n("Match this app — class: %1", pickMenu.pickClass)
            enabled: pickMenu.pickClass.length > 0
            onTriggered: kcm.rulesModel.addRule("class", pickMenu.pickClass, "float")
        }
        QQC2.MenuItem {
            text: i18n("Match only this window — title: %1", pickMenu.pickCaption)
            enabled: pickMenu.pickCaption.length > 0
            onTriggered: kcm.rulesModel.addRule("title", pickMenu.pickCaption, "float")
        }
    }

    Connections {
        target: kcm
        function onWindowPicked(resourceClass, caption) {
            pickMenu.pickClass = resourceClass;
            pickMenu.pickCaption = caption;
            pickMenu.popup();
        }
    }
}
