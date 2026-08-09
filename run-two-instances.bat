@echo off
rem Launch a second QLanMsg instance on the same machine with its own
rem identity and TCP port so two instances can run side by side.
rem Double-click this file next to qlanmsg.exe.
start "" "%~dp0qlanmsg.exe"
set QLANMSG_APPID=instance-b
set QLANMSG_TCPPORT=24262
start "" "%~dp0qlanmsg.exe"
set QLANMSG_APPID=
set QLANMSG_TCPPORT=
