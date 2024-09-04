#include "ShortcutEditorModel.h"

ShortcutEditorModel::ShortcutEditorModel(const QList<QAction *> &actions,
                                         const QStringList &builtInStaticActions,
                                         QObject *parent)
    : QAbstractTableModel{parent},
      actionList(actions),
      builtInStaticActionList(builtInStaticActions)
{
    std::sort(actionList.begin(),
              actionList.end(),
              [](const QAction *a, const QAction *b)
    {
        return a->toolTip().localeAwareCompare(b->toolTip()) < 0;
    });
}

int ShortcutEditorModel::rowCount(const QModelIndex &) const
{
    return actionList.count();
}

int ShortcutEditorModel::columnCount(const QModelIndex &) const
{
    return 2;
}

QVariant ShortcutEditorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ( role != Qt::DisplayRole || orientation != Qt::Horizontal )
        return QVariant();

    switch ( section )
    {
    case COLUMN_DESCRIPTION: return tr("Description");
    case COLUMN_SHORTCUT: return tr("Shortcut");
    default: return QVariant();
    }
}

QVariant ShortcutEditorModel::data(const QModelIndex &index, int role) const
{
    QAction *action = actionList.at(index.row());

    if ( !action )
        return QVariant();

    if ( role == Qt::DisplayRole )
    {
        switch ( index.column() )
        {
        case COLUMN_DESCRIPTION: return action->toolTip();
        case COLUMN_SHORTCUT: return action->shortcut().toString(QKeySequence::NativeText);
        default: return QVariant();
        }
    }

    return QVariant();
}

bool ShortcutEditorModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if ( role == Qt::EditRole && index.column() == COLUMN_SHORTCUT )
    {
        QAction *action = actionList.at(index.row());
        const QString &newShortcutString = value.toString();

        if ( !action )
            return false;

        if ( newShortcutString.isEmpty() )
        {
            action->setShortcut(QKeySequence());
            emit dataChanged(index, index);
            return true;
        }

        if ( builtInStaticActionList.contains(newShortcutString) )
        {
            emit conflictDetected(tr("Conflict with a built-in shortcut"));
            return false;
        }

        if ( findShortcut(actionList, action, newShortcutString) )
        {
            emit conflictDetected(tr("Conflict with a user-defined shortcut"));
            return false;
        }

        action->setShortcut(QKeySequence(newShortcutString));
        emit dataChanged(index, index);
        return true;
    }

    return QAbstractItemModel::setData(index, value, role);
}

Qt::ItemFlags ShortcutEditorModel::flags(const QModelIndex &index) const
{
    if ( !index.isValid() )
        return Qt::NoItemFlags;

    Qt::ItemFlags modelFlags = QAbstractItemModel::flags(index);
    if ( index.column() == COLUMN_SHORTCUT )
        modelFlags |= Qt::ItemIsEditable;

    return modelFlags;
}

const QAction *ShortcutEditorModel::findShortcut(const QList<QAction *> &list,
                                                 const QAction *currAction,
                                                 const QString &shortcut) const
{
    for ( const QAction* action : list )
    {
        if ( action->shortcut().toString(QKeySequence::NativeText) == shortcut
             && action != currAction )
            return action;
    }

    return nullptr;
}
