#include <QFocusEvent>
#include <QCompleter>
#include <QSerialPortInfo>
#include <QDir>
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
    focusInSelectionOffset(0)
{
    setInputMask("xxxxxxx;");
}

void NewContactRSTEditLine::setSelectionOffset(int offset)
{
    focusInSelectionOffset = offset;
}

void NewContactRSTEditLine::setMaxLength(int len)
{
    NewContactEditLine::setMaxLength(len);
    setInputMask(QString(len, 'x') + ';');
}

void NewContactRSTEditLine::focusInEvent(QFocusEvent *event)
{
    NewContactEditLine::focusInEvent(event);

    int position = 0;

    if ( event->reason() != Qt::PopupFocusReason && !text().isEmpty() && text().length() >= focusInSelectionOffset )
        position = focusInSelectionOffset;

    setCursorPosition(position);
}

SerialPortEditLine::SerialPortEditLine(QWidget *parent) :
    QLineEdit(parent)
{
    const QList<QSerialPortInfo> &ports = QSerialPortInfo::availablePorts();
    QStringList portNames;

#if defined(Q_OS_WIN)
    for (const QSerialPortInfo &port : ports)
        portNames << port.portName();
#elif defined(Q_OS_MACOS)
    for (const QSerialPortInfo &port : ports)
        portNames << QString("/dev/%1").arg(port.portName());
#else
    // In the case of Linux, it is good to use /dev/serial/by-id
    // because it does not change over time.
    // obtain /dev/serial/by-id files
    QDir dir("/dev/serial/by-id");
    const QStringList &symlinks = dir.entryList(QDir::System | QDir::Readable | QDir::NoDotAndDotDot);

    for ( const QSerialPortInfo &port : ports )
    {
        QString dev = QString("/dev/%1").arg(port.portName());
        QString niceName = dev;

        // try to find symlink
        for ( const QString &entry : symlinks )
        {
            QString fullPath = dir.absoluteFilePath(entry);
            QFileInfo fi(fullPath);
            if ( fi.canonicalFilePath() == dev )
            {
                niceName = fullPath;
                break;
            }
        }
        portNames << niceName;
    }
#endif
    setCompleter(new QCompleter(portNames));
}

void SerialPortEditLine::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
#if defined(Q_OS_WIN)
    completer()->setCompletionPrefix("COM");
    completer()->complete();
#else
    completer()->setCompletionPrefix("/dev/");
    completer()->complete();
#endif
}
