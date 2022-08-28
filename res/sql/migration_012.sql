CREATE TABLE IF NOT EXISTS cwkey_profiles(
        profile_name TEXT PRIMARY KEY,
        model NUMBER NOT NULL,
        default_speed NUMBER NOT NULL,
        key_mode NUMBER,
        port_pathname TEXT,
        baudrate NUMBER
);

