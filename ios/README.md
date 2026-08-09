# QLanMsg iOS 客户端

iOS 客户端,与桌面端(`src/`)通过同一套二进制协议互通:
聊天、文件传输、远程控制(iPhone → 桌面)。

## 目录结构

```
ios/
  project.yml                     # XcodeGen 工程定义
  QLanMsg/
    Info.plist                    # 本地网络权限、文件共享等
    Sources/
      Core/                       # 纯逻辑层,无 UIKit (framework target)
        Protocol.swift            # 协议常量 + MsgType
        Framing.swift             # 12 字节小端帧头 构建/解析
        Peer.swift                # 发现的设备
        Discovery.swift           # UDP 广播/单播发现 (POSIX socket)
        Transport.swift           # NWConnection 会话 + 去重 + 发送
        InputEvent.swift          # 输入事件 + Qt::Key 常量
        KeyMapping.swift          # HID 用法码 -> Qt::Key
        SettingsStore.swift       # UserDefaults 持久化
      App/                        # SwiftUI UI 层
        QLanMsgApp.swift          # @main
        AppModel.swift            # 中央协调:发现/传输/帧路由
        RootView.swift            # 三标签页 + 全局弹窗/全屏
        PeerListView.swift        # 设备列表 (右键/滑动菜单)
        ChatView.swift            # 聊天
        TransferListView.swift    # 传输记录 (含接收文件分享)
        SettingsView.swift        # 昵称/密码/本机信息
        RemoteController.swift    # 远程控制会话 (控制端)
        RemoteControlView.swift   # 远程画面 + 触摸/键盘注入
        FileTransfer.swift        # FileSender / FileReceiver
    Tests/
      ProtocolTests.swift         # 分帧/粘包/输入事件/键映射 单测
  tools/
    protocol_dump.py              # 原始字节流帧解码工具
```

## 构建(在 Mac 上)

```bash
brew install xcodegen
cd ios
xcodegen generate
open QLanMsg.xcodeproj
```

- 在 Signing 中选择你的 Apple ID(免费个人签名即可),连接 iPhone 直接 Run。
- 首次启动会弹出「本地网络」权限,需允许。
- 应用需保持在前台:iOS 后台会挂起进程,发现与传输会暂停。

## 与桌面端联调

1. 桌面端与本机连同一 Wi-Fi,分别启动。
2. 桌面端出现在 iOS「设备」列表,同时 iPhone 也会出现在桌面端设备列表。
3. 聊天、文件传输双向互通。
4. 远程控制:**iPhone → 桌面**。在设备上选择「远程控制」,桌面端收到请求后
   按提示输入/确认密码;画面以 JPEG 流回显。
   - 单击/拖动 = 左键;长按拖动 = 右键;双指滑动 = 滚轮;双指轻点 = 中键。
   - iPad 外接硬件键盘时,键盘输入会转发到桌面(支持 Cmd/Ctrl/Alt 组合键)。
   - 桌面 → iPhone 的远程控制请求会被自动拒绝(iOS 不支持被注入输入)。

## 测试

```bash
cd ios && xcodegen generate
xcodebuild test -project QLanMsg.xcodeproj -scheme QLanMsg \
  -destination 'platform=iOS Simulator,name=iPhone 15'
```

## 协议速查(与 `src/common/Protocol.h` 一致)

- 发现:UDP 24260,`QLMSG` magic JSON;每 3s 广播 `255.255.255.255` + 单播,收到即回。
- TCP 24261,12 字节小端头:`total(4) | msgType(4) | jsonLen(4)`,随后 JSON + body。
- 发起方连接后发 Hello;两端会话就绪后即可收发帧。
- 文件按 64KB 分块:`FileOffer/FileAccept/FileChunk(seq)/FileDone/FileCancel`,
  接收端以 `seq * 64KB` 偏移写入。
- 远程控制:`RcRequest(token,password) → RcAccept/RcDecline`,之后 `RcFrame(jpeg,w,h,ts)`
  与 `RcInput(t,x,y,b,dx,dy,k)`;`RcStop` 结束。
