#ifndef RIG_DRIVERS_OMNIRIGRIGFILE_H
#define RIG_DRIVERS_OMNIRIGRIGFILE_H

#include <QString>
#include <QUuid>

/* OmniRig names the CW modes by sideband, CW_U and CW_L, while QLog names them
   the way Hamlib does, as the rig's normal CW and its reverse. Which sideband is
   the normal one differs between rigs - CW-U on a Yaesu, the lower side on a
   recent Icom - and only the rig file OmniRig loads for the rig says which. */
namespace OmniRigRigFile
{
    // True when the rig file's CW_U command selects the rig's normal CW. False
    // when it selects the reverse, and also when the file cannot be found or
    // read, which keeps the mapping QLog has always used.
    bool normalCwIsUpper(const QUuid &serverClsid, const QString &rigType);
}

#endif // RIG_DRIVERS_OMNIRIGRIGFILE_H
