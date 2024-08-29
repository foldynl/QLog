#ifndef QLOG_CORE_LOTW_H
#define QLOG_CORE_LOTW_H

#include <QObject>
#include <QNetworkReply>
#include <logformat/LogFormat.h>

class QNetworkAccessManager;
class QNetworkReply;

class Lotw : public QObject
{
    Q_OBJECT
public:
    explicit Lotw(QObject *parent = nullptr);
    ~Lotw();

    void update(const QDate &, bool, const QString &);
    void uploadAdif(const QByteArray &);

    static const QString getUsername();
    static const QString getPassword();
    static const QString getTQSLPath(const QString &defaultPath = QDir::rootPath());

    static void saveUsernamePassword(const QString&, const QString&);
    static void saveTQSLPath(const QString&);

signals:
    void updateProgress(int value);
    void updateStarted();
    void updateComplete(QSLMergeStat update);
    void updateFailed(QString);

    void uploadFinished();
    void uploadError(QString);

public slots:
    void processReply(QNetworkReply* reply);
    void abortRequest();

private:
    QNetworkAccessManager* nam;
    QNetworkReply *currentReply;
    QTemporaryFile file;

    static const QString SECURE_STORAGE_KEY;
    static const QString CONFIG_USERNAME_KEY;

    void get(QList<QPair<QString, QString>> params);
};

#endif // QLOG_CORE_LOTW_H
