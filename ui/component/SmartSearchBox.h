#ifndef SMARTSEARCHBOX_H
#define SMARTSEARCHBOX_H

#include <QWidget>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QLineEdit>
#include <QListView>
#include <QDialog>

class SmartSearchBox : public QWidget
{
    Q_OBJECT
public:
    explicit SmartSearchBox(QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model);

    void setModelColumn(int column);
    QVariant currentValue(int column);
    QString currentText() const;
    int currentIndex();
    void setCurrentText(const QString &text);
    void adjustMaxSize();
    void refreshModel();
    void setHighlightWhenEnable(bool state);

signals:
    void currentTextChanged(QString);

private slots:
    void onTextChanged(const QString &text);
    void onItemClicked(const QModelIndex &index);
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QLineEdit *searchField;
    QListView *listView;
    QDialog *popup;
    QSortFilterProxyModel *filterModel;
    QPushButton *openButton;
    int selectedColumn;
    int maxRowsInList;
    QModelIndex currentValueIndex;
    bool highlightWhenEnable;
    void adjustPopupSize();
    void changeButtonText(const QString &text);
};

#endif // SMARTSEARCHBOX_H
