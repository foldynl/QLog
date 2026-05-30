#ifndef QLOG_UI_MODESUBMODEDELEGATE_H
#define QLOG_UI_MODESUBMODEDELEGATE_H

#include <QStyledItemDelegate>
#include <QString>
#include <QWidget>

class QComboBox;
class ModeSelectionController;

class ModeSubmodeEditor : public QWidget
{
public:
    explicit ModeSubmodeEditor(bool showMode, QWidget *parent = nullptr);

    void setModeSubmode(const QString &mode, const QString &submode);
    QString mode() const;
    QString submode() const;

private:
    QComboBox *modeCombo;
    QComboBox *submodeCombo;
    ModeSelectionController *modeController;
};

class ModeSubmodeDelegate : public QStyledItemDelegate
{
public:
    explicit ModeSubmodeDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
};

class SubmodeDelegate : public QStyledItemDelegate
{
public:
    explicit SubmodeDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
};

#endif // QLOG_UI_MODESUBMODEDELEGATE_H
