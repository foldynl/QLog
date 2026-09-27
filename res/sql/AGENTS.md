# QLog SQL Migration Agent Instructions

These rules apply to schema and data migration files in `res/sql/` and supplement the repository-root `AGENTS.md`.

A schema change requires all applicable parts of the following chain:

- add the next sequential `migration_NNN.sql` file;
- register the migration resource in `res/res.qrc`;
- increment `DBSchemaMigration::latestVersion`;
- add an entry to `DBSchemaMigration::functionMigration()` only when a required transformation cannot be expressed safely in SQL; and
- update the non-GUI migration tests when schema or migration behavior changes.

Additionally:

- Keep migrations forward-only and deterministic for every supported existing schema version.
- Preserve existing user data and explicitly handle failure and rollback of partial transformations.
- Prefer SQL for schema and set-based data changes. Use a C++ migration function only for transformations that genuinely require application logic.
- Do not edit an already released migration to change deployed behavior; add a new migration unless the user explicitly confirms that the migration has never shipped.
- Avoid relying on SQLite behavior or syntax without checking compatibility with the SQLite versions bundled or used across QLog's supported platforms.
