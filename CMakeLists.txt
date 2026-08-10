cmake_minimum_required(VERSION 3.16)
project(qlanmsg VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(Qt6 COMPONENTS Multimedia QUIET)

set(SOURCES
    src/main.cpp
    src/common/Config.cpp
    src/discovery/PeerDiscovery.cpp
    src/net/PeerSession.cpp
    src/net/NetworkService.cpp
    src/net/FileSender.cpp
    src/net/FileReceiver.cpp
    src/remote/RemoteServer.cpp
    src/ui/MainWindow.cpp
    src/ui/ChatWidget.cpp
    src/ui/PeerListDelegate.cpp
    src/ui/RemoteDialog.cpp
    src/ui/SettingsDialog.cpp
    resources/resources.qrc
)

if(WIN32)
    list(APPEND SOURCES src/remote/ScreenSourceWindows.cpp src/remote/InputSinkWindows.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND SOURCES src/remote/ScreenSourceX11.cpp src/remote/InputSinkX11.cpp)
endif()

qt_add_executable(qlanmsg ${SOURCES})
target_include_directories(qlanmsg PRIVATE src)
target_link_libraries(qlanmsg PRIVATE Qt6::Widgets Qt6::Network)

# Optional Qt Multimedia: real notification sound via QSoundEffect.
# Falls back to QApplication::beep() when the module is unavailable.
if(Qt6Multimedia_FOUND)
    target_link_libraries(qlanmsg PRIVATE Qt6::Multimedia)
    target_compile_definitions(qlanmsg PRIVATE QLANMSG_HAS_MULTIMEDIA)
    message(STATUS "Qt Multimedia found - notification sound enabled")
else()
    message(STATUS "Qt Multimedia not found - using system beep fallback")
endif()

if(WIN32)
    set_target_properties(qlanmsg PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(qlanmsg PRIVATE user32 gdi32)
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(qlanmsg PRIVATE X11 Xtst Xext)
endif()

if(MSVC)
    target_compile_options(qlanmsg PRIVATE /W3)
else()
    target_compile_options(qlanmsg PRIVATE -Wall -Wextra)
endif()
