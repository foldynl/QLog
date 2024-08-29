#ifndef MULTISELECTCOMPLETER_H
#define MULTISELECTCOMPLETER_H

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

#endif // MULTISELECTCOMPLETER_H
