// This module is compiled only under Windows - therefore no ifdef related to Windows is needed

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

#include "OmniRigRigFile.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.rig.driver.omnirigrigfile");

namespace
{

enum class CwKind { Unknown, Normal, Reverse };

/* OmniRig is a 32-bit COM server, so it is registered in the 32-bit view of the
   registry. The value may be quoted and may carry arguments after the program. */
QString serverDirectory(const QUuid &clsid)
{
    FCT_IDENTIFICATION;

    const QSettings registry(QStringLiteral("HKEY_CLASSES_ROOT\\CLSID\\%1\\LocalServer32")
                             .arg(clsid.toString(QUuid::WithBraces)),
                             QSettings::Registry32Format);

    QString path = registry.value(QStringLiteral("Default")).toString().trimmed();
    path.remove(QChar('"'));

    const int exe = path.indexOf(QStringLiteral(".exe"), 0, Qt::CaseInsensitive);

    if ( exe >= 0 )
        path.truncate(exe + 4);

    return path.isEmpty() ? QString() : QFileInfo(path).absolutePath();
}

/* OmniRig 1 keeps its rig files beside the program, Omni-Rig V2 in the user's
   profile. */
QString rigFilePath(const QUuid &clsid, const QString &rigType)
{
    FCT_IDENTIFICATION;

    const QString fileName = rigType + QStringLiteral(".ini");
    QStringList dirs;

    const QString serverDir = serverDirectory(clsid);

    if ( !serverDir.isEmpty() )
        dirs << serverDir + QStringLiteral("/Rigs");

    const QString appData = qEnvironmentVariable("APPDATA");

    if ( !appData.isEmpty() )
        dirs << QDir::fromNativeSeparators(appData) + QStringLiteral("/Afreet/Rigs");

    for ( const QString &dir : static_cast<const QStringList &>(dirs) )
    {
        const QString candidate = dir + QChar('/') + fileName;

        if ( QFileInfo::exists(candidate) )
            return candidate;
    }

    return QString();
}

QString sectionCommand(const QString &path, const QString &section)
{
    FCT_IDENTIFICATION;

    QFile file(path);

    if ( !file.open(QIODevice::ReadOnly | QIODevice::Text) )
        return QString();

    QTextStream in(&file);
    const QString header = QChar('[') + section + QChar(']');
    bool inSection = false;

    while ( !in.atEnd() )
    {
        const QString line = in.readLine().trimmed();

        if ( line.startsWith(QChar('[')) )
        {
            inSection = ( line.compare(header, Qt::CaseInsensitive) == 0 );
            continue;
        }

        if ( inSection && line.startsWith(QStringLiteral("Command"), Qt::CaseInsensitive) )
        {
            const int equals = line.indexOf(QChar('='));

            if ( equals > 0 )
                return line.mid(equals + 1).trimmed();
        }
    }

    return QString();
}

/* Kenwood, Elecraft and Yaesu take text, where mode 3 is CW and 7 its reverse
   (MD3; or, on a Yaesu, MD03;). Icom takes CI-V bytes written in hex, where mode
   03 is CW and 07 CW-R, set by command 06, 01 or, with the data flag, 26 00. */
CwKind cwKind(const QString &command)
{
    FCT_IDENTIFICATION;

    // not ZZMD: that is the PowerSDR and SmartSDR command, numbered otherwise
    static const QRegularExpression text(QStringLiteral("^\\(.*(?<![A-Z])MD0?([37]);.*\\)$"));
    /* SmartSDR numbers CW-L 03 and CW-U 04, and calls CW-U plain CW. Read
       as the Kenwood MD it ends with, its CW-L passed for the normal CW. */
    static const QRegularExpression flex(QStringLiteral("^\\(.*ZZMD0([34]);.*\\)$"));
    static const QRegularExpression civ(QStringLiteral("^FEFE[0-9A-F]{2}E0(?:06|01|260[01])(0[37])"));

    const QString upper = command.toUpper();
    const QRegularExpressionMatch flexMatch = flex.match(upper);

    if ( flexMatch.hasMatch() )
        return ( flexMatch.captured(1) == QLatin1String("4") ) ? CwKind::Normal : CwKind::Reverse;

    const QRegularExpressionMatch textMatch = text.match(upper);

    if ( textMatch.hasMatch() )
        return ( textMatch.captured(1) == QLatin1String("3") ) ? CwKind::Normal : CwKind::Reverse;

    const QRegularExpressionMatch civMatch = civ.match(QString(upper).remove(QChar('.')));

    if ( civMatch.hasMatch() )
        return ( civMatch.captured(1) == QLatin1String("03") ) ? CwKind::Normal : CwKind::Reverse;

    return CwKind::Unknown;
}

}

bool OmniRigRigFile::normalCwIsUpper(const QUuid &serverClsid, const QString &rigType)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << serverClsid << rigType;

    if ( rigType.isEmpty() )
        return false;

    const QString path = rigFilePath(serverClsid, rigType);

    if ( path.isEmpty() )
    {
        qCDebug(runtime) << "Rig file not found for" << rigType;
        return false;
    }

    CwKind upper = cwKind(sectionCommand(path, QStringLiteral("pmCW_U")));

    // CW_L answers the same question when CW_U says nothing usable
    if ( upper == CwKind::Unknown )
    {
        switch ( cwKind(sectionCommand(path, QStringLiteral("pmCW_L"))) )
        {
        case CwKind::Normal:  upper = CwKind::Reverse; break;
        case CwKind::Reverse: upper = CwKind::Normal;  break;
        default: break;
        }
    }

    qCDebug(runtime) << "Rig file" << path << "normal CW on CW_U:" << ( upper == CwKind::Normal );

    return upper == CwKind::Normal;
}
