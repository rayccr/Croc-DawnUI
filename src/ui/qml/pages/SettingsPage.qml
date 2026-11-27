import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls

// 设置页：分组表单、进程内草稿与保存预览。
// 草稿只存在内存里，保存不会写磁盘，也不会影响任何引擎参数。
// 上述为早期预览阶段说明；当前草稿仍在内存，保存已由 ConfigStore 校验并原子写盘。
ColumnLayout {
    id: settingsPage
    objectName: "settingsPage"
    property bool narrow: false
    property int tab: 0
    property var draft: ({})

    signal notifyRequested(string text)
    signal navigateRequested(string page)

    // 页面显示时从配置读取，保存后重新读取，避免草稿与磁盘不一致
    function reload() {
        settingsPage.draft = JSON.parse(JSON.stringify(AppController.settings));
    }

    Component.onCompleted: settingsPage.reload()

    // 只读信息项：版本与诊断导出等不渲染成输入框
    function infoText(strSource) {
        if (strSource === "appVersion") {
            return AppController.appVersion;
        }
        if (strSource === "engineVersion") {
            return AppController.engineVersion;
        }
        return I18n.t(I18n.t("尚未接入（后续提供脱敏导出）"));
    }

    Controls.Popup {
        id: resetPopup; anchors.centerIn: parent; width: Math.min(400, settingsPage.width); modal: true; padding: 20
        background: GlassSurface { glassTint: Theme.panel; tintStrength: 0.78; glassOpacity: 0.98; radius: 12 }
        contentItem: ColumnLayout {
            spacing: 12
            BodyText { text: I18n.t("恢复默认设置？") }
            Hint { text: I18n.t("仅重置偏好，不删除历史或用户文件。"); Layout.fillWidth: true }
            RowLayout {
                AppButton { text: I18n.t("取消"); onClicked: resetPopup.close() }
                AppButton { text: I18n.t("确认重置"); onClicked: { AppController.resetSettings(); settingsPage.reload(); resetPopup.close(); } }
            }
        }
    }
    spacing: Theme.compactDesktop ? 10 : 20

    GridLayout {
        Layout.fillWidth: true
        columns: settingsPage.narrow ? 2 : 5
        columnSpacing: 6
        rowSpacing: 6

        Repeater {
            model: [I18n.t("常规"), I18n.t("传输"), I18n.t("连接"), I18n.t("安全"), I18n.t("数据与关于")]
            AppButton {
                required property int index
                required property string modelData
                text: I18n.t(modelData)
                Layout.fillWidth: true
                tint: settingsPage.tab === index ? Theme.hover : "transparent"
                onClicked: settingsPage.tab = index
            }
        }
    }

    Panel {
        visible: settingsPage.tab !== 4
        Layout.fillWidth: true

        Repeater {
            model: SettingsSchema.settingGroups[settingsPage.tab]

            ColumnLayout {
                required property var modelData
                visible: Qt.platform.os !== "android" || modelData.key !== "save"
                Layout.fillWidth: true
                spacing: Theme.compactDesktop ? 5 : 8

                BodyText {
                    text: I18n.t(modelData.label)
                    font.pixelSize: 13
                    Layout.fillWidth: true
                }

                Loader {
                    Layout.fillWidth: true
                    visible: modelData.info !== true
                    enabled: field.available !== false
                    opacity: enabled ? 1 : 0.55
                    sourceComponent: modelData.info === true ? null : (modelData.options ? choiceControl : textControl)
                    property var field: modelData

                    Component {
                        id: choiceControl
                        SelectField {
                            options: field.options
                            currentIndex: Math.max(0, field.options.indexOf(settingsPage.draft[field.key] || field.options[0]))
                            onActivated: settingsPage.draft[field.key] = currentText
                        }
                    }

                    Component {
                        id: textControl
                        FieldBox {
                            text: settingsPage.draft[field.key] !== undefined ? settingsPage.draft[field.key] : (field.value || "")
                            echoMode: field.key === "password" ? TextInput.Password : TextInput.Normal
                            onEditingFinished: settingsPage.draft[field.key] = text
                        }
                    }
                }

                BodyText {
                    visible: modelData.info === true
                    text: I18n.t(settingsPage.infoText(modelData.source))
                    Layout.fillWidth: true
                }

                Hint {
                    visible: modelData.hint !== undefined
                    text: modelData.hint !== undefined ? I18n.t(modelData.hint) : ""
                    Layout.fillWidth: true
                }
            }
        }

        Hint {
            text: settingsPage.tab === 0 ? I18n.t(Qt.platform.os === "android" ? "文件先保存到应用内，完成后可从传输历史导出。" : "保存位置会作为接收页的默认目录；通知与托盘行为尚未接入系统，暂不生效。")
                  : settingsPage.tab === 1 ? I18n.t("限速与目录打包会传给引擎；并发上限的调度尚未实现。")
                  : settingsPage.tab === 2 ? I18n.t("中继与仅局域网参数传给引擎；强制代理与指定路径尚未验证，暂不开放。")
                  : settingsPage.tab === 3 ? I18n.t("曲线传给引擎；自定义口令在发送任务中填写；传输加密由引擎处理，不提供关闭加密的开关。")
                  : I18n.t("设置保存在本地，只影响之后新建的任务。")
            Layout.fillWidth: true
        }
    }

    AboutPanel {
        visible: settingsPage.tab === 4
        narrow: settingsPage.narrow
        Layout.fillWidth: true
        onOpenHistory: settingsPage.navigateRequested("history")
    }

    RowLayout {
        visible: settingsPage.tab !== 4
        Layout.fillWidth: true
        AppButton {
            text: I18n.t("保存设置")
            tint: Theme.blue
            labelColor: Theme.onAccent
            onClicked: {
                if (AppController.saveSettings(settingsPage.draft)) { settingsPage.reload(); }
            }
        }
        Item { Layout.fillWidth: true }
        AppButton {
            text: I18n.t("恢复出厂设置")
            onClicked: {
                resetPopup.open();
            }
        }
    }
}
