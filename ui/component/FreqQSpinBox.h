#ifndef QLOG_UI_COMPONENT_FREQQSPINBOX_H
#define QLOG_UI_COMPONENT_FREQQSPINBOX_H

#include <data/Band.h>
#include "ui/component/BaseDoubleSpinBox.h"

class QAction;
class QMenu;

class FreqQSpinBox : public BaseDoubleSpinBox
{
    Q_OBJECT

public:
    FreqQSpinBox(QWidget *parent = nullptr);
    virtual ~FreqQSpinBox() {};
    void setSelectionModeEnabled(bool enabled);
    void setDebounceEnabled(bool enabled);
    void setDebounceIntervalMs(int ms);
    void enableBandSelection();
    void setBandSelectionEnabled(bool enabled);
    bool setBand(const QString &bandName);
    void setValue(double value);
    bool hasBand() const;
    bool hasFrequency() const;
    QString selectedBand() const;

public slots:
    void loadBands();

signals:
    void debouncedValueChanged(double value);
    void bandSelected(const QString &bandName);

protected:
    QValidator::State validate(QString &input, int &pos) const override;
    double valueFromText(const QString &text) const override;
    QAbstractSpinBox::StepEnabled stepEnabled() const override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void stepBy(int steps) override;

private slots:
    void onValueChangedImmediate(double v);
    void flushDebounced();
    void showBandMenu();
    void bandMenuTriggered(QAction *action);

private:
    void increaseByBand();
    void decreaseByBand();
    void stepBand(int steps);
    int findBand(const QString &bandName) const;
    void maybeSelectAll();
    void populateBandMenu();
    void enterFrequencyMode();
    void updateButtonSymbols();
    void updateOutOfBandState();
    bool containsLettersOutsideSuffix(const QString &text) const;

    QList<Band> enabledBands;
    bool selectionModeEnabled;
    bool bandSelectionEnabled = true;
    QTimer debounceTimer;
    int    debounceMs = 150;
    bool   debounceEnabled = false;
    double pendingValue = 0.0;
    bool   hasPending = false;
    QAction *bandMenuButton = nullptr;
    QMenu *bandMenu = nullptr;
    int bandIndex = -1;
    double lastFrequency = 0.0;
};

#endif // QLOG_UI_COMPONENT_FREQQSPINBOX_H
