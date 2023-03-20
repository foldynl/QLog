CREATE TABLE IF NOT EXISTS rot_user_buttons_profiles(
        profile_name TEXT PRIMARY KEY,
        button1_short  TEXT,
        button1_value  REAL,
        button2_short  TEXT,
        button2_value  REAL,
        button3_short  TEXT,
        button3_value  REAL,
        button4_short  TEXT,
        button4_value  REAL
);

INSERT INTO rot_user_buttons_profiles(
profile_name,
button1_short, button1_value,
button2_short, button2_value,
button3_short, button3_value,
button4_short, button4_value
)
VALUES (
"Basic",
"N", 0,
"S", 180,
"W", 270,
"E", 90
);
