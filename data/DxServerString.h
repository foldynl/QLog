#ifndef QLOG_DATA_DXSERVERSTRING_H
#define QLOG_DATA_DXSERVERSTRING_H

#include <QString>

class DxServerString
{
public:
    explicit DxServerString(const QString &connectString,
                            const QString &defaultUsername = QString());

    static bool isValidServerString(const QString &);
    bool isValid() const {return valid;};
    QString getUsername() const {return username;};
    QString getHostname() const {return hostname;};
    int getPort() const {return port;};
    const QString getPasswordStorageKey() const;

private:
    static const QRegularExpression serverStringRegEx();

    QString username, hostname;
    int port;
    bool valid;
};

#endif // QLOG_DATA_DXSERVERSTRING_H
