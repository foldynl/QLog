#ifndef QLOG_CORE_LOGPARAM_H
#define QLOG_CORE_LOGPARAM_H

#include <QObject>
#include <QDate>

class LogParam : public QObject
{
    Q_OBJECT
public:
    explicit LogParam(QObject *parent = nullptr);

    static bool setParam(const QString&, const QString&);
    static bool setParam(const QString&, const QDate&);
    static QVariant getParam(const QString&);

private:
    static QCache<QString, QVariant> localCache;
};

#endif // QLOG_CORE_LOGPARAM_H
