#include "debug.h"

static bool logToFile = false;
static QString debugLogFilename;

bool isLogToFileEnabled()
{
    return logToFile;
}

void setLogToFile(bool enabled)
{
    logToFile = enabled;
}

QString currentDebugLogFilename()
{
    return debugLogFilename;
}

void setCurrentDebugLogFilename(const QString &filename)
{
    debugLogFilename = filename;
}

void set_debug_level(DEBUG_LEVEL_TYPE type)
{
    switch ( type )
    {
    case LEVEL_DEBUG_MAX:
        QLoggingCategory::setFilterRules("*.debug=true \n"
                                         "*.function.parameters=true \n"
                                         "*.function.entered=true\n");
        break;
    case LEVEL_DEBUG_FUNCTION_PARAMETERS:
        QLoggingCategory::setFilterRules("*.function.parameters=true \n"
                                         "*.function.entered=true\n");
        break;
    case LEVEL_DEBUG_FUNCTION_CALLS:
        QLoggingCategory::setFilterRules("*.function.parameters=false\n");
        break;
    case LEVEL_DEBUG_RUNTIME:
        QLoggingCategory::setFilterRules("*.function.*=false\n");
        break;
    case LEVEL_PRODUCTION:
    default:
        QLoggingCategory::setFilterRules("*.debug=false\n");
    }
}

