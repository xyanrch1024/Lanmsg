#pragma once

#include <QSettings>
#include <QString>

class Config {
public:
    static Config &instance();

    QString nickname() const;
    void setNickname(const QString &v);

    // Remote control password. Empty means "ask every time".
    QString remotePassword() const;
    void setRemotePassword(const QString &v);

    quint16 tcpPort() const;
    QString downloadDir() const;

    QByteArray appId() const { return m_appId; }
    QSettings &settings() { return m_settings; }

private:
    Config();
    QSettings m_settings;
    QByteArray m_appId;
};
