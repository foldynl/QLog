#ifndef QLOG_MODELS_SHORTCUTEDITORMODEL_H
#define QLOG_MODELS_SHORTCUTEDITORMODEL_H

#include <QAbstractTableModel>
#include <QAction>

class ShortcutEditorModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ShortcutEditorModel(const QList<QAction *> &actions,
                                 const QStringList &builtInStaticActions,
                                 QObject *parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    enum column_id
    {
        COLUMN_DESCRIPTION = 0,
        COLUMN_SHORTCUT = 1,
    };

signals:
    void conflictDetected(const QString &);

private:
    QList<QAction *> actionList;
    QStringList builtInStaticActionList;

    const QAction *findShortcut(const QList<QAction *> &list,
                                const QAction *currAction,
                                const QString&) const;
};

#endif // QLOG_MODELS_SHORTCUTEDITORMODEL_H
