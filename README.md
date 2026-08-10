# QLanMsg

局域网即时通讯与远程控制工具(纯 P2P,无中心服务器)。

- **聊天**:局域网内点对点文本消息,多端在线,历史记录
- **文件传输**:点对点分块传输,带进度、速度、剩余时间、拖拽发送
- **远程控制**:屏幕实时画面 + 鼠标/键盘/滚轮输入注入
- 跨平台:**Windows / Linux(桌面端)** + **iOS(客户端)**

## 目录结构

```
├── src/                      # 桌面端 (Qt6 / C++17)
│   ├── main.cpp
│   ├── common/               # 协议常量、配置
│   ├── discovery/            # UDP 对等发现
│   ├── net/                  # TCP 会话、文件传输
│   ├── remote/               # 远程控制(截屏 + 输入注入)
│   └── ui/                   # 主窗口、聊天、传输、远程控制、设置
├── ios/                      # iOS 客户端 (SwiftUI,与桌面端同一协议)
├── resources/                # 图标 / 提示音 (msgnotify.wav)
├── docs/DESIGN.md            # 架构与协议设计文档
└── .github/workflows/        # Windows 构建 + iOS 构建/测试 CI
```

详细架构见 [`docs/DESIGN.md`](docs/DESIGN.md),iOS 客户端说明见 [`ios/README.md`](ios/README.md)。

## 快速开始

### 桌面端(Windows / Linux)

依赖:CMake ≥ 3.16、Qt 6(Widgets、Network;可选 Multimedia 获得真实提示音)。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/qlanmsg
```

- 局域网内多台设备启动后自动互相发现,出现在左侧设备列表。
- 点开设备即可聊天;输入框回车发送、Shift+回车换行;点「发送」按钮同样发送。
- 把文件拖到窗口(或右键设备 → 发送文件)即可传输;聊天区底部显示进度与速度。
- 右键设备 → 远程控制;被控端确认/输入密码后显示实时画面。

### iOS 客户端

见 [`ios/README.md`](ios/README.md)(需要 Mac + Xcode/XcodeGen,免费个人签名即可)。

## 托盘与通知

- 点击窗口关闭按钮(X)会**最小化到系统托盘**,托盘菜单可恢复/退出。
- 收到新消息时:窗口最小化或隐藏在托盘 → 弹出托盘气泡提醒 + 提示音;窗口在前台时直接显示在聊天区。
- 提示音使用 `resources/msgnotify.wav`;未编译 Qt Multimedia 时自动降级为系统提示音。

## 局域网发现机制

- 无周期广播:应用**启动时广播一次** `hello`(UDP 24260),本机 IP/网络变化时重新广播。
- 只对**首次出现的新设备**回包,避免回显风暴;退出时广播 `bye`,对端立即移除。
- 两台真实主机(同一 Wi-Fi / 有线局域网)谁先谁后都能互相发现。WSL2 环境下 Windows→WSL 的 UDP 广播受 NAT 限制,可能无法互相发现,属环境限制。

## 端口与协议

| 端口 | 用途 |
|------|------|
| UDP 24260 | 对等发现(`QLMSG` magic JSON) |
| TCP 24261 | 消息会话(12 字节小端帧头 + JSON + body) |

协议细节见 [`docs/DESIGN.md`](docs/DESIGN.md)。

## 环境变量

| 变量 | 说明 |
|------|------|
| `QLANMSG_APPID` | 覆盖持久化设备 ID |
| `QLANMSG_TCPPORT` | 覆盖 TCP 端口(默认 24261) |
| `QLANMSG_LOG` | 打印发现/聊天等调试日志 |
| `QLANMSG_AUTO_ACCEPT` | 自动接受文件接收与远程控制请求(免模态框) |
| `QLANMSG_TEST_CHAT` / `QLANMSG_TEST_FILE` / `QLANMSG_TEST_RC` | 启动后自动向指定 IP 发测试流量 |

## CI

- `.github/workflows/windows.yml`:windows-latest 上编译 + 打包 Qt 运行时,产出可执行 exe。
- `.github/workflows/ios.yml`:macos-15 上用 XcodeGen 生成工程、模拟器编译 + 单元测试,失败时把编译错误回显到 PR/commit 注释。
