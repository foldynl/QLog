#include <QStringListModel>
#include <QMessageBox>
#include <QDateTimeEdit>
#include <QStackedWidget>
#include "QSOFilterDetail.h"
#include "ui_QSOFilterDetail.h"
#include "core/debug.h"
#include "data/Data.h"
#include "core/QSOFilterManager.h"

MODULE_IDENTIFICATION("qlog.ui.qsofilterdetail");

QSOFilterDetail::QSOFilterDetail(const QString &filterName, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSOFilterDetail),
    filterName(filterName),
    condCount(0)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    if ( ! filterName.isEmpty() )
        loadFilter(filterName);
    else
    {
        /* get Filters name from DB to checking whether a new filter name
         * will be unique */
        filterNamesList = QSOFilterManager::instance()->getFilterList();
    }
}

QSOFilterDetail::~QSOFilterDetail()
{
    FCT_IDENTIFICATION;
    delete ui;
}

void QSOFilterDetail::addCondition(int fieldIdx, int operatorId, QString value)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "FieldIDX: " << fieldIdx << " Operator: " << operatorId << " Value: " << value;

    QHBoxLayout* conditionLayout = new QHBoxLayout();
    conditionLayout->setObjectName(QString::fromUtf8("conditionLayout%1").arg(condCount));

    /***************/
    /* Field Combo */
    /***************/
    QComboBox* fieldNameCombo = new QComboBox(this);
    fieldNameCombo->setObjectName(QString::fromUtf8("fieldNameCombo%1").arg(condCount));
    QSizePolicy sizePolicy1(QSizePolicy::Maximum, QSizePolicy::Fixed);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(fieldNameCombo->sizePolicy().hasHeightForWidth());
    fieldNameCombo->setSizePolicy(sizePolicy1);

    QList<QPair<LogbookModel::ColumnID, QString>> items;

    for ( int i = LogbookModel::ColumnID::COLUMN_ID; i < LogbookModel::ColumnID::COLUMN_LAST_ELEMENT; ++i )
    {
        LogbookModel::ColumnID columnID = static_cast<LogbookModel::ColumnID>(i);
        items.append({columnID, LogbookModel::getFieldNameTranslation(columnID)});
    }

    std::sort(items.begin(), items.end(), [](const QPair<LogbookModel::ColumnID, QString>& a,
                                             const QPair<LogbookModel::ColumnID, QString>& b)
    {
        return a.second.localeAwareCompare(b.second) < 0;
    });

    for (const auto& item : items)
        fieldNameCombo->addItem(item.second, item.first);

    /* Do not set combo value here because we will connect signal Change later */
    conditionLayout->addWidget(fieldNameCombo);

    /*******************/
    /* Condition Combo */
    /*******************/
    QComboBox* conditionCombo = new QComboBox(this);
    conditionCombo->addItem(QString(tr("Equal")));
    conditionCombo->addItem(QString(tr("Not Equal")));
    conditionCombo->addItem(QString(tr("Contains")));
    conditionCombo->addItem(QString(tr("Not Contains")));
    conditionCombo->addItem(QString(tr("Greater Than")));
    conditionCombo->addItem(QString(tr("Less Than")));
    conditionCombo->addItem(QString(tr("Starts with")));
    conditionCombo->setObjectName(QString::fromUtf8("conditionCombo%1").arg(condCount));

    if ( operatorId >= 0 )
        conditionCombo->setCurrentIndex(operatorId);

    conditionLayout->addWidget(conditionCombo);

    /**************/
    /* Value Edit */
    /**************/

    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);

    // use stack to change Line and Date Edit - it will depend on column from combo selection
    QStackedWidget* stacked = new QStackedWidget(this);
    stacked->setObjectName(QString::fromUtf8("stackedValueEdit%1").arg(condCount));
    stacked->setMaximumSize(QSize(16777215, 30));
    stacked->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    stacked->setSizePolicy(sizePolicy);

    stacked->addWidget(createLineEdit(value, condCount, sizePolicy));
    stacked->addWidget(createDateEdit(value, condCount, sizePolicy));
    stacked->addWidget(createDateTimeEdit(value, condCount, sizePolicy));
    stacked->addWidget(createComboBox(Data::instance()->qslSentEnum, value, condCount, sizePolicy));
    stacked->addWidget(createComboBox(Data::instance()->qslSentViaEnum, value, condCount, sizePolicy));
    stacked->addWidget(createComboBox(Data::instance()->qslRcvdEnum, value, condCount, sizePolicy));
    stacked->addWidget(createComboBox(Data::instance()->uploadStatusEnum, value, condCount, sizePolicy));
    stacked->addWidget(createComboBox(Data::instance()->antPathEnum, value, condCount, sizePolicy));
    stacked->addWidget(createComboBox(Data::instance()->boolEnum, value, condCount, sizePolicy));
    stacked->addWidget(createComboBox(Data::instance()->qsoCompleteEnum, value, condCount, sizePolicy));
    stacked->addWidget(createComboBox(Data::instance()->downloadStatusEnum, value, condCount, sizePolicy));
    stacked->addWidget(createComboBox(Data::instance()->morseKeyTypeEnum, value, condCount, sizePolicy));

    conditionLayout->addWidget(stacked);

    // connect field combo and stacked widged to switch correct Edit Widget
    connect(fieldNameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, stacked, value, fieldNameCombo](int)
    {
        /* Index is mapped the same way as LogbookModel columns
           Therefore, we can use Column aliases here
         */
        int fieldIndex = fieldNameCombo->currentData().toInt();

        if ( this->isDateField(fieldIndex) )
            stacked->setCurrentIndex(1); //Date Edit
        else if ( this->isDateTimeField(fieldIndex) )
            stacked->setCurrentIndex(2); //DateTime edit
        else if ( this->isQSLSentField(fieldIndex) )
            stacked->setCurrentIndex(3);
        else if ( this->isQSLSentViaField(fieldIndex) )
            stacked->setCurrentIndex(4);
        else if ( this->isQSLRcvdField(fieldIndex) )
            stacked->setCurrentIndex(5);
        else if ( this->isUploadStatusField(fieldIndex) )
            stacked->setCurrentIndex(6);
        else if ( this->isAntPathField(fieldIndex) )
            stacked->setCurrentIndex(7);
        else if ( this->isBoolField(fieldIndex) )
            stacked->setCurrentIndex(8);
        else if ( this->isQSOCompleteField(fieldIndex) )
            stacked->setCurrentIndex(9);
        else if ( this->isDownloadStatusField(fieldIndex))
            stacked->setCurrentIndex(10);
        else if ( this->isMorseKeyTypeField(fieldIndex))
            stacked->setCurrentIndex(11);
        else
            stacked->setCurrentIndex(0);
    });

    /* Set FieldNameCombo here to update Stacked Widget */
    if ( fieldIdx >= 0 )
    {
        int index = fieldNameCombo->findData(fieldIdx);
        if (index != -1)
            fieldNameCombo->setCurrentIndex(index);
    }

    /*****************/
    /* Remove Button */
    /*****************/
    QPushButton* removeButton = new QPushButton(tr("Remove"), this);
    removeButton->setObjectName(QString::fromUtf8("removeButton%1").arg(condCount));

    conditionLayout->addWidget(removeButton);

    connect(removeButton, &QPushButton::clicked, this, [conditionLayout]()
    {
        QLayoutItem *item = NULL;
        while ((item = conditionLayout->takeAt(0)) != 0)
        {
            delete item->widget();
            delete item;
        }
        conditionLayout->deleteLater();
    });

    /**************************/
    /* Add to the main layout */
    /**************************/
    ui->conditionsLayout->addLayout(conditionLayout);

    condCount++;
}

void QSOFilterDetail::loadFilter(const QString &filterName)
{
    FCT_IDENTIFICATION;

    ui->filterLineEdit->setText(filterName);
    ui->filterLineEdit->setEnabled(false);

    const QSOFilter &filter = QSOFilterManager::instance()->getFilter(filterName);

    if ( filter.filterName == filterName )
    {
        ui->matchingCombo->setCurrentIndex(filter.machingType);

        for ( const QSOFilterRule &rule : filter.rules )
            addCondition(rule.tableFieldIndex, rule.operatorID, rule.value);
    }
}

bool QSOFilterDetail::filterExists(const QString &filterName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filterName;

    return filterNamesList.contains(filterName);
}

bool QSOFilterDetail::isDateField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = (    index == LogbookModel::COLUMN_QSL_RCVD_DATE
                 || index == LogbookModel::COLUMN_QSL_SENT_DATE
                 || index == LogbookModel::COLUMN_LOTW_RCVD_DATE
                 || index == LogbookModel::COLUMN_LOTW_SENT_DATE
                 || index == LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_DATE
                 || index == LogbookModel::COLUMN_EQSL_QSLRDATE
                 || index == LogbookModel::COLUMN_EQSL_QSLSDATE
                 || index == LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_DATE
                 || index == LogbookModel::COLUMN_HAMLOGEU_QSO_UPLOAD_DATE
                 || index == LogbookModel::COLUMN_HAMQTH_QSO_UPLOAD_DATE
                 || index == LogbookModel::COLUMN_DCL_QSLRDATE
                 || index == LogbookModel::COLUMN_DCL_QSLSDATE
                 || index == LogbookModel::COLUMN_QRZCOM_QSO_DOWNLOAD_DATE);

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isDateTimeField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = (    index == LogbookModel::COLUMN_TIME_ON
                 || index == LogbookModel::COLUMN_TIME_OFF );

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isQSLSentField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = (    index == LogbookModel::COLUMN_QSL_SENT
                 || index == LogbookModel::COLUMN_LOTW_SENT
                 || index == LogbookModel::COLUMN_EQSL_QSL_SENT
                 || index == LogbookModel::COLUMN_DCL_QSL_SENT);

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isQSLSentViaField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = (    index == LogbookModel::COLUMN_QSL_SENT_VIA
                 || index == LogbookModel::COLUMN_QSL_RCVD_VIA );

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isQSLRcvdField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = (    index == LogbookModel::COLUMN_QSL_RCVD
                 || index == LogbookModel::COLUMN_LOTW_RCVD
                 || index == LogbookModel::COLUMN_EQSL_QSL_RCVD
                 || index == LogbookModel::COLUMN_DCL_QSL_RCVD);

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isUploadStatusField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = (    index == LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_STATUS
                 || index == LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_STATUS
                 || index == LogbookModel::COLUMN_QRZCOM_QSO_UPLOAD_STATUS
                 || index == LogbookModel::COLUMN_HAMLOGEU_QSO_UPLOAD_STATUS
                 || index == LogbookModel::COLUMN_HAMQTH_QSO_UPLOAD_STATUS);

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isAntPathField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = (    index == LogbookModel::COLUMN_ANT_PATH );

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isBoolField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = (    index == LogbookModel::COLUMN_FORCE_INIT
                 || index == LogbookModel::COLUMN_QSO_RANDOM
                 || index == LogbookModel::COLUMN_SILENT_KEY
                 || index == LogbookModel::COLUMN_SWL);

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isQSOCompleteField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = ( index == LogbookModel::COLUMN_QSO_COMPLETE );

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isDownloadStatusField(int index)
{
    FCT_IDENTIFICATION;

    bool ret = ( index == LogbookModel::COLUMN_QRZCOM_QSO_DOWNLOAD_STATUS );

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

bool QSOFilterDetail::isMorseKeyTypeField(int index)
{
    bool ret = ( index == LogbookModel::COLUMN_MORSE_KEY_TYPE
                 || index == LogbookModel::COLUMN_MY_MORSE_KEY_TYPE );

    qCDebug(function_parameters) << index << " return " << ret;
    return ret;
}

QComboBox* QSOFilterDetail::createComboBox(const QMap<QString, QString> &mapping,
                                           const QString &value, const int identifier,
                                           const QSizePolicy &sizepolicy)
{
    FCT_IDENTIFICATION;

    QComboBox* combo = new QComboBox();
    combo->setObjectName(QString::fromUtf8("valueCombo%1").arg(identifier));
    combo->setFocusPolicy(Qt::ClickFocus);

    QMapIterator<QString, QString> iter(mapping);
    int iter_index = 0;
    int value_index = 0;
    while ( iter.hasNext() )
    {
        iter.next();
        combo->addItem(iter.value(), iter.key());
        if ( ! value.isEmpty() && iter.key() == value )
            value_index = iter_index;
        iter_index++;
    }
    combo->setCurrentIndex(value_index);
    combo->setSizePolicy(sizepolicy);

    return combo;
}

QDateEdit *QSOFilterDetail::createDateEdit(const QString &value, const int identified,
                                           const QSizePolicy &sizepolicy)
{
    FCT_IDENTIFICATION;


    QDateEdit* valueDate = new QDateEdit();
    valueDate->setObjectName(QString::fromUtf8("valueDateEdit%1").arg(identified));
    valueDate->setFocusPolicy(Qt::ClickFocus);
    valueDate->setCalendarPopup(true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    valueDate->setTimeZone(QTimeZone::UTC);
#else
    valueDate->setTimeSpec(Qt::UTC);
#endif
    valueDate->setDisplayFormat(locale.formatDateShortWithYYYY());
    valueDate->setSizePolicy(sizepolicy);
    if ( !value.isEmpty() )
        valueDate->setDate(QDate::fromString(value, "yyyy-MM-dd"));
    return valueDate;
}

QDateTimeEdit *QSOFilterDetail::createDateTimeEdit(const QString &value, const int identified,
                                                   const QSizePolicy &sizepolicy)
{
    FCT_IDENTIFICATION;

    QDateTimeEdit* valueDateTime = new QDateTimeEdit();
    valueDateTime->setObjectName(QString::fromUtf8("valueDateTimeEdit%1").arg(identified));
    valueDateTime->setFocusPolicy(Qt::ClickFocus);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    valueDateTime->setTimeZone(QTimeZone::UTC);
#else
    valueDateTime->setTimeSpec(Qt::UTC);
#endif
    valueDateTime->setDisplayFormat(locale.formatDateShortWithYYYY()
                                    + " " + locale.formatTimeLongWithoutTZ());
    valueDateTime->setSizePolicy(sizepolicy);
    if ( !value.isEmpty() )
    {
        QDateTime dtValue = QDateTime::fromString(value, "yyyy-MM-ddTHH:mm:ss");
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        dtValue.setTimeZone(QTimeZone::UTC);
#else
        dtValue.setTimeSpec(Qt::UTC);
#endif
        valueDateTime->setDateTime(dtValue);
    }
    return valueDateTime;

}

QLineEdit *QSOFilterDetail::createLineEdit(const QString &value, const int identified,
                                           const QSizePolicy &sizepolicy)
{
    FCT_IDENTIFICATION;

    QLineEdit* valueEdit = new QLineEdit();
    valueEdit->setObjectName(QString::fromUtf8("valueLineEdit%1").arg(identified));
    valueEdit->setSizePolicy(sizepolicy);
    valueEdit->setText(value);
    return valueEdit;
}

void QSOFilterDetail::save()
{
    FCT_IDENTIFICATION;

    if ( ui->filterLineEdit->text().isEmpty() )
    {
        ui->filterLineEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( filterExists(ui->filterLineEdit->text()) )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Info"),
                              QMessageBox::tr("Filter name is already exists."));
        return;
    }

    const QList<QHBoxLayout *> &conditionLayouts = ui->conditionsLayout->findChildren<QHBoxLayout *>();

    QSOFilter filter;

    filter.filterName = ui->filterLineEdit->text();
    filter.machingType = ui->matchingCombo->currentIndex();

    for ( auto &condition: conditionLayouts )
    {
        QSOFilterRule rule;

        for ( int i = 0; i < 3; i++ )
        {

            QString objectName = condition->itemAt(i)->widget()->objectName();

            if ( objectName.contains("fieldNameCombo") )
                rule.tableFieldIndex = dynamic_cast<QComboBox*>(condition->itemAt(i)->widget())->currentData().toInt();
            else if ( objectName.contains("conditionCombo") )
                rule.operatorID = dynamic_cast<QComboBox*>(condition->itemAt(i)->widget())->currentIndex();
            else if ( objectName.contains("stackedValueEdit") )
            {
                QStackedWidget* editStack = dynamic_cast<QStackedWidget*>(condition->itemAt(i)->widget());

                QWidget* stackedEdit = editStack->currentWidget();

                if ( stackedEdit )
                {
                    QString stacketEditObjName = stackedEdit->objectName();

                    if ( stacketEditObjName.contains("valueLineEdit") )
                    {
                        QLineEdit* editLine = dynamic_cast<QLineEdit*>(stackedEdit);
                        rule.value = editLine->text();
                    }
                    else if ( stacketEditObjName.contains("valueDateEdit") )
                    {
                        QDateEdit* dateTimeEdit = dynamic_cast<QDateEdit*>(stackedEdit);
                        rule.value = dateTimeEdit->date().toString(Qt::ISODate);
                    }
                    else if ( stacketEditObjName.contains("valueDateTimeEdit") )
                    {
                        QDateTimeEdit* dateEdit = dynamic_cast<QDateTimeEdit*>(stackedEdit);
                        rule.value = dateEdit->dateTime().toString("yyyy-MM-ddTHH:mm:ss");
                    }
                    else if ( stacketEditObjName.contains("valueCombo") )
                    {
                        QComboBox* combo = dynamic_cast<QComboBox*>(stackedEdit);
                        rule.value = combo->currentData().toString();
                        if ( rule.value == " ") // empty value
                            rule.value = QString();
                    }
                }
                else
                    qCritical(runtime) << "Unexpected empty Stack - null pointer";
            }
            else
                qWarning() << "Unknown object name"  << objectName;
        }

        filter.addRule(rule);
    }

    if ( !QSOFilterManager::instance()->save(filter) )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("Cannot update QSO Filter Conditions"));
        return;
    }

    accept();
}

void QSOFilterDetail::filterNameChanged(const QString &newFilterName)
{
    FCT_IDENTIFICATION;

    QPalette p;
    p.setColor(QPalette::Text, (filterExists(newFilterName)) ? Qt::red
                                                             : qApp->palette().text().color());
    ui->filterLineEdit->setPalette(p);
}
