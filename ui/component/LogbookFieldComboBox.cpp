#include "ui/component/LogbookFieldComboBox.h"

#include <QSqlDatabase>
#include <QSqlRecord>
#include <algorithm>

LogbookFieldComboBox::LogbookFieldComboBox(QWidget *parent) :
    QComboBox(parent)
{
}

void LogbookFieldComboBox::populate(ValueMode valueMode,
                                    EmptyMode emptyMode,
                                    const QString &emptyText)
{
    populateCombo(this, valueMode, emptyMode, emptyText);
}

LogbookModel::ColumnID LogbookFieldComboBox::currentColumnId() const
{
    return static_cast<LogbookModel::ColumnID>(currentData().toInt());
}

QString LogbookFieldComboBox::currentDbFieldName() const
{
    return currentData().toString();
}

void LogbookFieldComboBox::setCurrentColumnId(LogbookModel::ColumnID columnId)
{
    const int index = findData(static_cast<int>(columnId));

    if ( index >= 0 )
        setCurrentIndex(index);
}

void LogbookFieldComboBox::setCurrentDbFieldName(const QString &dbFieldName)
{
    const int index = findData(dbFieldName);

    if ( index >= 0 )
        setCurrentIndex(index);
}

void LogbookFieldComboBox::populateCombo(QComboBox *combo,
                                         ValueMode valueMode,
                                         EmptyMode emptyMode,
                                         const QString &emptyText)
{
    if ( !combo )
        return;

    combo->clear();

    if ( emptyMode != EmptyMode::None )
        combo->addItem(emptyMode == EmptyMode::Blank ? QString() : emptyText,
                       emptyData(valueMode));

    const QList<FieldItem> items = fieldItems();
    for ( const FieldItem &item : items )
        combo->addItem(item.label, itemData(item, valueMode));
}

QList<LogbookFieldComboBox::FieldItem> LogbookFieldComboBox::fieldItems()
{
    QList<FieldItem> items;
    const QSqlRecord contactsRecord = QSqlDatabase::database().record("contacts");

    for ( int i = LogbookModel::ColumnID::COLUMN_ID; i < LogbookModel::ColumnID::COLUMN_LAST_ELEMENT; ++i )
    {
        const LogbookModel::ColumnID columnId = static_cast<LogbookModel::ColumnID>(i);
        const QString label = LogbookModel::getFieldNameTranslation(columnId);

        if ( label.isEmpty() )
            continue;

        const QString dbFieldName = contactsRecord.fieldName(i);

        if ( dbFieldName.isEmpty() )
            continue;

        items.append({columnId, label, dbFieldName});
    }

    std::sort(items.begin(), items.end(),
              [](const FieldItem &a, const FieldItem &b)
    {
        return a.label.localeAwareCompare(b.label) < 0;
    });

    return items;
}

QVariant LogbookFieldComboBox::itemData(const FieldItem &item, ValueMode valueMode)
{
    return valueMode == ValueMode::ColumnId
           ? QVariant(static_cast<int>(item.columnId))
           : QVariant(item.dbFieldName);
}

QVariant LogbookFieldComboBox::emptyData(ValueMode valueMode)
{
    return valueMode == ValueMode::ColumnId
           ? QVariant(static_cast<int>(LogbookModel::COLUMN_INVALID))
           : QVariant(QString());
}
