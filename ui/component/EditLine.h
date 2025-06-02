#ifndef QLOG_UI_COMPONENT_EDITLINE_H
#define QLOG_UI_COMPONENT_EDITLINE_H

#include <QObject>
#include <QLineEdit>

class NewContactEditLine : public QLineEdit
{
    Q_OBJECT

public:
    explicit NewContactEditLine(QWidget *parent = nullptr);
    void setText(const QString & text);
    void spaceForbidden(bool);

signals:
    void focusIn();
    void focusOut();

protected:
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool spaceForbiddenFlag;
};

class NewContactRSTEditLine : public NewContactEditLine
{
    Q_OBJECT

public:
    explicit NewContactRSTEditLine(QWidget *parent = nullptr);
    void setSelectionBackwardOffset(int offset);

protected:
    void focusInEvent(QFocusEvent* event) override;
    int focusInSelectionBackwardOffset;
};

class SerialPortEditLine : public QLineEdit
{
    Q_OBJECT

public:
    explicit SerialPortEditLine(QWidget *parent = nullptr);

protected:
    void focusInEvent(QFocusEvent* event) override;
};

#endif // QLOG_UI_COMPONENT_EDITLINE_H
