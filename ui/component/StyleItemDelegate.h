#ifndef QLOG_UI_STYLEITEMDELEGATE_H
#define QLOG_UI_STYLEITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QItemDelegate>
#include <QDate>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QAbstractItemModel>
#include <QDebug>
#include <QTextEdit>
#include <QKeySequenceEdit>
#include <QHBoxLayout>
#include <QPushButton>
#include <QApplication>
#include <QTimeZone>
#include <QLineEdit>
#include <QMessageBox>

#include "core/LogLocale.h"
#include "data/Gridsquare.h"

class CallsignDelegate : public QStyledItemDelegate {
public:
    CallsignDelegate(QObject* parent = 0) :
        QStyledItemDelegate(parent) { }

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const {
        QStyledItemDelegate::initStyleOption(option, index);
        option->font.setBold(true);
    }
};

class DateFormatDelegate : public QStyledItemDelegate {
private:
    LogLocale locale;
public:
    DateFormatDelegate(QObject* parent = 0) :
        QStyledItemDelegate(parent) { }

    QString displayText(const QVariant& value, const QLocale&) const
    {
        return value.toDate().toString(locale.formatDateShortWithYYYY());
    }

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem&,
                          const QModelIndex&) const
    {
        QDateEdit* editor = new QDateEdit(parent);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        editor->setTimeZone(QTimeZone::UTC);
#else
        editor->setTimeSpec(Qt::UTC);
#endif
        editor->setMinimumDate(QDate(1900, 1, 1));
        editor->setSpecialValueText(tr("Blank"));
        editor->setDisplayFormat(locale.formatDateShortWithYYYY());
        return editor;
    }

    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex&) const
    {
        editor->setGeometry(option.rect);
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const
    {
        QDate value = index.model()->data(index, Qt::EditRole).toDate();
        QDateEdit* timeEdit = static_cast<QDateEdit*>(editor);
        timeEdit->setDate(value);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const
    {
        QDateEdit* dateEdit = static_cast<QDateEdit*>(editor);
        dateEdit->interpretText();
        QDateTime value = dateEdit->dateTime();
        model->setData(index,
                       ( dateEdit->date() == dateEdit->minimumDate() ) ? QVariant()
                                                                       : value,
                       Qt::EditRole);
    }
};

class TimestampFormatDelegate : public QStyledItemDelegate {
private:
    LogLocale locale;
public:
    TimestampFormatDelegate(QObject* parent = 0) :
        QStyledItemDelegate(parent) { }

    QString displayText(const QVariant& value, const QLocale&) const
    {
        return locale.toString(value.toDateTime().toTimeZone(QTimeZone::utc()), locale.formatDateShortWithYYYY()
                                                                                + " "
                                                                                + locale.formatTimeLongWithoutTZ());
    }

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem&,
                          const QModelIndex&) const
    {
        QDateTimeEdit* editor = new QDateTimeEdit(parent);

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        editor->setTimeZone(QTimeZone::UTC);
#else
        editor->setTimeSpec(Qt::UTC);
#endif
        editor->setDateTime(QDateTime(QDate(1900, 1, 1), QTime(0, 0, 0)));
        editor->setSpecialValueText(tr("Blank"));
        editor->setDisplayFormat(locale.formatDateShortWithYYYY() + " " + locale.formatTimeLongWithoutTZ());
        return editor;
    }

    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex&) const
    {
        editor->setGeometry(option.rect);
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const
    {
        QDateTime value = index.model()->data(index, Qt::EditRole).toDateTime();
        QDateTimeEdit* timeEdit = static_cast<QDateTimeEdit*>(editor);
        timeEdit->setDateTime(value);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const
    {
        QDateTimeEdit* timeEdit = static_cast<QDateTimeEdit*>(editor);
        timeEdit->interpretText();
        QDateTime value = timeEdit->dateTime();
        model->setData(index,
                       ( timeEdit->date() == timeEdit->minimumDate() ) ? QVariant()
                                                                       : value,
                       Qt::EditRole);
    }
};

class UnitFormatDelegate : public QStyledItemDelegate {
public:
    UnitFormatDelegate(const QString &unit, int precision, double step, QObject* parent = 0) :
        QStyledItemDelegate(parent), unit(unit), precision(precision), step(step) { }

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const {
        QStyledItemDelegate::initStyleOption(option, index);
        option->displayAlignment = Qt::AlignVCenter | Qt::AlignLeft;
    }

    QString displayText(const QVariant& value, const QLocale&) const {
        return QString("%1 %2").arg(QString::number(value.toDouble(), 'f', precision), unit);
    }

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem&,
                          const QModelIndex&) const
    {
        QDoubleSpinBox* editor = new QDoubleSpinBox(parent);
        editor->setDecimals(precision);
        editor->setRange(-1*step, 1e12);
        editor->setSingleStep(step);
        editor->setSpecialValueText("Empty");
        return editor;
    }

    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex&) const
    {
        editor->setGeometry(option.rect);
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const
    {
        double value = index.model()->data(index, Qt::EditRole).toDouble();
        QDoubleSpinBox* spinBox = static_cast<QDoubleSpinBox*>(editor);
        spinBox->setValue(value);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const
    {
        QDoubleSpinBox* spinBox = static_cast<QDoubleSpinBox*>(editor);
        if (spinBox->text() == "Empty" )
        {
            model->setData(index, QVariant() , Qt::EditRole);
            return;
        }
        spinBox->interpretText();
        double value = spinBox->value();
        model->setData(index,value, Qt::EditRole);
    }

private:
    QString unit;
    int precision;
    double step;
};

class DistanceFormatDelegate : public QStyledItemDelegate {
public:
    DistanceFormatDelegate(int precision, double step, QObject* parent = 0) :
        QStyledItemDelegate(parent), precision(precision), step(step) { }

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const {
        QStyledItemDelegate::initStyleOption(option, index);
        option->displayAlignment = Qt::AlignVCenter | Qt::AlignLeft;
    }

    QString displayText(const QVariant& value, const QLocale&) const {
        QString unit;
        double displayValue = Gridsquare::distance2localeUnitDistance(value.toDouble(), unit, locale);
        return QString("%1 %2").arg(QString::number(displayValue, 'f', precision), unit);
    }

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem&,
                          const QModelIndex&) const
    {
        QDoubleSpinBox* editor = new QDoubleSpinBox(parent);
        editor->setDecimals(precision);
        editor->setRange(-1*step, 1e12);
        editor->setSingleStep(step);
        editor->setSpecialValueText("Empty");
        return editor;
    }

    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex&) const
    {
        editor->setGeometry(option.rect);
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const
    {
        double value = index.model()->data(index, Qt::EditRole).toDouble();
        QDoubleSpinBox* spinBox = static_cast<QDoubleSpinBox*>(editor);
        spinBox->setValue(value);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const
    {
        QDoubleSpinBox* spinBox = static_cast<QDoubleSpinBox*>(editor);
        if (spinBox->text() == "Empty" )
        {
            model->setData(index, QVariant() , Qt::EditRole);
            return;
        }
        spinBox->interpretText();
        double value = spinBox->value();
        model->setData(index,value, Qt::EditRole);
    }

private:
    int precision;
    double step;
    LogLocale locale;
};

class ComboFormatDelegate : public QStyledItemDelegate {
public:
    ComboFormatDelegate(QAbstractTableModel* in_model, QObject* parent = nullptr):
        QStyledItemDelegate(parent)
    {
        model = in_model;
    }
    ComboFormatDelegate(QStringList in_list, QObject* parent = nullptr):
        QStyledItemDelegate(parent)
    {
        model = nullptr;
        list = in_list;
    }

    ComboFormatDelegate(QMap<QString, QString> in_map, QObject* parent = nullptr):
        QStyledItemDelegate(parent)
    {
        model = nullptr;
        map = in_map;
    }

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem&,
                          const QModelIndex&) const
    {
        QComboBox *editor = new QComboBox(parent);
        return editor;
    }

    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex&) const
    {
        editor->setGeometry(option.rect);
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const
    {

        QComboBox* combo = static_cast<QComboBox*>(editor);
        combo->clear();

        if ( model )
        {
            combo->setModel(model);
            combo->setCurrentText(index.model()->data(index).toString());
        }
        else if ( ! list.isEmpty() )
        {
            combo->addItems(list);
            combo->setCurrentText(index.model()->data(index).toString());
        }
        else if ( ! map.isEmpty() )
        {
            QMapIterator<QString, QString> iter(map);
            int iter_index = 0;
            int value_index = 0;

            while ( iter.hasNext() )
            {
                iter.next();
                combo->addItem(iter.value(), iter.key());
                if ( iter.key() == index.model()->data(index).toString() )
                {
                    value_index = iter_index;
                }
                iter_index++;
            }
            combo->setCurrentIndex(value_index);
        }
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const
    {
       QComboBox* combo = static_cast<QComboBox*>(editor);
       QString curr_value;

       if ( !map.isEmpty() )
       {
           curr_value = combo->currentData().toString();
       }
       else
       {
           curr_value = combo->currentText();
       }

       if ( curr_value == " " )
       {
           model->setData(index, QVariant(), Qt::EditRole);
       }
       else
       {
           model->setData(index, QVariant(curr_value), Qt::EditRole);
       }
    }

private:
    QAbstractTableModel *model;
    QStringList list;
    QMap<QString, QString> map;
};



class CheckBoxDelegate: public QItemDelegate
{
    Q_OBJECT
public:
    CheckBoxDelegate(QObject *parent = nullptr ) : QItemDelegate(parent), theCheckBox(nullptr) {};

    void paint( QPainter *painter,
                const QStyleOptionViewItem &option,
                const QModelIndex &index ) const
    {
        bool is_enabled = index.model()->data(index, Qt::DisplayRole).toBool();
        if ( !is_enabled) painter->fillRect(option.rect, option.palette.dark());
        drawDisplay(painter,option,option.rect,is_enabled? QString("     ").append(tr("Enabled"))
                                                         : QString("     ").append(tr("Disabled")));
        drawFocus(painter,option,option.rect);

    };

    QWidget *createEditor( QWidget *parent,
                        const QStyleOptionViewItem &option,
                        const QModelIndex &index ) const
    {
        (void)option;
        (void)index;

        theCheckBox = new QCheckBox( parent );
        QObject::connect(theCheckBox,&QCheckBox::toggled,this,&CheckBoxDelegate::setData);
        return theCheckBox;
    };

    void setEditorData( QWidget *editor,
                        const QModelIndex &index ) const
    {
        bool val = index.model()->data( index, Qt::DisplayRole ).toBool();

        (static_cast<QCheckBox*>( editor ))->setChecked(val);

    }

    void setModelData( QWidget *editor,
                        QAbstractItemModel *model,
                        const QModelIndex &index ) const
    {
        model->setData( index, (bool)(static_cast<QCheckBox*>( editor )->isChecked() ) );
    }


    void updateEditorGeometry( QWidget *editor,
                        const QStyleOptionViewItem &option,
                        const QModelIndex &index ) const
    {
        (void)index;
        editor->setGeometry( option.rect );
    }

    mutable QCheckBox * theCheckBox;

private slots:

    void setData(bool val)
    {
        (void)val;
        emit commitData(theCheckBox);
    }
};

class TextBoxDelegate: public QItemDelegate
{
    Q_OBJECT
public:
    TextBoxDelegate(QObject *parent = 0 ) :QItemDelegate(parent), theText(nullptr){};

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem&,
                          const QModelIndex&) const
    {
        theText = new QTextEdit(parent);

        return theText;
    }

    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex&) const
    {
        editor->setGeometry(option.rect.x(),option.rect.y(),editor->sizeHint().width(),editor->sizeHint().height());
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const
    {
        QString value = index.model()->data(index, Qt::EditRole).toString();
        QTextEdit* textEditor = static_cast<QTextEdit*>(editor);
        textEditor->setPlainText(value);
        textEditor->setAcceptRichText(false);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const
    {
        QTextEdit* textEditor = static_cast<QTextEdit*>(editor);
        QString value = textEditor->toPlainText();
        model->setData(index,value, Qt::EditRole);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        QString line = index.model()->data(index, Qt::EditRole).toString().simplified();

        drawDisplay(painter,option,option.rect, line);
        drawFocus(painter, option, option.rect);
    }

    mutable QTextEdit *theText;

private slots:

    void setData(bool val)
    {
        (void)val;
        emit commitData(theText);
    }
};

class KeySequenceEdit : public QWidget
{
    Q_OBJECT

public:
    KeySequenceEdit(QWidget* parent = nullptr) : QWidget(parent)
    {

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        QPushButton* clearButton = new QPushButton(QLatin1String(), this);
        QIcon clearIcon = QApplication::style()->standardIcon(QStyle::SP_LineEditClearButton);
        clearButton->setIcon(clearIcon);
        clearButton->setToolTip(tr("Clear"));

        keySequenceEdit = new QKeySequenceEdit(this);
        layout->addWidget(keySequenceEdit);
        layout->addWidget(clearButton);
        connect(clearButton, &QPushButton::clicked, this, [=]()
        {
            keySequenceEdit->clear();
            emit editingFinished();
        });
        connect(keySequenceEdit, &QKeySequenceEdit::editingFinished, this, [=]()
        {
            emit editingFinished();
        });

        setFocusProxy(keySequenceEdit);
    }

    void setKeySequence(const QKeySequence &keySequence)
    {
        keySequenceEdit->setKeySequence(keySequence);
    }

    QKeySequence keySequence() const
    {
        return keySequenceEdit->keySequence();
    }

signals:
    void editingFinished();

private:
     QKeySequenceEdit* keySequenceEdit;
};

class ShortcutDelegate : public QStyledItemDelegate
{
     Q_OBJECT

public:
    ShortcutDelegate(QObject* parent = nullptr) :
        QStyledItemDelegate(parent) { }

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem&,
                          const QModelIndex&) const
    {
        KeySequenceEdit *editor = new KeySequenceEdit(parent);
        connect(editor, &KeySequenceEdit::editingFinished, this,
                &ShortcutDelegate::commitAndCloseEditor);
        return editor;
    }

    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex&) const
    {
        editor->setGeometry(option.rect);
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const
    {
        KeySequenceEdit *keySequenceEdit = static_cast<KeySequenceEdit *>(editor);
        if ( keySequenceEdit )
            keySequenceEdit->setKeySequence(index.model()->data(index, Qt::EditRole).toString());
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const
    {
        const KeySequenceEdit *keySequenceEdit = static_cast<KeySequenceEdit *>(editor);
        if ( ! keySequenceEdit )
            return;

        model->setData(index, keySequenceEdit->keySequence().toString(QKeySequence::NativeText));
    }

private slots:
    void commitAndCloseEditor()
    {
        KeySequenceEdit *editor = static_cast<KeySequenceEdit *>(sender());
        emit commitData(editor);
        emit closeEditor(editor);
    }
};

class ReadOnlyDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
            return nullptr;
    }
};

class UpperCaseDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        QLineEdit *editor = new QLineEdit(parent);
        return editor;
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
        {
            QString text = lineEdit->text().toUpper();
            model->setData(index, text);
        }
    }
};

class UpperCaseUniqueDelegate : public UpperCaseDelegate {
    Q_OBJECT
public:
    using UpperCaseDelegate::UpperCaseDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        QLineEdit *editor = new QLineEdit(parent);
        return editor;
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
        {
            QString text = lineEdit->text().toUpper();
            for (int row = 0; row < model->rowCount(); ++row)
            {
                if (row == index.row()) continue;
                if (model->data(model->index(row, index.column()), Qt::EditRole).toString() == text)
                {
                    QMessageBox::warning(nullptr, "Duplicate value", "The value must be unique.");
                    return;
                }
            }
            model->setData(index, text);
        }
    }
};

class PasswordDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        QLineEdit *editor = new QLineEdit(parent);
        editor->setEchoMode(QLineEdit::Password);
        return editor;
    }

    QString displayText(const QVariant &value, const QLocale &) const override
    {
        QLineEdit dummy;
        dummy.setEchoMode(QLineEdit::Password);
        dummy.setText(value.toString());
        return dummy.displayText();
    }
};

#endif // QLOG_UI_STYLEITEMDELEGATE_H
