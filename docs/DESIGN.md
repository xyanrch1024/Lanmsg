# QLanMsg 架构与设计文档

版本:0.1.0
日期:2026-08-09

---

## 1. 项目概述

QLanMsg 是一款基于 **Qt6 / C++17** 的局域网通讯工具,实现了三大核心功能:

| 功能 | 说明 |
|------|------|
| 聊天 | 局域网内点对点文本消息,支持多端在线、会话历史 |
| 文件传输 | 点对点分块传输,带进度、取消、断连处理 |
| 远程控制 | 屏幕实时画面 + 鼠标/键盘/滚轮输入注入 |

它采用 **无中心服务器** 的 P2P 模型:每台设备既是客户端也是服务端,通过 UDP 广播发现对等端,通过 TCP 直连建立会话。

> 本项目在 WSL2 (WSLg) 环境下开发与验证,亦支持 Windows(原生构建,含远程控制后端;另有 GitHub Actions 在 windows-latest 上自动编译验证)。

---

## 2. 技术栈与环境

- **语言/标准**:C++17
- **GUI 框架**:Qt 6 (Widgets + Network)
- **构建系统**:CMake (≥ 3.16),AUTOMOC/AUTORCC/AUTOUIC
- **平台后端**
  - Linux:libX11 + Xtst + Xext(XTest 输入注入、XShm 截屏)
  - Windows:user32 + gdi32(SetCursorPos/SendInput 输入注入、GDI BitBlt 截屏)
- **依赖组件**:`Qt6::Widgets`、`Qt6::Network`、`X11`、`Xtst`、`Xext`
- **无第三方开源依赖**

### 端口约定 (src/common/Protocol.h)

| 端口 | 用途 |
|------|------|
| UDP 24260 | 对等发现广播/应答 |
| TCP 24261 | 消息会话(可用环境变量 `QLANMSG_TCPPORT` 覆盖) |

---

## 3. 总体架构

```
┌───────────────────────────── 应用层 (src/ui) ─────────────────────────────┐
│  MainWindow         Peer 列表 / 聊天 / 传输记录 / 远程控制入口 / 设置        │
│   ├─ ChatWidget         聊天面板                                            │
│   ├─ TransferWidget     传输进度列表                                        │
│   ├─ RemoteDialog       远程控制"控制器端"窗口                               │
│   └─ SettingsDialog     昵称 / 远程密码                                     │
└──────────────┬──────────────────────────────┬─────────────────────────────┘
               │ 信号/槽 (Qt 事件驱动)          │
┌──────────────▼────────────────┐  ┌─────────▼──────────────────────────────┐
│ 业务服务层 (src/net, src/remote) │  │ 会话服务层                             │
│  FileSender / FileReceiver     │  │  NetworkService (路由中枢)             │
│  RemoteServer (+CaptureWorker) │──│  PeerSession (单连接封装)              │
└──────────────┬────────────────┘  └─────────────────┬──────────────────────┘
               │                                      │
┌──────────────▼──────────────────────────────────────▼────────────────────┐
│ 发现层 (src/discovery)                                                     │
│  PeerDiscovery:UDP 广播 + 单播 + 超时清理,输出 Peer 列表                     │
└──────────────┬──────────────────────────────────────┬────────────────────┘
               │ UDP 24260                             │ TCP 24261
┌──────────────▼─────────────────────┐  ┌─────────────▼─────────────────────┐
│ 局域网 (LAN)                        │  │ 对端设备(同一套程序)                │
└────────────────────────────────────┘  └───────────────────────────────────┘
```

### 分层职责

1. **发现层 `PeerDiscovery`**:只负责"找到谁在线"。UDP 广播 + 对已知对端单播,维护带过期时间的 Peer 表。
2. **会话层 `NetworkService` + `PeerSession`**:TCP 监听 + 主动连接,封装帧协议、握手、去重、消息路由。
3. **业务服务层**:`FileSender`/`FileReceiver` 实现文件协议状态机;`RemoteServer` 实现被控端逻辑。
4. **应用层 `MainWindow`**:组装所有模块,负责 UI 展示与用户交互,是信号总线的汇聚点。

所有模块运行在 GUI 主线程(事件循环驱动);仅远程控制的 **CaptureWorker** 运行在独立线程。

---

## 4. 目录结构与模块清单

```
qlanmsg/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                 入口:QApplication + 中文字体加载
│   ├── common/
│   │   ├── Protocol.h           帧协议、消息类型、常量、序列化(header-only)
│   │   ├── Config.h/.cpp        单例配置(QSettings + 环境变量覆盖)
│   ├── discovery/
│   │   ├── Peer.h               对端数据结构
│   │   └── PeerDiscovery.h/.cpp UDP 发现
│   ├── net/
│   │   ├── PeerSession.h/.cpp   TCP 连接封装、帧收发、发送队列
│   │   ├── NetworkService.h/.cpp 监听、会话注册/路由/去重
│   │   ├── FileSender.h/.cpp    发送方文件协议机
│   │   └── FileReceiver.h/.cpp  接收方文件协议机
│   ├── remote/
│   │   ├── IScreenSource.h      截屏抽象接口
│   │   ├── IInputSink.h         输入注入抽象接口
│   │   ├── ScreenSourceX11.h/.cpp  X11/XShm 截屏
│   │   ├── InputSinkX11.h/.cpp    XTest 输入注入
│   │   ├── ScreenSourceWindows.h/.cpp  GDI BitBlt 截屏(仅主屏)
│   │   └── InputSinkWindows.h/.cpp    SetCursorPos/SendInput 输入注入
│   │   └── RemoteServer.h/.cpp   被控端逻辑 + CaptureWorker
│   └── ui/
│       ├── MainWindow.h/.cpp    主窗口与信号总线
│       ├── ChatWidget.h/.cpp
│       ├── TransferWidget.h/.cpp
│       ├── RemoteDialog.h/.cpp  控制器端远程窗口
│       └── SettingsDialog.h/.cpp
```

---

## 5. 通信协议设计

### 5.1 发现协议 (UDP)

- 固定端口 **24260**,`QUdpSocket::bind(AnyIPv4, 24260, ShareAddress | ReuseAddressHint)` 允许多实例共存。
- 每 **3 秒**(`kDiscoveryIntervalMs`)广播一次 `255.255.255.255`;同时对已知对端**单播**一份(WSL2/NAT 隔离广播域时的补救)。
- 收到合法数据包后,记录对端并**立即回复**一份自己的信息,加快双向感知。
- 对端 **10 秒**(`kPeerTimeoutMs`)内无新包即判离线移除。

数据包为紧凑 JSON:

```json
{ "magic": "QLMSG", "ver": 1, "id": "<12hex>", "name": "昵称",
  "host": "主机名", "os": "系统", "app": "0.1.0", "port": 24261 }
```

- `id` 是本机持久化 appId(见 §9 配置),用于识别对端身份与过滤自己。
- `port` 使对端 TCP 端口可配置。

### 5.2 消息协议 (TCP)

每条消息为一个**帧**,二进制小端编码:

```
┌─────────────┬─────────────┬─────────────┬──────────────────┬────────────┐
│ totalLen:4  │ msgType:4   │ jsonLen:4   │ json(0~jsonLen)  │ body(可选) │
└─────────────┴─────────────┴─────────────┴──────────────────┴────────────┘
  header 共 12 字节 (kFrameHeaderSize)
```

- `totalLen`:整帧字节数(含 header),用于从 TCP 流中切帧。
- `msgType`:见 `MsgType` 枚举(1~16)。
- `json`:结构化字段,`QJsonDocument::Compact` 序列化。
- `body`:二进制负载(文件分块、JPEG 帧)。

切帧由 `consumeFrames()` 完成:循环判断 `totalLen`,数据不足则等待 `readyRead`;解析失败(`totalLen < 12`)则丢弃缓冲区并返回 false。

### 5.3 消息类型总表 (src/common/Protocol.h)

| 类型 | 值 | 方向 | JSON 字段 | body |
|------|----|------|-----------|------|
| Hello | 1 | 发起→接受 | id, name, seq, initIp, appId | - |
| Chat | 2 | 双向 | text, ts | - |
| FileOffer | 3 | 发送→接收 | token, name, size | - |
| FileAccept | 4 | 接收→发送 | token | - |
| FileDecline | 5 | 任意 | token, reason | - |
| FileChunk | 6 | 发送→接收 | token, seq | 文件数据 |
| FileDone | 7 | 发送→接收 | token | - |
| FileCancel | 8 | 双向 | token, reason | - |
| RcRequest | 9 | 控制→被控 | token, password | - |
| RcAccept | 10 | 被控→控制 | token | - |
| RcDecline | 11 | 被控→控制 | token, reason | - |
| RcFrame | 12 | 被控→控制 | token, w, h, ts | JPEG |
| RcInput | 13 | 控制→被控 | token + InputEvent 字段 | - |
| RcStop | 14 | 双向 | token | - |
| RcPing / RcPong | 15/16 | 双向 | token | - (预留) |

---

## 6. 会话管理与连接生命周期

### 6.1 会话对象 `PeerSession`

- 封装一条 **双向 TCP 连接** 和一个状态:
  - `Role`: `Outgoing`(本端发起) / `Incoming`(对端连入)。
  - `ready`:完成 Hello 握手后置位。
  - `m_queue`:未就绪前 `sendMessage()` 的消息按序缓存,就绪后 `flushQueue()` 一次写出(见下)。
- 关键设计:**先入队、后发送**。UI 在会话尚未连接完成时即可发消息,由会话层保证不丢。

### 6.2 握手与就绪

```
Outgoing 侧                                  Incoming 侧
────────                                   ─────────
connectToHost ──────────────── TCP ────────► accept (注册会话)
onConnected:
  发 Hello(id,name,seq,initIp,appId)
  setReady(true)                            onReadyRead:
  (flush 发送队列)                           收到 Hello:
                                             记录 peerName/seq/initIp/appId
                                             setReady(true) → flush 队列
                                             emit helloReceived
```

- **Outgoing 侧就绪即自报**,不依赖对端回 Hello,因此 `NetworkService` 在 `readyChanged` 中为 Outgoing 会话直接派发 `sessionReady`。
- **Incoming 侧就绪**由收到 Hello 触发。

### 6.3 双连接去重 (dedupe)

两台设备可能**同时**互相发起 TCP 连接,形成两条对向连接。处理:

1. 两条连接各自发送 Hello,携带各自的 `(initIp, seq, appId)`。
2. `dedupe()` 比较握手键 `initIp|seq|appId`(字典序),**保留键较小的一条**,`abort()` 并销毁另一条。
3. 由于比较的是发起方属性而非对端 IP,两端对"留哪条"的判断**一致**,不会各留一条。
4. 始终用幸存会话覆盖会话表,避免表项被覆盖成被淘汰者。

### 6.4 发送辅助:sessionFor vs sessionAny

- `sessionFor(ip)`:要求会话存在且 socket 处于 ConnectedState —— 用于**必须立即传输**的消息(文件、远程帧等)。
- `sessionAny(ip)`:只要会话对象存在(连接中/已连接皆可)—— 用于**可以排队**的消息(聊天),配合 PeerSession 队列实现"连接好再发"。

### 6.5 断连与重连

- `PeerSession::onDisconnected` → `NetworkService::onSessionClosed`:注销、发 `sessionClosed`,延迟销毁。
- 会话**不自动重连**;采用**惰性重连**:每次发送操作前 `MainWindow` 调用 `ensureSession(peer)`,若会话已不存在则新建并 `connectToHost`。

### 6.6 关键值

- 发送队列:无上限(内存内 QList)。
- 文件发送背压阈值:512 KB(`FileSender::pump` 中 `bytesToWrite()` 判断)。
- 远程帧丢弃阈值:768 KB(`kRemoteFrameMaxBuffered`)。

---

## 7. 功能模块设计

### 7.1 聊天

- 发送:`onChatSend` → 本地回显 + `sendChat`(可排队)。
- 接收:`chatReceived` → 追加 `m_history[ip]`,若当前正在与该端聊天则即时显示,否则标题栏提示新消息。
- 历史按 IP 缓存在内存,切换对端时从 `m_history` 重放。

### 7.2 文件传输

状态机(发送方 `FileSender`):

```
start()
  └─ 等 sessionReady 或会话已就绪
      └─ 打开文件 → 发 FileOffer
          ├─ 收 FileDecline → finish(false)
          ├─ 收 FileAccept → 进入 pump 循环
          │     └─ 背压循环:bytesToWrite() < 512KB 时
          │          读 64KB 分块 → sendFileChunk(seq++)
          │          文件读完 → sendFileDone → finish(true)
          └─ 任意时刻收 FileCancel / 析构(仍 active) → 发 FileCancel → finish(false)
```

接收方 `FileReceiver`:

```
构造: 打开目标文件(失败即 finish)
onChunk: 按 seq*64KB 定位写盘,累计字节 ≥ 总大小 → sendFileDone → finish(true)
onDone:  对端确认完成 → finish(true)
onCancel/取消: 删除已写文件 → finish(false)
```

要点:
- **序号定位写**:按 `seq` 计算偏移,支持乱序不丢(协议层严格保序,这里仅防御)。
- **背压调速**:不盲目快发,以 socket 发送缓冲剩余量驱动,避免大文件占满内核缓冲导致其他消息(聊天/远程)饿死。
- **令牌(token)路由**:每个传输用 UUID 关联,收发两端用 `ip|token` 双向映射,多文件可并发。

### 7.3 远程控制

```
控制端 RemoteDialog                          被控端 RemoteServer
───────────────                              ─────────────────
RcRequest(token,password)
                                             ─► onRcRequest → requestIncoming
                                                MainWindow 弹窗/输密码
                                                respond(accept) → begin()
RcAccept ◄───────────────────────────────────  RcAccept
                                                CaptureWorker(独立线程):
                                                   XShm 截屏 → JPEG 编码
RcFrame(JPEG) ◄─────────────────────────────   10fps / quality 65
  onFrame → 显示缩放画面
鼠标/键盘事件 → RcInput ────────────────────►  onRcInput → InputSinkX11(XTest)
RcStop / 关闭窗口 ──────────────────────────►  onRcStop → end() 停止采集
```

关键设计:

- **截屏与编码在独立线程** `CaptureWorker`(QThread):阻塞式 X 调用与 JPEG 编码不卡 UI。
- **XShm 优先,XGetImage 兜底**:共享内存截屏;分辨率变化时自动重建。
- **帧丢弃的背压控制**:`sendRemoteFrame` 检查 `bytesToWrite() > 768KB` 则丢帧,防止画面延迟累积。
- **空屏/黑屏检测**:`CaptureWorker` 连续约 1 秒(10 帧)采集到纯黑或空帧即判定"桌面不可采集",停止推流并把原因通过 `RcDecline` 回传控制端,避免控制端看到一团黑却不知缘由(WSLg 下 Xwayland 根窗口为纯黑,见 §13)。
- **输入注入按需创建** `InputSinkX11`(XTest),首次收到输入事件时才初始化。
- **密码策略**:`Config::remotePassword()` 为空 → 每次都弹确认框;非空 → 校验密码。由 `MainWindow::onRcRequest` 决策,`RemoteServer::respond()` 只执行。
- **并发保护**:已被控制时拒绝新请求;会话断开自动结束。

### 7.4 线程模型

```
GUI 主线程(QThread: main):
  PeerDiscovery · NetworkService · PeerSession · FileSender/Receiver
  RemoteServer(非阻塞部分) · MainWindow/全部 UI

远程控制专用线程:
  CaptureWorker → ScreenSourceX11(阻塞 X11) → JPEG 编码
  结果经 QueuedConnection 回到主线程 → sendRemoteFrame(主线程写 socket)
```

跨线程仅一条 `CaptureWorker.frameReady → RemoteServer.onFrameReady`,使用 `Qt::QueuedConnection` 保证线程安全。

---

## 8. 平台抽象 (remote 层)

- `IScreenSource`:截屏接口 `initialize/shutdown/grab/width/height`。
- `IInputSink`:输入注入接口 `mouseMove/mouseButton/wheel/key`。
- Linux 实现:
  - `ScreenSourceX11`:`XShmCreateImage` 共享内存,字节序假定 ARGB32 ⇔ X11 ZPixmap 32bpp(小端)。
  - `InputSinkX11`:`XTestFakeMotionEvent / XTestFakeButtonEvent / XTestFakeKeyEvent`,含 `Qt::Key → keysym` 映射(特殊键表 + ASCII 直通)。
- Windows 实现:
  - `ScreenSourceWindows`:`CreateDIBSection`(32bpp、负高度自顶向下)+ `BitBlt` 抓取主屏 DC(仅主屏),返回 `Format_ARGB32`;物理像素,与输入注入坐标一致。
  - `InputSinkWindows`:鼠标移动用 `SetCursorPos`(物理像素绝对定位),按键/滚轮/键盘用 `SendInput`(`MOUSEEVENTF_*` / `KEYEVENTF_*`);`Qt::Key → VK` 映射(特殊键表 + 字母数字直通 + `VkKeyScanW` 解析其余可打印字符)。
  - 局限:安全桌面(锁屏/UAC 弹窗)下 `SendInput` 与 `BitBlt` 均失效;截屏仅覆盖主屏。

---

## 9. 配置管理 (Config)

- **持久化**:`QSettings("QLanMsg","QLanMsg")`,字段:`appId`、`nickname`、`remotePassword`。
- **appId**:首次运行生成 12 位 hex 持久化;可用环境变量 `QLANMSG_APPID` 覆盖(用于多实例测试)。
- **环境变量覆盖**:
  - `QLANMSG_TCPPORT`:覆盖 TCP 监听端口。
  - `QLANMSG_APPID`:覆盖设备身份。
- **下载目录**:`QStandardPaths::DownloadLocation/qlanmsg`,自动创建。

---

## 10. 中文字体加载 (main.cpp)

WSL/WSLg 的 Linux 侧默认无 CJK 字体,界面中文会显示为方框。启动时探测并加载 Windows 挂载字体:

```
/mnt/c/Windows/Fonts/msyh.ttc (微软雅黑) → msyhbd → simhei → simsun
```

`QFontDatabase::addApplicationFont()` 成功后,取实际族名(如 "Microsoft YaHei")设为 `QGuiApplication` 全局字体。文件缺失的环境(如 Windows 原生)自动跳过。

---

## 11. 测试与验证

### 11.1 无头回归钩子(环境变量)

所有钩子仅在设置了对应环境变量时激活,不影响正常使用:

| 变量 | 行为 |
|------|------|
| `QLANMSG_APPID` | 覆盖设备 ID(区分两实例) |
| `QLANMSG_TCPPORT` | 覆盖 TCP 端口(本机两实例测试) |
| `QLANMSG_LOG` | 输出 `[test]/[net]/[chat]/[file]/[rc]/[discovery]` 日志 |
| `QLANMSG_AUTO_ACCEPT` | 自动接受文件接收与远程控制请求(免模态框) |
| `QLANMSG_TEST_CHAT` | 启动后向指定 IP 发测试聊天 |
| `QLANMSG_TEST_FILE` | 启动后向指定 IP 发送指定文件 |
| `QLANMSG_TEST_RC` | 启动后向指定 IP 发起远程控制请求 |
| `QLANMSG_TEST_RCINPUT` | 远程接受后注入一次鼠标移动 |

### 11.2 端到端验证结果(双进程 + offscreen 平台)

- **发现**:A、B 互相在约 3 秒内发现对方。
- **聊天**:A→B、B→A 双向送达(消息在 TCP 会话建立前入队,连接后自动发出)。
- **文件传输**:2 MB 随机文件,传输后 MD5 一致;断连/取消路径正常。
- **远程控制**:握手→接受→55 帧 2560×1440 JPEG @10fps 真实屏幕捕获;输入注入事件(XTest 鼠标移动)到达被控端。

### 11.3 构建

```bash
cmake -B build && cmake --build build -j$(nproc)
./build/qlanmsg
# 本机多实例联调:
QLANMSG_APPID=AAAA ./build/qlanmsg &
QLANMSG_APPID=BBBB QLANMSG_TCPPORT=24262 ./build/qlanmsg &
```

---

## 12. 安全设计

- 远程控制支持**口令校验**(`remotePassword`),未设置时强制人工确认。
- 无中心服务器,信息仅在局域网内两台设备间传递。
- **未加密传输**:当前版本无 TLS,口令/文件/屏幕数据明文走局域网。属已知限制(见 §13)。

---

## 13. 已知限制与未来方向

1. **无传输加密**:可加 TLS(WSS/OpenSSL)或简化对称加密(AES-GCM + ECDH 协商)。
2. **会话复用单一 TCP 连接**:聊天/文件/远程共用一条流,文件发送虽已背压,但无优先级;未来可拆流或引入消息优先级队列。
3. **远程控制仅支持 X11/XWayland 与 Windows**:Wayland 原生(pipewire/screencast)未实现;Windows 截屏仅主屏,锁屏/UAC 安全桌面下截屏与输入注入失效。
   - **WSLg 限制**:WSLg 的桌面由 Windows 宿主机合成(RDP),Linux 侧 Xwayland 根窗口为纯黑且 `XGetImage` 报 `BadMatch`,**无法从 Linux 侧采集真实桌面**。程序检测到黑屏后会自动停止推流,并把原因("桌面画面为空白:WSLg/无 X11 桌面环境下无法采集真实屏幕")回传给控制端显示,而不是静默黑屏。远程控制仅在真实 X11 桌面会话或原生 Windows 下可用。
4. **无 NAT 穿透**:仅限同一可达局域网;WSL2 已用单播弥补广播隔离。
5. **文件无断点续传/校验和**:完成后未做端到端哈希校验(可加 SHA-256 摘要)。
6. **聊天无持久化**:历史仅存内存,退出即失(可接 SQLite)。
7. **RcPing/RcPong 预留但未启用**:可做被控端在线心跳/画面冻结检测。
8. **单个对端单会话**:`m_sessions` 以 IP 为键,不支持同一 IP 多身份。

---

## 14. 核心类关系一览

```
PeerDiscovery ──► Peer { id,name,host,os,ip,tcpPort,lastSeen }
NetworkService ──► PeerSession { socket, role, ready, queue }
      │  ├─ FileSender / FileReceiver (按 token 路由)
      │  ├─ RemoteServer
      │  │     ├─ CaptureWorker(QThread) ── IScreenSource (ScreenSourceX11)
      │  │     └─ IInputSink (InputSinkX11)
      │  └─ MainWindow ── RemoteDialog
      └─ Config (单例)
```
