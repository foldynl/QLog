#ifndef QLOG_UI_QSOFILTERDETAIL_H
#define QLOG_UI_QSOFILTERDETAIL_H

#include <QDialog>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include "core/LogLocale.h"
#include "core/QSOFilterManager.h"

class QStackedWidget;

namespace Ui {
class QSOFilterDetail;
}

class QSOFilterDetail : public QDialog
{
    Q_OBJECT

public:
    explicit QSOFilterDetail(const QString &filterName = QString(), QWidget *parent = nullptr,
                             bool clone = false);
    explicit QSOFilterDetail(const QSOFilter &filter, QWidget *parent = nullptr);
    ~QSOFilterDetail();
    const QSOFilter &filter() const { return editedFilter; }

public slots:
    void addCondition(int fieldIdx = -1, int operatorId = -1, QString value = QString());
    void save();
    void filterNameChanged(const QString&);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    class Condition;
    QList<Condition *> conditions;
    Ui::QSOFilterDetail *ui;
    QString filterName;
    int condCount;
    QStringList filterNamesList;
    bool parametersOnly = false;
    QSOFilter editedFilter;

private:
    void loadFilter(const QString &filterName);
    void loadFilter(const QSOFilter &filter);
    QSOFilter readFilter() const;
    static QString editorValue(QStackedWidget *stack);
    bool filterExists(const QString &filterName);
    static QWidget *fieldEditor(const Condition *row);
    void populateComboBox(QComboBox *, const QMap<QString, QString> &, const QString &);

    LogLocale locale;
};

#endif // QLOG_UI_QSOFILTERDETAIL_H
