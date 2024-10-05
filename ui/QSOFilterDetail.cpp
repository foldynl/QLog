#include <QStringListModel>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QMessageBox>
#include <QDateTimeEdit>
#include <QStackedWidget>
#include "QSOFilterDetail.h"
#include "ui_QSOFilterDetail.h"
#include "core/debug.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.ui.qsofilterdetail");

QSOFilterDetail::QSOFilterDetail(const QString &filterName, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSOFilterDetail),
    filterName(filterName),
    condCount(0)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    logbookmodel = new LogbookModel(this);

    if ( ! filterName.isEmpty() )
    {
        loadFilter(filterName);
    }
    else
    {
        /* get Filters name from DB to checking whether a new filter name
         * will be unique */
        QSqlQuery filterStmt;
        if ( ! filterStmt.prepare("SELECT filter_name FROM qso_filters ORDER BY filter_name") )
        {
            qWarning() << "Cannot prepare select statement";
        }
        else
        {
            if ( filterStmt.exec() )
            {
                while (filterStmt.next())
                {
                    filterNamesList << filterStmt.value(0).toString();
                }
            }
            else
            {
                qInfo()<< "Cannot get filters names from DB" << filterStmt.lastError();;
            }
        }
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
    QComboBox* fieldNameCombo = new QComboBox();
    fieldNameCombo->setObjectName(QString::fromUtf8("fieldNameCombo%1").arg(condCount));
    QSizePolicy sizePolicy1(QSizePolicy::Maximum, QSizePolicy::Fixed);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(fieldNameCombo->sizePolicy().hasHeightForWidth());
    fieldNameCombo->setSizePolicy(sizePolicy1);

    int columnIndex = 0;

    QStringList columnsNames;
    while ( columnIndex < logbookmodel->columnCount() )
    {
        columnsNames << logbookmodel->headerData(columnIndex, Qt::Horizontal).toString();
        columnIndex++;
    }

    QStringListModel* columnsNameModel = new QStringListModel(columnsNames,this);
    fieldNameCombo->setModel(columnsNameModel);

    /* Do not set combo value here because we will connect signal Change later */
    conditionLayout->addWidget(fieldNameCombo);

    /*******************/
    /* Condition Combo */
    /*******************/
    QComboBox* conditionCombo = new QComboBox();
    conditionCombo->addItem(QString(tr("Equal")));
    conditionCombo->addItem(QString(tr("Not Equal")));
    conditionCombo->addItem(QString(tr("Contains")));
    conditionCombo->addItem(QString(tr("Not Contains")));
    conditionCombo->addItem(QString(tr("Greater Than")));
    conditionCombo->addItem(QString(tr("Less Than")));
    conditionCombo->addItem(QString(tr("Starts with")));
    conditionCombo->setObjectName(QString::fromUtf8("conditionCombo%1").arg(condCount));

    if ( operatorId >= 0 )
    {
        conditionCombo->setCurrentIndex(operatorId);
    }

    conditionLayout->addWidget(conditionCombo);

    /**************/
    /* Value Edit */
    /**************/

    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);

    // use stack to change Line and Date Edit - it will depend on column from combo selection
    QStackedWidget* stacked = new QStackedWidget();
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

    conditionLayout->addWidget(stacked);

    // connect field combo and stacked widged to switch correct Edit Widget
    connect(fieldNameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, stacked, value](int index)
    {
        /* Index is mapped the same way as LogbookModel columns
           Therefore, we can use Column aliases here
         */
        if ( this->isDateField(index) )
        {
            stacked->setCurrentIndex(1); //Date Edit
        }
        else if ( this->isDateTimeField(index) )
        {
            stacked->setCurrentIndex(2); //DateTime edit
        }
        else if ( this->isQSLSentField(index) )
        {
            stacked->setCurrentIndex(3);
        }
        else if ( this->isQSLSentViaField(index) )
        {
            stacked->setCurrentIndex(4);
        }
        else if ( this->isQSLRcvdField(index) )
        {
            stacked->setCurrentIndex(5);
        }
        else if ( this->isUploadStatusField(index) )
        {
            stacked->setCurrentIndex(6);
        }
        else if ( this->isAntPathField(index) )
        {
            stacked->setCurrentIndex(7);
        }
        else if ( this->isBoolField(index) )
        {
            stacked->setCurrentIndex(8);
        }
        else if ( this->isQSOCompleteField(index) )
        {
            stacked->setCurrentIndex(9);
        }
        else
        {
            stacked->setCurrentIndex(0);
        }
    });

    /* Set FieldNameCombo here to update Stacked Widget */
    if ( fieldIdx >= 0 )
    {
        fieldNameCombo->setCurrentIndex(fieldIdx);
    }

    /*****************/
    /* Remove Button */
    /*****************/
    QPushButton* removeButton = new QPushButton(tr("Remove"));
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

    QSqlQuery query;
    if ( ! query.prepare("SELECT matching_type, table_field_index, operator_id, value "
                  "FROM qso_filter_rules r, qso_filters f "
                  "WHERE f.filter_name = :filter AND f.filter_name = r.filter_name") )
    {
        qWarning() << "Cannot prepare select statement";
        return;
    }

    query.bindValue(":filter", filterName);

    if ( query.exec() )
    {
        while ( query.next() )
        {
            QSqlRecord record = query.record();

            ui->matchingCombo->setCurrentIndex(record.value("matching_type").toInt());
            addCondition(record.value("table_field_index").toInt(),
                         record.value("operator_id").toInt(),
                         record.value("value").toString());
        }
    }
    else
    {
        qCDebug(runtime) << "SQL execution error: " << query.lastError().text();
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
                 || index == LogbookModel::COLUMN_HAMQTH_QSO_UPLOAD_DATE);

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
                 || index == LogbookModel::COLUMN_EQSL_QSL_SENT );

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
                 || index == LogbookModel::COLUMN_EQSL_QSL_RCVD);

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
        {
            value_index = iter_index;
        }
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
    valueDate->setTimeSpec(Qt::UTC);
    valueDate->setDisplayFormat(locale.formatDateShortWithYYYY());
    valueDate->setSizePolicy(sizepolicy);
    if ( !value.isEmpty() )
    {
        valueDate->setDate(QDate::fromString(value, "yyyy-MM-dd"));
    }
    return valueDate;
}

QDateTimeEdit *QSOFilterDetail::createDateTimeEdit(const QString &value, const int identified,
                                                   const QSizePolicy &sizepolicy)
{
    FCT_IDENTIFICATION;

    QDateTimeEdit* valueDateTime = new QDateTimeEdit();
    valueDateTime->setObjectName(QString::fromUtf8("valueDateTimeEdit%1").arg(identified));
    valueDateTime->setFocusPolicy(Qt::ClickFocus);
    valueDateTime->setCalendarPopup(true);
    valueDateTime->setTimeSpec(Qt::UTC);
    valueDateTime->setDisplayFormat(locale.formatDateShortWithYYYY()
                                    + " " + locale.formatTimeLongWithoutTZ());
    valueDateTime->setSizePolicy(sizepolicy);
    valueDateTime->setDateTime(QDateTime::fromString(value, "yyyy-MM-ddTHH:mm:ss"));
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

    QString valueString;
    QSqlQuery filterInsertStmt, deleteFilterStmt, updateStmt;

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

    if ( ! filterInsertStmt.prepare("INSERT INTO qso_filter_rules(filter_name, table_field_index, operator_id, value) "
                              "VALUES (:filterName, :tableFieldIndex, :operatorID, :valueString)") )
    {
        qWarning() << "cannot preapre insert statement";
        return;
    }

    if ( ! updateStmt.prepare("INSERT INTO qso_filters (filter_name, matching_type) VALUES (:filterName, :matchingType) "
                       "ON CONFLICT(filter_name) DO UPDATE SET matching_type = :matchingType WHERE filter_name = :filterName") )
    {
        qWarning() << "Cannot prepare insert statement";
        return;
    }

    updateStmt.bindValue(":matchingType", ui->matchingCombo->currentIndex());
    updateStmt.bindValue(":filterName", ui->filterLineEdit->text());

    if ( ! deleteFilterStmt.prepare("DELETE FROM qso_filter_rules WHERE filter_name = :filterName") )
    {
        qWarning() << "Cannot prepare delete statement";
        return;
    }
    deleteFilterStmt.bindValue(":filterName", ui->filterLineEdit->text());

    QSqlDatabase::database().transaction();

    if ( updateStmt.exec() )
    {
        if ( deleteFilterStmt.exec() )
        {
            const QList<QHBoxLayout *> &conditionLayouts = ui->conditionsLayout->findChildren<QHBoxLayout *>();

            for (auto &condition: conditionLayouts )
            {
                int fieldNameIdx = 0;
                int conditionIdx = 0;

                for (int i = 0; i < 3; i++)
                {

                    QString objectName = condition->itemAt(i)->widget()->objectName();

                    if ( objectName.contains("fieldNameCom") )
                    {
                        fieldNameIdx = dynamic_cast<QComboBox*>(condition->itemAt(i)->widget())->currentIndex();
                    }

                    if ( objectName.contains("conditionCombo") )
                    {
                        conditionIdx = dynamic_cast<QComboBox*>(condition->itemAt(i)->widget())->currentIndex();
                    }

                    if ( objectName.contains("stackedValueEdit") )
                    {
                        QStackedWidget* editStack = dynamic_cast<QStackedWidget*>(condition->itemAt(i)->widget());

                        QWidget* stackedEdit = editStack->currentWidget();

                        if ( stackedEdit )
                        {
                            QString stacketEditObjName = stackedEdit->objectName();

                            if ( stacketEditObjName.contains("valueLineEdit") )
                            {
                                QLineEdit* editLine = dynamic_cast<QLineEdit*>(stackedEdit);
                                valueString = editLine->text();
                            }
                            else if ( stacketEditObjName.contains("valueDateEdit") )
                            {
                                QDateEdit* dateTimeEdit = dynamic_cast<QDateEdit*>(stackedEdit);
                                valueString = dateTimeEdit->date().toString(Qt::ISODate);
                            }
                            else if ( stacketEditObjName.contains("valueDateTimeEdit") )
                            {
                                QDateTimeEdit* dateEdit = dynamic_cast<QDateTimeEdit*>(stackedEdit);
                                valueString = dateEdit->dateTime().toString("yyyy-MM-ddTHH:mm:ss");
                            }
                            else if ( stacketEditObjName.contains("valueCombo") )
                            {
                                QComboBox* combo = dynamic_cast<QComboBox*>(stackedEdit);
                                valueString = combo->currentData().toString();
                                if ( valueString == " ") // empty value
                                {
                                    valueString = QString();
                                }
                            }
                        }
                        else
                        {
                            qCCritical(runtime) << "Unexpected empty Stack - null pointer";
                        }
                    }
                }

                qCDebug(runtime)<< "Condition Values: " << ui->filterLineEdit->text() << " " << fieldNameIdx << " " << conditionIdx << " " << valueString;
                filterInsertStmt.bindValue(":filterName", ui->filterLineEdit->text());
                filterInsertStmt.bindValue(":tableFieldIndex", fieldNameIdx);
                filterInsertStmt.bindValue(":operatorID", conditionIdx);
                if ( valueString.isEmpty() )
                {
                    filterInsertStmt.bindValue(":valueString", QVariant());
                }
                else
                {
                    filterInsertStmt.bindValue(":valueString", valueString);
                }

                if ( ! filterInsertStmt.exec() )
                {
                    QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                                          QMessageBox::tr("Cannot update QSO Filter Conditions - ") + filterInsertStmt.lastError().text());
                    qInfo()<< "Cannot update QSO Filter Conditions - " << filterInsertStmt.lastError().text();
                    QSqlDatabase::database().rollback();
                    return;
                }
            }
            QSqlDatabase::database().commit();
        }
        else
        {
            QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                                  QMessageBox::tr("Cannot delete QSO Filter Conditions before updating - ") + deleteFilterStmt.lastError().text());
            qInfo()<< "Cannot delete QSO Filter Conditions before updating -  " << deleteFilterStmt.lastError().text();
            QSqlDatabase::database().rollback();
            return;
        }
    }
    else
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("Cannot Update QSO Filter Matching type - ") + updateStmt.lastError().text());
        qInfo()<< "Cannot Update QSO Filter Matching type - " << updateStmt.lastError().text();
        QSqlDatabase::database().rollback();
        return;
    }

    accept();
}

void QSOFilterDetail::filterNameChanged(const QString &newFilterName)
{
    FCT_IDENTIFICATION;

    QPalette p;

    if ( filterExists(newFilterName) )
    {
        p.setColor(QPalette::Text,Qt::red);
    }
    else
    {
        p.setColor(QPalette::Text,qApp->palette().text().color());
    }

    ui->filterLineEdit->setPalette(p);
}
