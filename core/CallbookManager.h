#ifndef QLOG_CORE_CALLBOOKMANAGER_H
#define QLOG_CORE_CALLBOOKMANAGER_H

#include <QObject>
#include <QPointer>
#include "service/GenericCallbook.h"

class CallbookManager : public QObject
{
    Q_OBJECT
public:
    explicit CallbookManager(QObject *parent = nullptr);

    void queryCallsign(const QString &callsign);
    bool isActive();

signals:
    void loginFailed(QString);
    void callsignResult(const QMap<QString, QString>& data);
    void callsignNotFound(QString);
    void lookupError(QString);

public slots:
    void initCallbooks();
    void abortQuery();

private slots:
    void primaryCallbookCallsignNotFound(const QString&);
    void secondaryCallbookCallsignNotFound(const QString&);
    void processCallsignResult(const QMap<QString, QString>& data);

private:
    GenericCallbook *createCallbook(const QString&);

private:
    QPointer<GenericCallbook> primaryCallbook;
    bool primaryCallbookAuthSuccess;
    QPointer<GenericCallbook> secondaryCallbook;
    bool secondaryCallbookAuthSuccess;
    QString currentQueryCallsign;
    static QCache<QString, QMap<QString, QString>> queryCache;

};

#endif // QLOG_CORE_CALLBOOKMANAGER_H
