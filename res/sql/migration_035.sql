CREATE TABLE IF NOT EXISTS clublog_entities (
  adif INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  prefix TEXT NOT NULL,
  deleted INTEGER NOT NULL,
  cqz INTEGER,
  cont TEXT,
  lon REAL,
  lat REAL,
  start TEXT,
  "end" TEXT,
  whitelist INTEGER,
  whitelist_start TEXT,
  whitelist_end TEXT
);

CREATE TABLE IF NOT EXISTS clublog_exceptions (
  record INTEGER PRIMARY KEY,
  call TEXT NOT NULL,
  entity TEXT NOT NULL,
  adif INTEGER NOT NULL,
  cqz INTEGER,
  cont TEXT,
  lon REAL,
  lat REAL,
  start TEXT,
  "end" TEXT
);
CREATE INDEX IF NOT EXISTS idx_exceptions_call ON clublog_exceptions(call);

CREATE TABLE IF NOT EXISTS clublog_prefixes (
  record INTEGER PRIMARY KEY,
  call TEXT NOT NULL,
  entity TEXT NOT NULL,
  adif INTEGER NOT NULL,
  cqz INTEGER,
  cont TEXT,
  lon REAL,
  lat REAL,
  start TEXT,
  end TEXT
);
CREATE INDEX IF NOT EXISTS idx_prefixes_call ON clublog_prefixes(call);

CREATE TABLE IF NOT EXISTS clublog_invalid_ops (
  record INTEGER PRIMARY KEY,
  call TEXT NOT NULL,
  start TEXT,
  end TEXT
);
CREATE INDEX IF NOT EXISTS idx_invalid_call ON clublog_invalid_ops(call);

CREATE TABLE IF NOT EXISTS clublog_zone_exceptions (
  record INTEGER PRIMARY KEY,
  call TEXT NOT NULL,
  zone INTEGER NOT NULL,
  start TEXT,
  end TEXT
);
CREATE INDEX IF NOT EXISTS idx_zone_call ON clublog_zone_exceptions(call);

