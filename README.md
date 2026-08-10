# QLanMsg

**English** | [**中文**](README.zh-CN.md)

A LAN instant-messaging and remote-control tool (pure P2P, no central server).

- **Chat**: peer-to-peer text messages on the LAN, multiple devices online, message history
- **File transfer**: chunked peer-to-peer transfer with progress, speed, ETA, drag & drop
- **Remote control**: live screen view + mouse / keyboard / wheel input injection
- Cross-platform: **Windows / Linux (desktop)** + **iOS (client)**

## Layout

```
├── src/                      # Desktop app (Qt6 / C++17)
│   ├── main.cpp
│   ├── common/               # Protocol constants, config
│   ├── discovery/            # UDP peer discovery
│   ├── net/                  # TCP sessions, file transfer
│   ├── remote/               # Remote control (screen capture + input injection)
│   └── ui/                   # Main window, chat, transfer, remote control, settings
├── ios/                      # iOS client (SwiftUI, same wire protocol as desktop)
├── resources/                # Icons / notification sound (msgnotify.wav)
├── docs/DESIGN.md            # Architecture & protocol design (Chinese)
└── .github/workflows/        # Windows build + iOS build/test CI
```

Detailed architecture is in [`docs/DESIGN.md`](docs/DESIGN.md); the iOS client is documented in [`ios/README.md`](ios/README.md).

## Quick start

### Desktop (Windows / Linux)

Requirements: CMake ≥ 3.16, Qt 6 (Widgets, Network; Multimedia is optional for a real notification sound).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/qlanmsg
```

- Devices on the same LAN discover each other automatically and appear in the left list.
- Click a device to chat. Enter sends, Shift+Enter inserts a newline; the **发送** (Send) button works too.
- Drag a file onto the window (or right-click a device → Send file) to transfer; progress and speed show at the bottom of the chat area.
- Right-click a device → Remote control; the controlled side confirms/enters a password and the live screen is shown.

### iOS client

See [`ios/README.md`](ios/README.md) (requires a Mac with Xcode/XcodeGen; free personal signing is enough).

## Tray & notifications

- Clicking the window's close button (X) **minimizes to the system tray**; the tray menu can restore or quit.
- On a new message: if the window is minimized or hidden in the tray, a **tray balloon + notification sound** pops up; when the window is in the foreground, the message just appears in the chat area.
- The sound uses `resources/msgnotify.wav`; it falls back to a system beep when Qt Multimedia is not compiled in.

## LAN discovery

- No periodic broadcast: the app **broadcasts a `hello` once at startup** (UDP 24260) and re-broadcasts when the local IP / network changes.
- It only replies to **brand-new peers** (avoids an echo storm); on exit it broadcasts a `bye` so peers drop it immediately.
- Two real hosts on the same LAN (Wi-Fi / wired) discover each other regardless of start order. Under WSL2, Windows→WSL UDP broadcast is limited by NAT and discovery may not work — an environment limitation.

## Ports & protocol

| Port | Purpose |
|------|---------|
| UDP 24260 | Peer discovery (`QLMSG` magic JSON) |
| TCP 24261 | Message sessions (12-byte little-endian header + JSON + body) |

Protocol details are in [`docs/DESIGN.md`](docs/DESIGN.md).

## Environment variables

| Variable | Description |
|----------|-------------|
| `QLANMSG_APPID` | Override the persisted device ID |
| `QLANMSG_TCPPORT` | Override the TCP port (default 24261) |
| `QLANMSG_LOG` | Enable discovery/chat debug logging |
| `QLANMSG_AUTO_ACCEPT` | Auto-accept file transfers and remote-control requests (no dialogs) |
| `QLANMSG_TEST_CHAT` / `QLANMSG_TEST_FILE` / `QLANMSG_TEST_RC` | Send test traffic to a given IP after startup |

## CI

- `.github/workflows/windows.yml`: builds and bundles the Qt runtime on windows-latest, producing a runnable exe.
- `.github/workflows/ios.yml`: generates the Xcode project with XcodeGen on macos-15, builds for the simulator and runs unit tests; compile errors are surfaced as PR/commit annotations on failure.
