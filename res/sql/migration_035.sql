DROP TABLE IF EXISTS DXCC_PREFIXES;
DROP TABLE IF EXISTS DXCC_ENTITIES;

CREATE TABLE IF NOT EXISTS dxcc_entities_ad1c (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        prefix TEXT,
        cont TEXT,
        cqz INTEGER,
        ituz INTEGER,
        lat REAL,
        lon REAL,
        tz REAL
);

CREATE TABLE "dxcc_prefixes_ad1c" (
        "prefix"   TEXT NOT NULL,
        "exact"    INTEGER,
        "dxcc"     INTEGER,
        "cqz"      INTEGER,
        "ituz"     INTEGER,
        "cont"     TEXT,
        "lat"      REAL,
        "lon"      REAL
);

