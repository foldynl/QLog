#ifndef RIG_RIGCTLDMANAGER_H
#define RIG_RIGCTLDMANAGER_H

#include <QObject>
#include <QProcess>
#include "data/RigProfile.h"

struct RigctldVersion
{
    int major = -1;
    int minor = -1;
    int patch = -1;
    bool isValid() const { return major >= 0; }
};

class RigctldManager : public QObject
{
    Q_OBJECT

public:
    explicit RigctldManager(QObject *parent = nullptr);
    ~RigctldManager();

    bool start(const RigProfile &profile);
    void stop();
    bool isRunning() const;

    QString getConnectHost() const { return "127.0.0.1"; }
    quint16 getConnectPort() const { return currentPort; }

    static QString findRigctldPath();
    static RigctldVersion getVersion(const QString &rigctldPath = QString());
    static QStringList buildArguments(const RigProfile &profile);

signals:
    void started();
    void stopped();
    void errorOccurred(const QString &error);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStdout();
    void onReadyReadStderr();

private:
    friend class RigctldManagerTest;

    bool waitForRigctldReady(int timeoutMs = 5000);

    QProcess *rigctldProcess = nullptr;
    quint16 currentPort = 4532;
    bool stoppingInProgress = false;
};

#endif // RIG_RIGCTLDMANAGER_H
