#ifndef QLOG_UI_EDITLINE_H
#define QLOG_UI_EDITLINE_H

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

#endif // QLOG_UI_EDITLINE_H
