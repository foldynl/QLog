#ifndef QLOG_CORE_MULTISELECTCOMPLETER_H
#define QLOG_CORE_MULTISELECTCOMPLETER_H

#include <QCompleter>

class MultiselectCompleter : public QCompleter
{
    Q_OBJECT

public:
    explicit MultiselectCompleter(const QStringList &items,
                                  QObject *parent = nullptr);
    ~MultiselectCompleter() {};

public:
    QString pathFromIndex( const QModelIndex& index ) const;
    QStringList splitPath( const QString& path ) const;
};

#endif // QLOG_CORE_MULTISELECTCOMPLETER_H
