#ifndef STATIONTABLEWIDGET_H
#define STATIONTABLEWIDGET_H

#include <QWidget>
#include <QTableView>
#include "data/Band.h"

class DxccTableModel;

class StationTableWidget : public QTableView
{
    Q_OBJECT
public:
    explicit StationTableWidget(QWidget*parent = nullptr);

signals:

public slots:
    void clear();
    void setCallsign(QString callsign, Band band);

private:
    DxccTableModel* dxccTableModel;
    QStringList headerStrings;

};

#endif // STATIONTABLEWIDGET_H

