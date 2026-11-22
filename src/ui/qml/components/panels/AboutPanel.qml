import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls

// 数据与关于：品牌主卡、数据管理及运行环境，避免把只读信息排成输入表单。
ColumnLayout {
    id: about
    property bool narrow: false
    signal openHistory()
    spacing: 16

    component RepositoryLink: Controls.AbstractButton {
        id: link

        required property string repositoryUrl
        required property string symbol
        required property string tone
        required property color accent
        required property color surface

        implicitHeight: Math.max(64, contentItem.implicitHeight + topPadding + bottomPadding)
        leftPadding: 12
        rightPadding: 12
        topPadding: 10
        bottomPadding: 10
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.Link
        Accessible.name: link.text
        Accessible.description: link.repositoryUrl
        onClicked: Qt.openUrlExternally(link.repositoryUrl)

        HoverHandler { cursorShape: Qt.PointingHandCursor }

        background: Rectangle {
            radius: Theme.radiusControl
            color: link.down ? Theme.pressed : (link.hovered || link.visualFocus ? link.surface : Theme.field)
            border.color: link.hovered || link.visualFocus ? link.accent : Theme.line
        }

        contentItem: RowLayout {
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: 8
                color: link.surface

                UiIcon {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    symbol: link.symbol
                    tone: link.tone
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                BodyText {
                    Layout.fillWidth: true
                    text: link.text
                    font.weight: Font.DemiBold
                }

                Hint {
                    Layout.fillWidth: true
                    text: link.repositoryUrl.substring("https://".length)
                    color: link.hovered || link.visualFocus ? link.accent : Theme.muted
                    wrapMode: Text.WrapAnywhere
                }
            }

            UiIcon {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                symbol: "arrow"
                tone: link.hovered || link.visualFocus ? link.tone : "muted"
                rotation: -45
            }
        }
    }

    Panel {
        Layout.fillWidth: true
        padding: about.narrow ? 18 : 24
        RowLayout {
            Layout.fillWidth: true
            spacing: 18
            Image {
                source: "qrc:/ui/icons/dawn-logo.svg"
                Layout.preferredWidth: 64; Layout.preferredHeight: 64
                sourceSize.width: 128; sourceSize.height: 128
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                BodyText { text: "Croc-DawnUI"; font.pixelSize: about.narrow ? 22 : 26; font.weight: Font.DemiBold; Layout.fillWidth: true }
                Hint { text: I18n.t("破晓 · 双方在线，即时互传"); Layout.fillWidth: true }
                Hint { text: AppController.appVersion.replace("Croc-DawnUI ", ""); color: Theme.blue; font.pixelSize: 12; Layout.fillWidth: true }
            }
        }
        Rule { }
        Hint { text: I18n.t("基于 Croc 的文件与文件夹互传。每次连接、接收确认与传输结果，都对应明确的任务。"); Layout.fillWidth: true }

        Rule { }

        BodyText {
            text: I18n.t("项目源码")
            font.weight: Font.DemiBold
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width >= 600 ? 2 : 1
            columnSpacing: 10
            rowSpacing: 8

            RepositoryLink {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                text: "Croc-DawnUI"
                repositoryUrl: "https://github.com/rayccr/Croc-DawnUI"
                symbol: "folder"
                tone: "send"
                accent: Theme.blue
                surface: Theme.sendSurface
            }

            RepositoryLink {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                text: "Croc-lib"
                repositoryUrl: "https://github.com/rayccr/Croc-lib"
                symbol: "box"
                tone: "receive"
                accent: Theme.green
                surface: Theme.receiveSurface
            }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: about.narrow ? 1 : 2
        columnSpacing: 16; rowSpacing: 16
        Panel {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            padding: 20
            BodyText { text: I18n.t("本地数据"); font.pixelSize: 17; font.weight: Font.DemiBold }
            Hint { text: I18n.t("历史保存在本机，包含文件位置和传输结果。清理记录不会删除传输文件。"); Layout.fillWidth: true }
            AppButton { text: I18n.t("查看传输历史"); symbol: "clock"; onClicked: about.openHistory() }
            Rule { }
            Hint { text: I18n.t("保留方式"); font.pixelSize: 11 }
            BodyText { text: I18n.t("手动清理"); font.pixelSize: 14 }
            Hint { text: I18n.t("自动清理尚未开放。"); font.pixelSize: 12; Layout.fillWidth: true }
        }
        Panel {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            padding: 20
            BodyText { text: I18n.t("运行环境"); font.pixelSize: 17; font.weight: Font.DemiBold }
            Hint { text: I18n.t("传输引擎"); font.pixelSize: 11 }
            BodyText { text: AppController.engineVersion; font.pixelSize: 14; Layout.fillWidth: true }
            Hint { text: I18n.t("连接结果以具体任务为准。"); font.pixelSize: 12; Layout.fillWidth: true }
            Rule { }
            Hint { text: I18n.t("问题诊断"); font.pixelSize: 11 }
            BodyText { text: I18n.t("诊断导出 · 计划中"); font.pixelSize: 14; Layout.fillWidth: true }
            Hint { text: I18n.t("开放后将默认脱敏，不导出口令、密钥或中继密码。"); font.pixelSize: 12; Layout.fillWidth: true }
        }
    }
}
