pragma Singleton

import QtQuick

// 设置项结构：只描述表单字段与默认值，不含任何传输数据。
// 设置本身尚未持久化，接入配置服务后由 C++ 提供同一结构。
// 历史说明保留；当前字段的保存已接入 ConfigStore，未实现项通过 available 禁用。
QtObject {
    readonly property var settingGroups: [
        [{key: "lang", label: "界面语言", options: ["中文", "English"], hint: "切换后界面立即使用所选语言；只影响界面显示，不改变传输内容。"},
         {key: "save", label: "默认保存位置", value: "", hint: "留空时使用系统下载目录下的 Croc-DawnUI。"},
         {key: "notify", available: false, label: "完成 / 失败通知", options: ["开启", "关闭"], hint: "系统通知尚未接入，此项暂时只保存不生效。"},
         {key: "close", available: false, label: "关闭窗口行为", options: ["最小化到托盘", "退出时确认"], hint: "托盘与关闭行为尚未接入，此项暂时只保存不生效。"}],
        [{key: "conflict", label: "同名文件处理", options: ["询问", "保留两份"],
          hint: "默认不覆盖：目标目录已有同名文件时需要你确认。"},
         {key: "sendLimit", label: "发送并发上限（1–8）", value: "2", hint: "DLL分别调度收发；等待对方不占实际传输名额。只影响新任务。"},
         {key: "receiveLimit", label: "接收并发上限（1–8）", value: "2", hint: "同一接收目录串行执行，避免重名竞争。只影响新任务。"},
         {key: "rate", label: "上传限速（MB/s，0 为不限速）", value: "0", hint: "只限制发送方向的上传速度，0 表示不限速。"},
         {key: "zip", label: "目录打包", options: ["默认不打包", "主动选择时打包"], hint: "选择“主动选择时打包”会给引擎加 --zip。"}],
        [{key: "policy", label: "连接策略", options: ["自动连接", "仅局域网"], hint: "仅局域网通过局域网发现和库内本地中继连接，自动分配端口，不转公网；双方均需选择此策略。防火墙应允许专用网络。"},
         {key: "lanAddress", label: "局域网发送端地址（可选）", value: "", hint: "默认自动发现。发现被阻止时，在接收方填发送电脑的私网IP:任务本地中继端口；端口见发送任务连接详情。只影响新接收任务。"},
         {key: "relay", label: "中继地址", value: "引擎默认", hint: "留空或保持默认时使用 Croc 自带中继；自建中继需填写地址与端口。"},
         {key: "password", label: "中继密码（仅当前会话）", value: "", hint: "不写入普通配置；重启后需重新输入。此密码不是文件加密口令。"},
         {key: "proxy", label: "代理", options: ["关闭"], available: false, hint: "强制代理的全部通路尚未验证，本版本暂不开放。"},
         {key: "proxyAddress", available: false, label: "代理地址", value: "127.0.0.1:1080"}],
        [{key: "phrase", label: "口令策略", options: ["随机生成", "单次任务自定义"]},
         {key: "mask", label: "显示策略", options: ["默认显示", "默认遮挡"], hint: "默认明文显示，可手动隐藏；选择默认遮挡时，临时显示 15 秒。"},
         {key: "clipboard", label: "剪贴板清理", options: ["不自动清理", "60 秒后有条件清理"]},
         // 历史配置 codeTimeout 仍可读取，活跃会话不再按此设置过期。
         {key: "reconnectWindow", label: "断线重连等待上限（秒）", value: "600", hint: "DLL默认保留任务10分钟，范围30–3600，0关闭自动重连；重新连接不代表原会话存活，重启需手动恢复。"},
         {key: "curve", label: "密钥交换曲线（高级）", options: ["p256", "p384", "p521", "siec", "ed25519"],
          hint: "加密握手使用的椭圆曲线，p256 是 Croc 默认值。不确定时保持默认；改这项前先确认对端版本也支持。"}],
        [{key: "retention", available: false, label: "历史保留", options: ["手动清理", "7 天", "30 天"], hint: "自动清理尚未实现，此项只保存偏好。"},
         {key: "export", label: "诊断导出", info: true, source: "diagnostics"},
         {key: "version", label: "应用版本", info: true, source: "appVersion"},
         {key: "engine", label: "Croc 版本", info: true, source: "engineVersion"}]
    ]
}
