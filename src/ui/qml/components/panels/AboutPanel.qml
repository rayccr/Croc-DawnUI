import QtQuick
import QtQuick.Layouts

// 数据与关于：品牌主卡、数据管理及运行环境，避免把只读信息排成输入表单。
ColumnLayout {
    id: about
    property bool narrow: false
    signal openHistory()
    spacing: 16

    Panel {
        Layout.fillWidth: true
        padding: 24
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
