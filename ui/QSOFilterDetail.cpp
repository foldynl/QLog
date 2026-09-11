#include <QMessageBox>
#include <QDateTimeEdit>
#include <QEvent>
#include <QScreen>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardItemModel>
#include "QSOFilterDetail.h"
#include "ui_QSOFilterDetail.h"
#include "ui_QSOFilterRule.h"
#include "core/debug.h"
#include "data/Data.h"
#include "core/QSOFilterManager.h"
#include "ui/component/LogbookFieldComboBox.h"
#include "ui/component/QSOFilterDateRangeEdit.h"

MODULE_IDENTIFICATION("qlog.ui.qsofilterdetail");

class QSOFilterDetail::Condition : public QWidget
{
public:
    explicit Condition(QWidget *parent) : QWidget(parent) { ui.setupUi(this); }

    Ui::QSOFilterRule ui;
    QSOFilterRule originalRule;
    QString initialValue;
};

QSOFilterDetail::QSOFilterDetail(const QString &filterName, QWidget *parent, bool clone) :
    QDialog(parent),
    ui(new Ui::QSOFilterDetail),
    filterName(filterName),
    condCount(0)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
    ui->conditionsWidget->installEventFilter(this);
    ui->parameterButtonBox->hide();
    ui->periodErrorLabel->hide();

    if ( clone || filterName.isEmpty() )
        filterNamesList = QSOFilterManager::instance()->getFilterList();

    if ( ! filterName.isEmpty() )
    {
        loadFilter(filterName);

        if ( clone )
        {
            ui->filterLineEdit->setEnabled(true);
            ui->filterLineEdit->clear();
            ui->filterLineEdit->setPlaceholderText(tr("Enter a new name"));
            ui->filterLineEdit->setFocus();
        }
    }
}

QSOFilterDetail::QSOFilterDetail(const QSOFilter &filter, QWidget *parent)
    : QSOFilterDetail(QString(), parent)
{
    parametersOnly = true;
    filterName = filter.filterName;
    setWindowTitle(tr("Filter Parameters: %1").arg(filterName));
    ui->filterNameLabel->hide();
    ui->filterLineEdit->hide();
    ui->matchingCombo->hide();
    ui->addConditionButton->hide();
    ui->buttonBox->hide();
    ui->parameterButtonBox->show();
    connect(ui->parameterButtonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &QSOFilterDetail::save);
    auto *reset = ui->parameterButtonBox->button(QDialogButtonBox::RestoreDefaults);
    connect(reset, &QPushButton::clicked, this, [this]()
    {
        loadFilter(QSOFilterManager::instance()->getFilter(filterName));
    });
    loadFilter(filter);
    setWindowFlags(Qt::Popup);
    adjustSize();
}

QSOFilterDetail::~QSOFilterDetail()
{
    FCT_IDENTIFICATION;
    delete ui;
}

bool QSOFilterDetail::eventFilter(QObject *watched, QEvent *event)
{
    if ( watched == ui->conditionsWidget && event->type() == QEvent::LayoutRequest )
    {
        const QSize contentSize = ui->conditionsLayout->minimumSize();
        // Keep complete rows visible, including when the vertical scrollbar appears.
        ui->conditionsScrollArea->setMinimumWidth(contentSize.width()
            + ui->conditionsScrollArea->verticalScrollBar()->sizeHint().width());
        if ( parametersOnly )
        {
            // Let the popup follow its contents, but keep long filters scrollable.
            ui->conditionsScrollArea->setMinimumHeight(qMin(contentSize.height(),
                screen()->availableGeometry().height() / 2));
            adjustSize();
        }
    }
    return QDialog::eventFilter(watched, event);
}

void QSOFilterDetail::addCondition(int fieldIdx, int operatorId, QString value)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "FieldIDX: " << fieldIdx << " Operator: " << operatorId << " Value: " << value;

    auto *row = new Condition(ui->conditionsWidget);
    auto &ruleUi = row->ui;
    auto *conditionLayout = ruleUi.conditionLayout;
    auto *fieldNameCombo = ruleUi.fieldNameCombo;
    auto *conditionCombo = ruleUi.conditionCombo;
    auto *stacked = ruleUi.stackedValueEdit;
    auto *removeButton = ruleUi.removeButton;
    // Keep per-row object names without duplicating their .ui definitions.
    const QObjectList controls = {conditionLayout, fieldNameCombo, conditionCombo, stacked, removeButton,
                                  ruleUi.valueLineEdit, ruleUi.valueDateEdit, ruleUi.valueDateTimeEdit};
    for ( auto *control : controls ) control->setObjectName(control->objectName() + QString::number(condCount));
    fieldNameCombo->populate(LogbookFieldComboBox::ValueMode::ColumnId);

    // Match the display order in QSOFilterRule.ui; never use translated captions as IDs.
    int index = 0;
    for ( auto op : {QSOFilterRule::Equal, QSOFilterRule::NotEqual, QSOFilterRule::Contains,
                     QSOFilterRule::NotContains, QSOFilterRule::GreaterThan, QSOFilterRule::LessThan,
                     QSOFilterRule::StartsWith, QSOFilterRule::RegExp, QSOFilterRule::InDateRange,
                     QSOFilterRule::OutsideDateRange, QSOFilterRule::BeforeDateRange, QSOFilterRule::AfterDateRange} )
        conditionCombo->setItemData(index++, op);
    Q_ASSERT(index == conditionCombo->count());
    if ( operatorId >= 0 )
        conditionCombo->setCurrentIndex(conditionCombo->findData(operatorId));

    ruleUi.valueLineEdit->setText(value);
    for ( auto *date : {static_cast<QDateTimeEdit *>(ruleUi.valueDateEdit), ruleUi.valueDateTimeEdit} )
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        date->setTimeZone(QTimeZone::UTC);
#else
        date->setTimeSpec(Qt::UTC);
#endif
    }
    ruleUi.valueDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ruleUi.valueDateTimeEdit->setDisplayFormat(locale.formatDateShortWithYYYY()
                                              + " " + locale.formatTimeLongWithoutTZ());
    if ( !value.isEmpty() )
    {
        ruleUi.valueDateEdit->setDate(QDate::fromString(value, Qt::ISODate));
        QDateTime dateTime = QDateTime::fromString(value, Qt::ISODate);
        if ( dateTime.timeSpec() == Qt::LocalTime )
        {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            dateTime.setTimeZone(QTimeZone::UTC);
#else
            dateTime.setTimeSpec(Qt::UTC);
#endif
        }
        ruleUi.valueDateTimeEdit->setDateTime(dateTime.toUTC());
    }
    const auto *data = Data::instance();
    populateComboBox(ruleUi.qslSentCombo, data->qslSentEnum, value);
    populateComboBox(ruleUi.qslSentViaCombo, data->qslSentViaEnum, value);
    populateComboBox(ruleUi.qslRcvdCombo, data->qslRcvdEnum, value);
    populateComboBox(ruleUi.uploadStatusCombo, data->uploadStatusEnum, value);
    populateComboBox(ruleUi.antPathCombo, data->antPathEnum, value);
    populateComboBox(ruleUi.boolCombo, data->boolEnum, value);
    populateComboBox(ruleUi.qsoCompleteCombo, data->qsoCompleteEnum, value);
    populateComboBox(ruleUi.downloadStatusCombo, data->downloadStatusEnum, value);
    populateComboBox(ruleUi.morseKeyTypeCombo, data->morseKeyTypeEnum, value);
    populateComboBox(ruleUi.eqslAgCombo, data->eqslAgEnum, value);
    auto *rangeEditor = new QSOFilterDateRangeEdit(value, stacked);
    stacked->addWidget(rangeEditor);

    const auto updateEditor = [row, rangeEditor]()
    {
        auto &ui = row->ui;
        auto *stacked = ui.stackedValueEdit;
        QWidget *editor = fieldEditor(row);
        const bool dateField = qobject_cast<QDateTimeEdit *>(editor) != nullptr;
        const int op = ui.conditionCombo->currentData().toInt();
        const bool dateOperator = op >= QSOFilterRule::InDateRange && op <= QSOFilterRule::AfterDateRange;
        auto *operators = qobject_cast<QStandardItemModel *>(ui.conditionCombo->model());
        for ( auto id : {QSOFilterRule::InDateRange, QSOFilterRule::OutsideDateRange,
                        QSOFilterRule::BeforeDateRange, QSOFilterRule::AfterDateRange} )
            operators->item(ui.conditionCombo->findData(id))->setEnabled(dateField);
        if ( !dateField && dateOperator )
            ui.conditionCombo->setCurrentIndex(ui.conditionCombo->findData(QSOFilterRule::Equal));

        if ( dateField && dateOperator ) editor = rangeEditor;
        stacked->setCurrentWidget(editor);

        // Hidden pages must not determine the width of every condition.
        for ( int i = 0; i < stacked->count(); ++i )
            stacked->widget(i)->setSizePolicy(i == stacked->currentIndex() ? QSizePolicy::Preferred
                                                                         : QSizePolicy::Ignored,
                                             QSizePolicy::Fixed);
    };
    if ( !parametersOnly )
    {
        connect(fieldNameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), row,
                [row, fieldIdx, conditionCombo, updateEditor]()
        {
            if ( fieldIdx < 0 && qobject_cast<QDateTimeEdit *>(fieldEditor(row)) )
                conditionCombo->setCurrentIndex(conditionCombo->findData(QSOFilterRule::InDateRange));
            updateEditor();
        });
        connect(conditionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), row, updateEditor);
    }

    /* Set FieldNameCombo here to update Stacked Widget */
    if ( fieldIdx >= 0 )
    {
        int index = fieldNameCombo->findData(fieldIdx);
        if (index != -1)
            fieldNameCombo->setCurrentIndex(index);
    }
    updateEditor();

    // Keep the exact stored representation unless the user changes the value.
    // Date editors and enum combos can otherwise normalize old values on save.
    if ( fieldIdx >= 0 )
    {
        row->originalRule = QSOFilterRule(fieldIdx, operatorId, value);
        row->initialValue = editorValue(stacked);
    }

    if ( parametersOnly )
    {
        auto *fieldLabel = new QLabel(fieldNameCombo->currentText(), row);
        auto *operatorLabel = new QLabel(conditionCombo->currentText(), row);
        delete conditionLayout->replaceWidget(fieldNameCombo, fieldLabel);
        delete conditionLayout->replaceWidget(conditionCombo, operatorLabel);
        delete fieldNameCombo;
        delete conditionCombo;
        ruleUi.fieldNameCombo = nullptr;
        ruleUi.conditionCombo = nullptr;
        removeButton->hide();
    }

    connect(removeButton, &QPushButton::clicked, this, [this, row]()
    {
        ui->conditionsLayout->removeWidget(row);
        conditions.removeOne(row);
        row->hide();
        row->deleteLater();
    });

    /**************************/
    /* Add to the main layout */
    /**************************/
    ui->conditionsLayout->insertWidget(ui->conditionsLayout->count() - 1, row);
    conditions.append(row);

    condCount++;
}

void QSOFilterDetail::loadFilter(const QString &filterName)
{
    loadFilter(QSOFilterManager::instance()->getFilter(filterName));
}

void QSOFilterDetail::loadFilter(const QSOFilter &filter)
{
    editedFilter = filter;
    qDeleteAll(conditions);
    conditions.clear();
    ui->filterLineEdit->setText(filter.filterName);
    ui->filterLineEdit->setEnabled(false);
    ui->matchingCombo->setCurrentIndex(filter.machingType);
    if ( parametersOnly )
        ui->matchingLabel->setText(filter.machingType == QSOFilter::All ? tr("All conditions must match")
                                                                     : tr("Any condition must match"));
    for ( const QSOFilterRule &rule : filter.rules )
        addCondition(rule.tableFieldIndex, rule.operatorID, rule.value);
}

bool QSOFilterDetail::filterExists(const QString &filterName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filterName;

    return filterNamesList.contains(filterName);
}

QWidget *QSOFilterDetail::fieldEditor(const Condition *row)
{
    const auto &ui = row->ui;
    switch ( ui.fieldNameCombo->currentData().toInt() )
    {
    case LogbookModel::COLUMN_QSL_RCVD_DATE:
    case LogbookModel::COLUMN_QSL_SENT_DATE:
    case LogbookModel::COLUMN_LOTW_RCVD_DATE:
    case LogbookModel::COLUMN_LOTW_SENT_DATE:
    case LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_DATE:
    case LogbookModel::COLUMN_EQSL_QSLRDATE:
    case LogbookModel::COLUMN_EQSL_QSLSDATE:
    case LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_DATE:
    case LogbookModel::COLUMN_HAMLOGEU_QSO_UPLOAD_DATE:
    case LogbookModel::COLUMN_HAMQTH_QSO_UPLOAD_DATE:
    case LogbookModel::COLUMN_DCL_QSLRDATE:
    case LogbookModel::COLUMN_DCL_QSLSDATE:
    case LogbookModel::COLUMN_QRZCOM_QSO_DOWNLOAD_DATE:
        return ui.valueDateEdit;
    case LogbookModel::COLUMN_TIME_ON:
    case LogbookModel::COLUMN_TIME_OFF:
        return ui.valueDateTimeEdit;
    case LogbookModel::COLUMN_QSL_SENT:
    case LogbookModel::COLUMN_LOTW_SENT:
    case LogbookModel::COLUMN_EQSL_QSL_SENT:
    case LogbookModel::COLUMN_DCL_QSL_SENT:
        return ui.qslSentCombo;
    case LogbookModel::COLUMN_QSL_SENT_VIA:
    case LogbookModel::COLUMN_QSL_RCVD_VIA:
        return ui.qslSentViaCombo;
    case LogbookModel::COLUMN_QSL_RCVD:
    case LogbookModel::COLUMN_LOTW_RCVD:
    case LogbookModel::COLUMN_EQSL_QSL_RCVD:
    case LogbookModel::COLUMN_DCL_QSL_RCVD:
        return ui.qslRcvdCombo;
    case LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_STATUS:
    case LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_STATUS:
    case LogbookModel::COLUMN_QRZCOM_QSO_UPLOAD_STATUS:
    case LogbookModel::COLUMN_HAMLOGEU_QSO_UPLOAD_STATUS:
    case LogbookModel::COLUMN_HAMQTH_QSO_UPLOAD_STATUS:
        return ui.uploadStatusCombo;
    case LogbookModel::COLUMN_ANT_PATH:
        return ui.antPathCombo;
    case LogbookModel::COLUMN_FORCE_INIT:
    case LogbookModel::COLUMN_QSO_RANDOM:
    case LogbookModel::COLUMN_SILENT_KEY:
    case LogbookModel::COLUMN_SWL:
        return ui.boolCombo;
    case LogbookModel::COLUMN_QSO_COMPLETE:
        return ui.qsoCompleteCombo;
    case LogbookModel::COLUMN_QRZCOM_QSO_DOWNLOAD_STATUS:
        return ui.downloadStatusCombo;
    case LogbookModel::COLUMN_MORSE_KEY_TYPE:
    case LogbookModel::COLUMN_MY_MORSE_KEY_TYPE:
        return ui.morseKeyTypeCombo;
    case LogbookModel::COLUMN_EQSL_AG:
        return ui.eqslAgCombo;
    default:
        return ui.valueLineEdit;
    }
}

void QSOFilterDetail::populateComboBox(QComboBox *combo, const QMap<QString, QString> &mapping,
                                      const QString &value)
{
    for ( auto it = mapping.cbegin(); it != mapping.cend(); ++it )
        combo->addItem(it.value(), it.key());
    if ( combo->findData(" ") < 0 ) combo->insertItem(0, tr("Blank"), " ");
    const QString selected = value.isEmpty() ? QStringLiteral(" ") : value;
    if ( combo->findData(selected) < 0 ) combo->addItem(value, value);
    combo->setCurrentIndex(combo->findData(selected));
}

void QSOFilterDetail::save()
{
    FCT_IDENTIFICATION;

    editedFilter = readFilter();
    ui->periodErrorLabel->hide();
    for ( const auto &rule : editedFilter.rules )
    {
        if ( !rule.isDateRange() ) continue;
        QSOFilterDateRange range;
        QDateTime start, end;
        if ( !QSOFilterDateRange::fromString(rule.value, range)
             || !range.resolve(QDateTime::currentDateTimeUtc().date(), start, end) )
        {
            ui->periodErrorLabel->show();
            return;
        }
    }
    if ( parametersOnly )
    {
        accept();
        return;
    }

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

    if ( !QSOFilterManager::instance()->save(editedFilter) )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("Cannot update QSO Filter Conditions"));
        return;
    }

    accept();
}

QString QSOFilterDetail::editorValue(QStackedWidget *stack)
{
    QWidget *editor = stack->currentWidget();
    if ( auto *range = qobject_cast<QSOFilterDateRangeEdit *>(editor) ) return range->value();
    if ( auto *date = qobject_cast<QDateEdit *>(editor) ) return date->date().toString(Qt::ISODate);
    if ( auto *dateTime = qobject_cast<QDateTimeEdit *>(editor) )
        return dateTime->dateTime().toString("yyyy-MM-ddTHH:mm:ss");
    QString value;
    if ( auto *line = qobject_cast<QLineEdit *>(editor) ) value = line->text();
    else if ( auto *combo = qobject_cast<QComboBox *>(editor) )
    {
        value = combo->currentData().toString();
        if ( value == " " ) value = QString();
    }
    return value.isEmpty() ? QString() : value;
}

QSOFilter QSOFilterDetail::readFilter() const
{
    QSOFilter filter;
    filter.filterName = ui->filterLineEdit->text();
    filter.machingType = ui->matchingCombo->currentIndex();
    for ( const auto *row : conditions )
    {
        QSOFilterRule rule = row->originalRule;
        if ( !parametersOnly )
        {
            rule.tableFieldIndex = row->ui.fieldNameCombo->currentData().toInt();
            rule.operatorID = row->ui.conditionCombo->currentData().toInt();
        }
        rule.value = editorValue(row->ui.stackedValueEdit);
        if ( row->originalRule.tableFieldIndex >= 0
             && rule.tableFieldIndex == row->originalRule.tableFieldIndex
             && rule.value == row->initialValue )
            rule.value = row->originalRule.value;
        filter.addRule(rule);
    }
    return filter;
}

void QSOFilterDetail::filterNameChanged(const QString &newFilterName)
{
    FCT_IDENTIFICATION;

    QPalette p;
    p.setColor(QPalette::Text, (filterExists(newFilterName)) ? Qt::red
                                                             : qApp->palette().text().color());
    ui->filterLineEdit->setPalette(p);
}
