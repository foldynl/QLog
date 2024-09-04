#ifndef QLOG_UI_MAINLAYOUTEDITOR_H
#define QLOG_UI_MAINLAYOUTEDITOR_H

#include <QDialog>
#include <QPointer>
#include <QStringListModel>
#include "ui/NewContactWidget.h"
#include "data/MainLayoutProfile.h"

namespace Ui {
class MainLayoutEditor;
}

class StringListModel : public QStringListModel
{
public:
    StringListModel(QObject *parent = nullptr) :
        QStringListModel(parent){};

    void append (const QString& string)
    {
        insertRows(rowCount(), 1);
        setData(index(rowCount()-1), string);
    };

    void deleteItem(const QModelIndex &index)
    {
        if (!index.isValid() || index.row() >= stringList().size())
                return;
        removeRow(index.row());
    }

    void deleteItem(const QString &value)
    {
        QModelIndexList itemIndexList = match(index(0,0),
              Qt::DisplayRole,
              value,
              1,
              Qt::MatchExactly);

        for (const QModelIndex & itemIndex: qAsConst(itemIndexList))
        {
            deleteItem(itemIndex);
        }
    }

    void moveUp(const QModelIndex &index)
    {
        if ( index.row() - 1 < 0 )
            return;

        moveRow(index.parent(), index.row(), index.parent(), index.row() - 1);
    }

    void moveDown(const QModelIndex &inIndex)
    {
        if ( inIndex.row() + 1 >= rowCount() )
            return;

         moveUp(index(inIndex.row()+1));
    }

    StringListModel& operator<<(const QString& string)
    {
        append(string);
        return *this;
    };
};

class MainLayoutEditor : public QDialog
{
    Q_OBJECT

public:
    explicit MainLayoutEditor(const QString &layoutName = QString(),
                                    QWidget *parent = nullptr);
    ~MainLayoutEditor();

private:
    Ui::MainLayoutEditor *ui;
    StringListModel *availableFieldsModel;
    StringListModel *qsoRowAFieldsModel;
    StringListModel *qsoRowBFieldsModel;
    StringListModel *detailColAFieldsModel;
    StringListModel *detailColBFieldsModel;
    StringListModel *detailColCFieldsModel;
    QByteArray mainGeometry;
    QByteArray mainState;
    bool darkMode;

    NewContactDynamicWidgets *dynamicWidgets;

private slots:
    void save();
    void profileNameChanged(const QString&);
    void clearMainLayoutClick();

private:
    void fillWidgets(const MainLayoutProfile &profile);
    bool layoutNameExists(const QString &layoutName);
    void moveField(StringListModel *source,
                   StringListModel *destination,
                   const QModelIndexList &sourceIndexList);
    void connectQSORowButtons();
    void connectDetailColsButtons();
    QList<int> getFieldIndexes(StringListModel *model);

    const QString statusUnSavedText = tr("Unsaved");
};

#endif // QLOG_UI_MAINLAYOUTEDITOR_H
