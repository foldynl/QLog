ALTER TABLE contacts ADD COLUMN qso_uuid TEXT;
ALTER TABLE contacts ADD COLUMN updated_at TEXT;
ALTER TABLE contacts ADD COLUMN origin_node TEXT;

CREATE UNIQUE INDEX IF NOT EXISTS idx_contacts_qso_uuid   ON contacts(qso_uuid);
CREATE INDEX        IF NOT EXISTS idx_contacts_updated_at ON contacts(updated_at);

CREATE TABLE IF NOT EXISTS contacts_tombstones (
    qso_uuid    TEXT PRIMARY KEY,
    deleted_at  TEXT NOT NULL,
    origin_node TEXT
);

CREATE INDEX IF NOT EXISTS idx_contacts_tombstones_deleted_at
    ON contacts_tombstones(deleted_at);

CREATE TABLE IF NOT EXISTS sync_runtime (
    key   TEXT PRIMARY KEY,
    value TEXT
);

CREATE TABLE IF NOT EXISTS sync_peers (
    node          TEXT PRIMARY KEY,
    last_file     TEXT,
    last_offset   INTEGER NOT NULL DEFAULT 0,
    last_seen_at  TEXT
);
