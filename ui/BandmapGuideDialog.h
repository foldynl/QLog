#ifndef QLOG_UI_BANDMAPGUIDEDIALOG_H
#define QLOG_UI_BANDMAPGUIDEDIALOG_H

#include <QDialog>
#include <QList>

#include "data/BandmapGuide.h"

namespace Ui {
class BandmapGuideDialog;
}

class QPushButton;
class QWidget;

class BandmapGuideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BandmapGuideDialog(QWidget *parent = nullptr);
    ~BandmapGuideDialog();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void accept() override;
    void guideSelectionChanged();
    void guideNameChanged(const QString &text);
    void newGuide();
    void copyGuide();
    void deleteGuide();
    void importGuide();
    void exportGuide();
    void addRange();
    void removeRange();

private:
    void loadGuides();
    void populateGuideList();
    void showGuideEditor(int index);
    void saveCurrentGuide();
    void populateRangesTable(const QList<BandmapGuide::Range> &ranges);
    void setupRangeRow(int row, const BandmapGuide::Range &range);
    QList<BandmapGuide::Range> readRangesFromTable() const;
    void sortRangesByFrom(QList<BandmapGuide::Range> *ranges) const;
    void chooseColor(QPushButton *button);
    void setColorButton(QPushButton *button, const QColor &color);
    QColor colorFromButton(QPushButton *button) const;
    void installWheelGuard(QWidget *widget);
    bool validateGuides();
    void resolveGuideNameConflict(BandmapGuide::Profile *profile) const;

    Ui::BandmapGuideDialog *ui;
    QList<BandmapGuide::Profile> guides;
    int currentGuideIndex;
};

#endif // QLOG_UI_BANDMAPGUIDEDIALOG_H
