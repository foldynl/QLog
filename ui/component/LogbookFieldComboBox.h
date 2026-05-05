#ifndef QLOG_UI_COMPONENT_LOGBOOKFIELDCOMBOBOX_H
#define QLOG_UI_COMPONENT_LOGBOOKFIELDCOMBOBOX_H

#include <QComboBox>
#include <QList>
#include <QVariant>

#include "models/LogbookModel.h"

class LogbookFieldComboBox : public QComboBox
{
public:
    enum class ValueMode
    {
        ColumnId,
        DbFieldName
    };

    // Existing users need three variants: no empty item (QSO filter),
    // a blank empty item (Cabrillo), and a named empty item (QSL labels).
    enum class EmptyMode
    {
        None,
        Blank,
        EmptyLabel
    };

    explicit LogbookFieldComboBox(QWidget *parent = nullptr);

    void populate(ValueMode valueMode,
                  EmptyMode emptyMode = EmptyMode::None,
                  const QString &emptyText = QString());

    LogbookModel::ColumnID currentColumnId() const;
    QString currentDbFieldName() const;
    void setCurrentColumnId(LogbookModel::ColumnID columnId);
    void setCurrentDbFieldName(const QString &dbFieldName);

    static void populateCombo(QComboBox *combo,
                              ValueMode valueMode,
                              EmptyMode emptyMode = EmptyMode::None,
                              const QString &emptyText = QString());

private:
    struct FieldItem
    {
        LogbookModel::ColumnID columnId;
        QString label;
        QString dbFieldName;
    };

    static QList<FieldItem> fieldItems();
    static QVariant itemData(const FieldItem &item, ValueMode valueMode);
    static QVariant emptyData(ValueMode valueMode);
};

#endif // QLOG_UI_COMPONENT_LOGBOOKFIELDCOMBOBOX_H
