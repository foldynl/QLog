#include "ModeSubmodeDelegate.h"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVariantMap>

#include "models/LogbookModel.h"
#include "ui/ModeSelectionController.h"

ModeSubmodeEditor::ModeSubmodeEditor(QWidget *parent) :
    QWidget(parent),
    modeCombo(new QComboBox(this)),
    submodeCombo(new QComboBox(this)),
    modeController(nullptr)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(modeCombo);
    layout->addWidget(submodeCombo);

    // Existing QSOs may contain modes disabled for new contacts, therefore the
    // editor must offer all known modes and not only enabled ones.
    modeController = new ModeSelectionController(modeCombo, submodeCombo,
                                                 false, false, false, false, this);
    connect(modeCombo, &QComboBox::currentTextChanged,
            modeController, &ModeSelectionController::applyCurrentMode);
}

void ModeSubmodeEditor::setModeSubmode(const QString &mode, const QString &submode)
{
    modeCombo->setCurrentText(mode);
    modeController->applyCurrentMode();
    submodeCombo->setCurrentText(submode);
}

QString ModeSubmodeEditor::mode() const
{
    return modeCombo->currentText();
}

QString ModeSubmodeEditor::submode() const
{
    return submodeCombo->currentText();
}

ModeSubmodeDelegate::ModeSubmodeDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{
}

QWidget *ModeSubmodeDelegate::createEditor(QWidget *parent,
                                           const QStyleOptionViewItem &,
                                           const QModelIndex &) const
{
    return new ModeSubmodeEditor(parent);
}

void ModeSubmodeDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    ModeSubmodeEditor *modeSubmodeEditor = static_cast<ModeSubmodeEditor *>(editor);
    const QVariantMap value = index.model()->data(index, Qt::EditRole).toMap();

    modeSubmodeEditor->setModeSubmode(value.value("mode").toString(),
                                      value.value("submode").toString());
}

void ModeSubmodeDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                       const QModelIndex &index) const
{
    ModeSubmodeEditor *modeSubmodeEditor = static_cast<ModeSubmodeEditor *>(editor);

    QVariantMap value;
    value.insert("mode", modeSubmodeEditor->mode());
    value.insert("submode", modeSubmodeEditor->submode());

    model->setData(index, value, Qt::EditRole);
}

void ModeSubmodeDelegate::updateEditorGeometry(QWidget *editor,
                                               const QStyleOptionViewItem &option,
                                               const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}
