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

    MainWindow w;
    w.show();
    return app.exec();
}
