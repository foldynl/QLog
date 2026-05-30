#include <QSignalBlocker>
#include "ModeSelectionController.h"
#include "core/debug.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.ui.modeselectioncontroller");

ModeSelectionController::ModeSelectionController(QComboBox *modeCombo,
                                                 QComboBox *submodeCombo,
                                                 bool enabledOnly,
                                                 bool selectFirstSubmode,
                                                 bool hideSubmodeWhenEmpty,
                                                 bool fallbackToFirstMode,
                                                 QObject *parent) :
    QObject(parent),
    modeCombo(modeCombo),
    submodeCombo(submodeCombo),
    modeModel(nullptr),
    submodeModel(nullptr),
    enabledOnly(enabledOnly),
    selectFirstSubmode(selectFirstSubmode),
    hideSubmodeWhenEmpty(hideSubmodeWhenEmpty),
    fallbackToFirstMode(fallbackToFirstMode),
    nameColumn(-1)
{
    FCT_IDENTIFICATION;

    if ( !modeCombo || !submodeCombo )
        return;

    QSignalBlocker modeBlocker(modeCombo);
    QSignalBlocker submodeBlocker(submodeCombo);

    modeModel = new QSqlTableModel(this);
    modeModel->setTable("modes");
    if ( enabledOnly ) modeModel->setFilter("enabled = true");
    nameColumn = modeModel->fieldIndex("name");
    modeModel->setSort(nameColumn, Qt::AscendingOrder);
    modeModel->select();

    modeCombo->setModel(modeModel);
    modeCombo->setModelColumn(nameColumn);

    submodeModel = new QStringListModel(this);
    submodeCombo->setModel(submodeModel);
}

void ModeSelectionController::applyCurrentMode()
{
    FCT_IDENTIFICATION;

    if ( !modeModel || !submodeModel || !modeCombo || !submodeCombo ) return;

    if ( modeCombo->currentIndex() < 0 )
    {
        applySubmodes(QStringList());
        emit defaultReportChanged(QString());
        return;
    }

    const QSqlRecord record = currentRecord();
    if ( record.isEmpty() )
    {
        applySubmodes(QStringList());
        emit defaultReportChanged(QString());
        return;
    }

    const QStringList submodeList = Data::instance()->submodesForMode(record.value("name").toString());

    applySubmodes(submodeList);
    emit defaultReportChanged(record.value("rprt").toString());
}

void ModeSelectionController::reloadModel()
{
    FCT_IDENTIFICATION;

    if ( !modeModel || !modeCombo || !submodeCombo )
        return;

    const QString prevMode = modeCombo->currentText();
    const QString prevSubmode = submodeCombo->currentText();

    QSignalBlocker modeBlocker(modeCombo);
    QSignalBlocker submodeBlocker(submodeCombo);

    modeModel->select();

    int modeIndex = modeCombo->findText(prevMode);
    if ( modeIndex >= 0 )
        modeCombo->setCurrentIndex(modeIndex);
    else if ( fallbackToFirstMode && modeCombo->count() > 0 )
        modeCombo->setCurrentIndex(0);
    else
        modeCombo->setCurrentIndex(-1);

    applyCurrentMode();

    if ( !prevSubmode.isEmpty() )
    {
        int submodeIndex = submodeCombo->findText(prevSubmode);
        if ( submodeIndex >= 0 )
            submodeCombo->setCurrentIndex(submodeIndex);
    }
}

void ModeSelectionController::applySubmodes(const QStringList &submodeList)
{
    FCT_IDENTIFICATION;

    QSignalBlocker blocker(submodeCombo);

    if ( submodeList.isEmpty() )
    {
        submodeModel->setStringList(QStringList());

        if ( hideSubmodeWhenEmpty )
            submodeCombo->setVisible(false);
        else
            submodeCombo->setEnabled(false);

        if ( selectFirstSubmode ) submodeCombo->setCurrentIndex(-1);

        return;
    }

    QStringList list = submodeList;
    list.prepend("");
    submodeModel->setStringList(list);

    if ( hideSubmodeWhenEmpty )
        submodeCombo->setVisible(true);
    else
        submodeCombo->setEnabled(true);

    if ( selectFirstSubmode ) submodeCombo->setCurrentIndex(1);
}

QSqlRecord ModeSelectionController::currentRecord() const
{
    FCT_IDENTIFICATION;

    if ( !modeModel ) return QSqlRecord();

    return modeModel->record(modeCombo->currentIndex());
}
