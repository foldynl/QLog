#ifndef QLOG_DATA_DXCCLOGSTATUSCONDITION_H
#define QLOG_DATA_DXCCLOGSTATUSCONDITION_H

#include <functional>
#include "data/Dxcc.h"

class DxSpot;

using DxccConfirmationResolver = std::function<DxccConfirmationResult(
    int, const QString &, const QString &, AlertDxccContext)>;

class DxccLogStatusCondition
{
public:
    explicit DxccLogStatusCondition(int mask = DxccStatus::All);

    bool hasCondition() const;
    bool isInvalid() const;
    bool isFiltered() const;
    bool match(const DxSpot &spot,
               const DxccConfirmationResolver &resolver = DxccConfirmationResolver()) const;
    QString toJson() const;
    static DxccLogStatusCondition fromJson(const QString &value);

    int mask;
    int goal = static_cast<int>(AlertDxccGoal::Confirmed);
    int context = static_cast<int>(AlertDxccContext::Anywhere);
    bool goalSet = false;
    bool contextSet = false;
};

#endif // QLOG_DATA_DXCCLOGSTATUSCONDITION_H
