#pragma once

#include <QString>

#include "common/Protocol.h"

struct Peer {
    QString id; // device app id
    QString name;
    QString host;
    QString os;
    QString ver;
    QString ip;
    quint16 tcpPort = qlm::kTcpPort;
    qint64 lastSeenMs = 0;

    bool online() const { return lastSeenMs > 0; }
    QString displayName() const { return name; }
};
