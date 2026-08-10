#include "ui/MainWindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLocale>
#include <QNetworkProxy>
#include <QStringList>

namespace {
// WSL/WSLg hosts have no CJK fonts on the Linux side, but the Windows fonts
// (Microsoft YaHei, SimHei, ...) are mounted under /mnt/c/Windows/Fonts.
// Load one at startup so Chinese UI text renders instead of tofu boxes.
void applyCjkFont() {
    static const char *const kCandidates[] = {
        "/mnt/c/Windows/Fonts/msyh.ttc",
        "/mnt/c/Windows/Fonts/msyhbd.ttc",
        "/mnt/c/Windows/Fonts/simhei.ttf",
        "/mnt/c/Windows/Fonts/simsun.ttc",
    };
    for (const char *path : kCandidates) {
        if (!QFileInfo::exists(QString::fromUtf8(path)))
            continue;
        const int id = QFontDatabase::addApplicationFont(QString::fromUtf8(path));
        if (id < 0)
            continue;
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (families.isEmpty())
            continue;
        QFont font = QGuiApplication::font();
        font.setFamily(families.first());
        QGuiApplication::setFont(font);
        return;
    }
}
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QLanMsg"));
    QApplication::setOrganizationName(QStringLiteral("QLanMsg"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

// LAN-only messenger: never route any socket through a SOCKS/HTTP proxy.
// Global-proxy software (Clash, V2rayN, ...) sets env vars (ALL_PROXY,
// socks_proxy) that Qt picks up by default; a SOCKS proxy breaks the UDP
// discovery socket (bind() fails -> multicast join error "not in
// BoundState" / "Connection not allowed by SOCKSv5 server").
QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

applyCjkFont();

// Telegram-style light theme: white sidebars, light-gray chat canvas and the
// Telegram blue (#3390ec) accent everywhere.
QString theme = QStringLiteral(R"(
    QMainWindow, QDialog, QMessageBox, QInputDialog { background: #f4f6f8; }
    QWidget { font-size: 14px; }

    QToolBar { background: #ffffff; border: none; border-bottom: 1px solid #e3e6ea; padding: 2px 8px; spacing: 6px; }
    QToolBar QToolButton { padding: 6px 12px; border-radius: 8px; color: #0f0f0f; background: transparent; }
    QToolBar QToolButton:hover { background: #f2f5f7; }

    QLineEdit#searchBox { background: #ffffff; border: 1px solid #e3e6ea; border-radius: 10px; padding: 4px 10px; }
    QLineEdit#searchBox:focus { border: 1px solid #3390ec; }

    QListWidget#peerList { background: #ffffff; border: none; outline: none; }
    QListWidget#peerList::item { border: none; padding: 0; }

    QWidget#chatHeader { background: #ffffff; border-bottom: 1px solid #e3e6ea; }
    QWidget#chatBubbles { background: #e9edf2; }
    QScrollArea#chatScroll { border: none; background: #e9edf2; }

    QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
    QScrollBar::handle:vertical { background: rgba(0,0,0,0.18); border-radius: 4px; min-height: 30px; }
    QScrollBar::handle:vertical:hover { background: rgba(0,0,0,0.28); }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

    QTextEdit#chatInput { background: #ffffff; border: 1px solid #e3e6ea; border-radius: 12px; padding: 8px 12px; }
    QTextEdit#chatInput:focus { border: 1px solid #3390ec; }

    QPushButton#attachButton { border: none; border-radius: 18px; background: transparent; }
    QPushButton#attachButton:hover { background: #f2f5f7; }
    QPushButton#sendButton { border: none; border-radius: 18px; background: transparent; }
    QPushButton#sendButton:hover { background: rgba(51,144,236,0.12); }

    QStatusBar { background: #ffffff; border-top: 1px solid #e3e6ea; color: #8d969e; }

    QMenu { background: #ffffff; border: 1px solid #e3e6ea; border-radius: 8px; padding: 6px; }
    QMenu::item { padding: 6px 26px 6px 12px; border-radius: 6px; }
    QMenu::item:selected { background: #3390ec; color: #ffffff; }
    QMenu::separator { height: 1px; background: #eef1f4; margin: 4px 8px; }

    QToolTip { background: #ffffff; color: #0f0f0f; border: 1px solid #e3e6ea; padding: 4px 8px; }
)");
qApp->setStyleSheet(theme);

    MainWindow w;
    w.show();
    return app.exec();
}
