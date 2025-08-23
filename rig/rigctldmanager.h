#pragma once
#include <QObject>
#include <QProcess>
#include <QTcpServer>
#include "data/RigProfile.h"
#include "hamlib/rig.h"

#ifndef RIGCTLDMANAGER_H
#define RIGCTLDMANAGER_H

class RigctldManager : public QObject {
    Q_OBJECT
public:
    explicit RigctldManager(QObject* parent=nullptr);

    bool startFromProfile(const RigProfile& prof,
                          quint16 preferPort,  // e.g. 4532; 0 = auto
                          QString rigctldPath, // resolved path (bundle or user setting)
                          QString* errorOut = nullptr);

    void stop();
    bool isRunning() const { return proc_.state() == QProcess::Running; }
    quint16 listenPort() const { return port_; }
    QString findRigctldExecutable();
    void setDebugLogsEnabled(bool v) {debugLogs_ = v;}

signals:
    void rigctldOutput(QString);
    void rigctldError(QString);
    void rigctldExited(int, QProcess::ExitStatus);

    // RigctldManager.h (only the private section shown)
private:
    static bool isTcpPortOpen(const QHostAddress& host, quint16 port, int timeoutMs = 250);
    static quint16 findFreePort(quint16 preferred);
    static QStringList buildRigctldArgs(const RigProfile& p, quint16 tcpPortToServeOn);
    static QString normalizeParity(const QString& p);
    static QString normalizeHandshake(const QString& s);

    QProcess proc_;
    quint16  port_ = 0;
    // RigctldManager.h (private:)
    void appendToRigctldLog(const QString& s);
    QString logPath_;
    bool debugLogs_ = true;
};

#endif // RIGCTLDMANAGER_H
