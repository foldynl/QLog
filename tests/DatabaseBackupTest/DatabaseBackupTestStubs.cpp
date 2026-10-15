#include "core/Migration.h"

// Database backup tests must not run migrations.
bool DBSchemaMigration::run(QString *)
{
    qFatal("Database backup tests must not run migrations");
}
