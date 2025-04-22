#include <QFocusEvent>
#include <QCompleter>
#include <QSerialPortInfo>

#include "EditLine.h"

NewContactEditLine::NewContactEditLine(QWidget *parent) :
    QLineEdit(parent),
    spaceForbiddenFlag(false)
{

}

void NewContactEditLine::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);

    Qt::FocusReason reason = event->reason();

    if ( reason != Qt::ActiveWindowFocusReason &&
         reason != Qt::PopupFocusReason )
    {
        end(false);
        emit focusIn();
    }

    //Deselect text when focus
    if ( hasSelectedText() && reason != Qt::PopupFocusReason )
    {
        deselect();
    }
}

void NewContactEditLine::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);

    Qt::FocusReason reason = event->reason();

    if ( reason != Qt::ActiveWindowFocusReason &&
         reason != Qt::PopupFocusReason )
    {
        home(false);
        emit focusOut();
    }
}

void NewContactEditLine::keyPressEvent(QKeyEvent *event)
{
    if ( spaceForbiddenFlag && event->key() == Qt::Key_Space )
        focusNextChild();
    else
        QLineEdit::keyPressEvent(event);
}

void NewContactEditLine::setText(const QString &text)
{
    QLineEdit::setText(text);
    home(false);
}

void NewContactEditLine::spaceForbidden(bool inSpaceForbidden)
{
    spaceForbiddenFlag = inSpaceForbidden;
}

NewContactRSTEditLine::NewContactRSTEditLine(QWidget *parent) :
    NewContactEditLine(parent),
    focusInSelectionBackwardOffset(1)
{
}

void NewContactRSTEditLine::setSelectionBackwardOffset(int offset)
{
    focusInSelectionBackwardOffset = offset;
}

void NewContactRSTEditLine::focusInEvent(QFocusEvent *event)
{
    NewContactEditLine::focusInEvent(event);

    if ( event->reason() != Qt::PopupFocusReason && !text().isEmpty() && text().length() >= focusInSelectionBackwardOffset )
        setSelection(text().length() - focusInSelectionBackwardOffset, 1);
}

SerialPortEditLine::SerialPortEditLine(QWidget *parent) :
    QLineEdit(parent)
{
#if defined(Q_OS_WIN)
    // setInputMask("COM000"); // temporarily removed - it does not work in a user-friendly
                               // because when you click, the cursor can reach the end
                               // and in that case the mask does not work correctly.
    QStringList portNames;
    const QList<QSerialPortInfo> &ports = QSerialPortInfo::availablePorts();

    for ( const QSerialPortInfo &port : ports )
        portNames << port.portName();

    setCompleter(new QCompleter(portNames));
#endif
}

void SerialPortEditLine::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
#if defined(Q_OS_WIN)
    completer()->setCompletionPrefix("COM");
    completer()->complete();
#endif
}
