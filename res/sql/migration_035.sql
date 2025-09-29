DROP TABLE IF EXISTS DXCC_PREFIXES;
DROP TABLE IF EXISTS DXCC_ENTITIES;

CREATE TABLE IF NOT EXISTS dxcc_entities_ad1c (
        id     INTEGER PRIMARY KEY,
        name   TEXT NOT NULL,
        prefix TEXT,
        cont   TEXT,
        cqz    INTEGER,
        ituz   INTEGER,
        lat    REAL,
        lon    REAL,
        tz     REAL
);

CREATE TABLE "dxcc_prefixes_ad1c" (
        prefix   TEXT NOT NULL,
        exact    INTEGER NOT NULL,
        dxcc     INTEGER NOT NULL,
        cqz      INTEGER,
        ituz     INTEGER,
        cont     TEXT,
        lat      REAL,
        lon      REAL
);

CREATE INDEX dxcc_prefixes_ad1c_idx ON dxcc_prefixes_ad1c (prefix, exact);

CREATE TABLE IF NOT EXISTS dxcc_entities_clublog (
        id      INTEGER PRIMARY KEY,
        name    TEXT NOT NULL,
        prefix  TEXT,
        deleted INTEGER NOT NULL,
        cont    TEXT,
        cqz     INTEGER,
        ituz    INTEGER,
        lat     REAL,
        lon     REAL,
        start   TEXT,
        "end"   TEXT
);

CREATE TABLE "dxcc_prefixes_clublog" (
        prefix   TEXT NOT NULL,
        exact    INTEGER NOT NULL,
        dxcc     INTEGER NOT NULL,
        cqz      INTEGER,
        ituz     INTEGER,
        cont     TEXT,
        lat      REAL,
        lon      REAL,
        start    TEXT,
        "end"    TEXT
);

CREATE INDEX dxcc_prefixes_clublog_idx on dxcc_prefixes_clublog (prefix, exact);

CREATE TABLE "dxcc_zone_exceptions_clublog" (
        record INTEGER PRIMARY KEY,
        call   TEXT NOT NULL,
        cqz    INTEGER NOT NULL,
        start  TEXT,
        "end"  TEXT
);

CREATE VIEW dxcc_entities AS
SELECT id, name, prefix, cont, cqz, ituz, lat, lon, tz
FROM dxcc_entities_ad1c;

CREATE VIEW dxcc_prefixes AS
SELECT prefix, exact, dxcc, cqz, ituz, cont, lat, lon
FROM dxcc_prefixes_ad1c;
