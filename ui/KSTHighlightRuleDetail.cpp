#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QMessageBox>

#include "KSTHighlightRuleDetail.h"
#include "ui_KSTHighlightRuleDetail.h"
#include "core/debug.h"
#include "core/KSTChat.h"

MODULE_IDENTIFICATION("qlog.ui.ksthightlightruledetail");

KSTHighlightRuleDetail::KSTHighlightRuleDetail(const QString &ruleName,
                                               QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KSTHighlightRuleDetail),
    condCount(0)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->roomCombo->addItem(tr("All"));
    ui->roomCombo->addItems(KSTChat::chatRooms);

    if ( ! ruleName.isEmpty() )
    {
        loadRule(ruleName);
    }
    else
    {
        /* get Rules name from DB to checking whether a new rule name
         * will be unique */
        ruleNamesList = chatHighlightEvaluator::getAllRuleNames();
    }
}

KSTHighlightRuleDetail::~KSTHighlightRuleDetail()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void KSTHighlightRuleDetail::addCondition(int fieldIdx, int operatorId, QString value)
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
    fieldNameCombo->addItem(tr("Sender"));
    fieldNameCombo->addItem(tr("Message"));
    fieldNameCombo->addItem(tr("Gridsquare"));

    if ( fieldIdx >= 0 )
    {
        fieldNameCombo->setCurrentIndex(fieldIdx);
    }

    conditionLayout->addWidget(fieldNameCombo);

    /*******************/
    /* Condition Combo */
    /*******************/
    QComboBox* conditionCombo = new QComboBox();
    conditionCombo->addItem(QString(tr("Contains")));
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
    QLineEdit* valueEdit = new QLineEdit();
    valueEdit->setObjectName(QString::fromUtf8("valueLineEdit%1").arg(condCount));
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    valueEdit->setSizePolicy(sizePolicy);
    valueEdit->setText(value);
    conditionLayout->addWidget(valueEdit);

    /*****************/
    /* Remove Button */
    /*****************/
    QPushButton* removeButton = new QPushButton(tr("Remove"));
    removeButton->setObjectName(QString::fromUtf8("removeButton%1").arg(condCount));
    QSizePolicy sizePolicy3(QSizePolicy::Maximum, QSizePolicy::Fixed);
    removeButton->setSizePolicy(sizePolicy3);

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

void KSTHighlightRuleDetail::save()
{
    FCT_IDENTIFICATION;

    if ( ui->ruleLineEdit->text().isEmpty() )
    {
        ui->ruleLineEdit->setPlaceholderText(tr("Must not be empty"));
        return;
    }

    if ( ruleExists(ui->ruleLineEdit->text()) )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Info"),
                              QMessageBox::tr("Filter name is already exists."));
        return;
    }

    chatHighlightRule rule;

    rule.ruleName = ui->ruleLineEdit->text();
    rule.ruleRoomIndex = ui->roomCombo->currentIndex();
    rule.enabled = ui->enabledCheckbox->isChecked();
    rule.interConditionOperand = static_cast<chatHighlightRule::InterConditionOperand>(ui->matchCombo->currentIndex());

    const QList<QHBoxLayout *> &conditionLayouts = ui->conditionsLayout->findChildren<QHBoxLayout *>();
    for (auto &conditionLine: conditionLayouts )
    {
        chatHighlightRule::Condition condition;
        for ( int i = 0; i < 3; i++ )
        {
            QString objectName = conditionLine->itemAt(i)->widget()->objectName();
            if ( objectName.contains("fieldNameCom") )
            {
               condition.source = static_cast<chatHighlightRule::InfoSource>(dynamic_cast<QComboBox*>(conditionLine->itemAt(i)->widget())->currentIndex());
            }

            if ( objectName.contains("conditionCombo") )
            {
                condition.operatorID = static_cast<chatHighlightRule::Operator>(dynamic_cast<QComboBox*>(conditionLine->itemAt(i)->widget())->currentIndex());
            }
            if ( objectName.contains("valueLineEdit") )
            {
                 condition.value = dynamic_cast<QLineEdit*>(conditionLine->itemAt(i)->widget())->text();
            }
        }
        qCDebug(runtime)<< "Condition Values: " << ui->ruleLineEdit->text() << " " << condition.source
                        << " " << condition.operatorID << " " << condition.value;

        rule.conditions << condition;
    }
    rule.save();
    accept();
}

void KSTHighlightRuleDetail::ruleNameChanged(const QString &newRuleName)
{
    FCT_IDENTIFICATION;

    QPalette p;

    if ( ruleExists(newRuleName) )
    {
        p.setColor(QPalette::Text,Qt::red);
    }
    else
    {
        p.setColor(QPalette::Text,qApp->palette().text().color());
    }

    ui->ruleLineEdit->setPalette(p);
}

void KSTHighlightRuleDetail::loadRule(const QString &ruleName)
{
    FCT_IDENTIFICATION;

    chatHighlightRule rule;

    if ( !rule.load(ruleName) )
    {
        qWarning() << "Cannot load rule " << ruleName;
    }
    ui->ruleLineEdit->setText(ruleName);
    ui->ruleLineEdit->setEnabled(false);

    ui->roomCombo->setCurrentIndex(rule.ruleRoomIndex);
    ui->enabledCheckbox->setChecked(rule.enabled);
    ui->matchCombo->setCurrentIndex(rule.interConditionOperand);

    for ( const chatHighlightRule::Condition &condition : static_cast<const QList<chatHighlightRule::Condition>&>(rule.conditions))
    {
        addCondition(condition.source,
                     condition.operatorID,
                     condition.value);
    }
}

bool KSTHighlightRuleDetail::ruleExists(const QString &ruleName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << ruleName;

    return ruleNamesList.contains(ruleName);
}
