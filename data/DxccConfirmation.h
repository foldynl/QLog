#ifndef QLOG_DATA_DXCCCONFIRMATION_H
#define QLOG_DATA_DXCCCONFIRMATION_H

#include "data/Dxcc.h"
#include "core/QuadKeyCache.h"

class DxccConfirmation
{
public:
    DxccConfirmationResult status(int dxcc,
                                  int myDxcc,
                                  const QString &band,
                                  const QString &mode,
                                  AlertDxccContext context,
                                  DxccConfirmationSources sources);

    void invalidate(int dxcc, int myDxcc);
    void clear();

    static QString confirmationCondition(DxccConfirmationSources sources,
                                         const QString &tableAlias = QString());
    static QString normalizedMode(const QString &mode);

private:
    QuadKeyCache<DxccConfirmationResult> cache {2048};
    DxccConfirmationSources cachedSources;
    bool cachedSourcesSet = false;
};

#endif // QLOG_DATA_DXCCCONFIRMATION_H
