#include "Config.h"

#include "common/Protocol.h"

#include <QHostInfo>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QDir>

#include <QUuid>

Config::Config()
    : m_settings("QLanMsg", "QLanMsg")
    , m_appId(m_settings.value("appId").toByteArray()) {
    const QByteArray envId = qEnvironmentVariable("QLANMSG_APPID").toUtf8();
    if (!envId.isEmpty()) {
        m_appId = envId;
    } else if (m_appId.isEmpty()) {
        m_appId = QUuid::createUuid().toRfc4122().toHex().left(12);
        m_settings.setValue("appId", m_appId);
    }
}

Config &Config::instance() {
    static Config cfg;
    return cfg;
}

QString Config::nickname() const {
    QString v = m_settings.value("nickname").toString();
    if (v.isEmpty())
        v = QHostInfo::localHostName();
    return v;
}

void Config::setNickname(const QString &v) {
    m_settings.setValue("nickname", v);
}

QString Config::remotePassword() const {
    return m_settings.value("remotePassword").toString();
}

void Config::setRemotePassword(const QString &v) {
    m_settings.setValue("remotePassword", v);
}

quint16 Config::tcpPort() const {
    const QString env = qEnvironmentVariable("QLANMSG_TCPPORT");
    if (!env.isEmpty()) {
        bool ok = false;
        const int v = env.toInt(&ok);
        if (ok && v > 0 && v < 65536)
            return static_cast<quint16>(v);
    }
    return qlm::kTcpPort;
}

QString Config::downloadDir() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (base.isEmpty())
        base = QDir::homePath();
    QDir dir(base + "/qlanmsg");
    dir.mkpath(".");
    return dir.absolutePath();
}
