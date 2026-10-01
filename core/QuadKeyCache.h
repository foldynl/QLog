#ifndef QLOG_CORE_QUADKEYCACHE_H
#define QLOG_CORE_QUADKEYCACHE_H

#include <QCache>
#include <QString>
#include <QPair>
#include <QList>

template<typename Value>
class QuadKeyCache : public QCache<QPair<QPair<int, int>, QPair<QString, QString>>, Value>
{
public:
    using Key = QPair<QPair<int, int>, QPair<QString, QString>>;

    explicit QuadKeyCache(int maxCost = 100) : QCache<Key, Value>(maxCost) {}

    void insert(const int keyA, const int keyB, const QString& keyC, const QString& keyD, Value* value)
    {
        Key key = qMakePair(qMakePair(keyA, keyB), qMakePair(keyC, keyD));
        QCache<Key, Value>::insert(key, value);
    }

    Value* value(const int keyA, const int keyB, const QString& keyC, const QString& keyD) const
    {
        Key key = qMakePair(qMakePair(keyA, keyB), qMakePair(keyC, keyD));
        return QCache<Key, Value>::object(key);
    }

    bool contains(const int keyA, const int keyB, const QString& keyC, const QString& keyD) const
    {
        Key key = qMakePair(qMakePair(keyA, keyB), qMakePair(keyC, keyD));
        return QCache<Key, Value>::contains(key);
    }

    int size() const
    {
        return QCache<Key, Value>::size();
    }

    void invalidate(const int keyA, const int keyB)
    {
        QList<Key> keysToRemove;

        for( const Key& key : QCache<Key, Value>::keys() )
        {
            if (key.first.first == keyA && key.first.second == keyB)
                keysToRemove.append(key);
        }

        for( const Key& key : keysToRemove )
            QCache<Key, Value>::remove(key);
    }
};


#endif // QLOG_CORE_QUADKEYCACHE_H
