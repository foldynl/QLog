#include <QLineEdit>
#include "MultiselectCompleter.h"

MultiselectCompleter::MultiselectCompleter(const QStringList& items, QObject* parent)
    : QCompleter(items, parent)
{
}

QString MultiselectCompleter::pathFromIndex( const QModelIndex& index ) const
{
    QString path = QCompleter::pathFromIndex(index);
    const QString &text = static_cast<QLineEdit*>(widget())->text();

    int pos = text.lastIndexOf(',');
    if ( pos >= 0 )
        path = text.left(pos) + ", " + path;

    return path;
}

QStringList MultiselectCompleter::splitPath( const QString& path ) const
{
    int pos = path.lastIndexOf(',') + 1;
    while ( pos < path.length() && path.at(pos) == QLatin1Char(' ') )
        pos++;

    return QStringList(path.mid(pos));
}
