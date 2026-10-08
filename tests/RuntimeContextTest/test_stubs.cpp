#include <QtTest>
#include "data/BandmapGuide.h"
#include "data/RigProfile.h"

// Reading Activity state must not apply a preset or change Bandmap Guide settings.
RigProfilesManager::RigProfilesManager()
    : ProfileManagerSQL<RigProfile>("rig_profiles")
{
    QFAIL("Runtime collection must not apply a Rig profile");
}

void BandmapGuide::setEnabled(bool)
{
    QFAIL("Runtime collection must not change Bandmap Guide settings");
}

void BandmapGuide::setCurrentProfileId(const QString &)
{
    QFAIL("Runtime collection must not select a Bandmap Guide profile");
}

bool BandmapGuide::profileExists(const QString &)
{
    QTest::qFail("Runtime collection must not apply a Bandmap Guide profile", __FILE__, __LINE__);
    return false;
}
