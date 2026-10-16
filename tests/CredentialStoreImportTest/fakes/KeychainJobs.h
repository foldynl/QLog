#ifndef QLOG_TEST_KEYCHAINJOBS_H
#define QLOG_TEST_KEYCHAINJOBS_H

#include <QObject>
#include <QMap>
#include <QTimer>

namespace QKeychain
{
enum Error { NoError, EntryNotFound, OtherError };

class Job : public QObject
{
    Q_OBJECT

public:
    enum Operation { READ, WRITE, DELETE };
    Job(const QString &service, Operation operation)
        : service(service), operation(operation) {}
    void setAutoDelete(bool) {}
    void setKey(const QString &value) { key = value; }
    void setTextData(const QString &value) { text = value; }
    QString textData() const { return text; }
    Error error() const { return result; }
    QString errorString() const { return QStringLiteral("Test backend failure"); }
    static QPair<QString, QString> credentialIdentity(const QString &service, const QString &key)
    {
#ifdef Q_OS_WIN
        return qMakePair(service.toCaseFolded(), key.toCaseFolded());
#else
        return qMakePair(service, key);
#endif
    }
    void start()
    {
        QTimer::singleShot(0, this, [this]()
        {
            const QPair<QString, QString> identity = credentialIdentity(service, key);
            if ( operation == failedOperation && identity.second == failedKey )
                result = OtherError;
            else if ( operation == WRITE )
                passwords[identity] = text;
            else if ( !passwords.contains(identity) )
                result = EntryNotFound;
            else if ( operation == READ )
                text = passwords.value(identity);
            else
                passwords.remove(identity);
            emit finished(this);
        });
    }

    static QMap<QPair<QString, QString>, QString> passwords;
    static Operation failedOperation;
    static QString failedKey;

signals:
    void finished(QKeychain::Job *job);

private:
    QString service;
    Operation operation;
    QString key;
    QString text;
    Error result = NoError;
};

class ReadPasswordJob : public Job
{
public:
    explicit ReadPasswordJob(const QString &service) : Job(service, READ) {}
};
class WritePasswordJob : public Job
{
public:
    explicit WritePasswordJob(const QString &service) : Job(service, WRITE) {}
};
class DeletePasswordJob : public Job
{
public:
    explicit DeletePasswordJob(const QString &service) : Job(service, DELETE) {}
};
}
#endif
