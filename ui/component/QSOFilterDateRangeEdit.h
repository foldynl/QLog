#ifndef QLOG_UI_QSOFILTERDATERANGEEDIT_H
#define QLOG_UI_QSOFILTERDATERANGEEDIT_H

#include <QWidget>
#include "core/QSOFilterDateRange.h"

class QComboBox;
class QSpinBox;
class QPushButton;
namespace Ui { class QSOFilterDateBoundary; }

class QSOFilterDateRangeEdit : public QWidget
{
    Q_OBJECT

public:
    explicit QSOFilterDateRangeEdit(const QString &value, QWidget *parent = nullptr);
    QString value() const { return range.toString(); }

signals:
    void valueChanged();

private:
    enum class Preset
    {
        Today,
        Yesterday,
        LastDays,
        Week,
        Month,
        Year,
        Custom
    };

    void updateControls();
    void presetChanged();
    void editCustomRange();
    void setupBoundaryEditor(const QSOFilterDateRange::Boundary &boundary,
                             Ui::QSOFilterDateBoundary &ui, QWidget *editor);
    QString boundaryValue(const Ui::QSOFilterDateBoundary &ui) const;

    QSOFilterDateRange range;
    QComboBox *preset;
    QSpinBox *days;
    QPushButton *customButton;
};

#endif
