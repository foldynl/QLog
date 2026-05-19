ALTER TABLE rig_profiles ADD get_split INTEGER DEFAULT 0;

CREATE TABLE IF NOT EXISTS cabrillo_templates (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT    NOT NULL UNIQUE,
    contest_name_for_header TEXT,
    default_category_mode   TEXT
);

CREATE TABLE IF NOT EXISTS cabrillo_template_columns (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    template_id INTEGER NOT NULL REFERENCES cabrillo_templates(id) ON DELETE CASCADE,
    position    INTEGER NOT NULL,
    db_field    TEXT    NOT NULL,
    width       INTEGER NOT NULL,
    formatter   TEXT,
    label       TEXT
);

DELETE FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_templates (name, contest_name_for_header, default_category_mode) VALUES ('ARRL DX CW', 'ARRL-DX-CW', 'CW');
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 1, 'freq', 5, 'freq_khz', 'Freq' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 2, 'mode', 2, 'mode_cabrillo', 'Mo' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 3, 'start_time', 10, 'date_yyyy_mm_dd', 'Date' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 4, 'start_time', 4, 'time_hhmm', 'Time' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 5, 'station_callsign', 13, 'upper', 'My Call' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 6, 'rst_sent', 3, '', 'Rst Sent' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 7, 'stx_string', 6, '', 'Exch Sent' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 8, 'callsign', 13, 'upper', 'Call Rcvd' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 9, 'rst_rcvd', 3, '', 'Rst Rcvd' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 10, 'srx_string', 6, '', 'Exch Rcvd' FROM cabrillo_templates WHERE name = 'ARRL DX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 11, '', 1, 'transmitter_id', 't' FROM cabrillo_templates WHERE name = 'ARRL DX CW';

DELETE FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_templates (name, contest_name_for_header, default_category_mode) VALUES ('ARRL DX SSB', 'ARRL-DX-SSB', 'SSB');
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 1, 'freq', 5, 'freq_khz', 'Freq' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 2, 'mode', 2, 'mode_cabrillo', 'Mo' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 3, 'start_time', 10, 'date_yyyy_mm_dd', 'Date' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 4, 'start_time', 4, 'time_hhmm', 'Time' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 5, 'station_callsign', 13, 'upper', 'My Call' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 6, 'rst_sent', 3, '', 'Rst Sent' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 7, 'stx_string', 6, '', 'Exch Sent' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 8, 'callsign', 13, 'upper', 'Call Rcvd' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 9, 'rst_rcvd', 3, '', 'Rst Rcvd' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 10, 'srx_string', 6, '', 'Exch Rcvd' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 11, '', 1, 'transmitter_id', 't' FROM cabrillo_templates WHERE name = 'ARRL DX SSB';

DELETE FROM cabrillo_templates WHERE name = 'ARRL VHF JAN';
INSERT INTO cabrillo_templates (name, contest_name_for_header, default_category_mode) VALUES ('ARRL VHF JAN', 'ARRL-VHF-JAN', 'MIXED');
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 1, 'freq', 5, 'freq_khz', 'Freq' FROM cabrillo_templates WHERE name = 'ARRL VHF JAN';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 2, 'mode', 2, 'mode_cabrillo', 'Mo' FROM cabrillo_templates WHERE name = 'ARRL VHF JAN';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 3, 'start_time', 10, 'date_yyyy_mm_dd', 'Date' FROM cabrillo_templates WHERE name = 'ARRL VHF JAN';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 4, 'start_time', 4, 'time_hhmm', 'Time' FROM cabrillo_templates WHERE name = 'ARRL VHF JAN';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 5, 'station_callsign', 17, 'upper', 'My Call' FROM cabrillo_templates WHERE name = 'ARRL VHF JAN';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 6, 'my_gridsquare', 6, 'upper', 'Grid Sent' FROM cabrillo_templates WHERE name = 'ARRL VHF JAN';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 7, 'callsign', 17, 'upper', 'Call Rcvd' FROM cabrillo_templates WHERE name = 'ARRL VHF JAN';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 8, 'gridsquare', 6, 'upper', 'Grid Rcvd' FROM cabrillo_templates WHERE name = 'ARRL VHF JAN';

DELETE FROM cabrillo_templates WHERE name = 'CQ VHF';
INSERT INTO cabrillo_templates (name, contest_name_for_header, default_category_mode) VALUES ('CQ VHF', 'CQ-VHF', 'MIXED');
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 1, 'freq', 5, 'freq_khz', 'Freq' FROM cabrillo_templates WHERE name = 'CQ VHF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 2, 'mode', 2, 'mode_cabrillo', 'Mo' FROM cabrillo_templates WHERE name = 'CQ VHF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 3, 'start_time', 10, 'date_yyyy_mm_dd', 'Date' FROM cabrillo_templates WHERE name = 'CQ VHF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 4, 'start_time', 4, 'time_hhmm', 'Time' FROM cabrillo_templates WHERE name = 'CQ VHF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 5, 'station_callsign', 17, 'upper', 'My Call' FROM cabrillo_templates WHERE name = 'CQ VHF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 6, 'my_gridsquare', 6, 'upper', 'Grid Sent' FROM cabrillo_templates WHERE name = 'CQ VHF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 7, 'callsign', 17, 'upper', 'Call Rcvd' FROM cabrillo_templates WHERE name = 'CQ VHF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 8, 'gridsquare', 6, 'upper', 'Grid Rcvd' FROM cabrillo_templates WHERE name = 'CQ VHF';

DELETE FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_templates (name, contest_name_for_header, default_category_mode) VALUES ('CQ WPX CW', 'CQ-WPX-CW', 'CW');
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 1, 'freq', 5, 'freq_khz', 'Freq' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 2, 'mode', 2, 'mode_cabrillo', 'Mo' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 3, 'start_time', 10, 'date_yyyy_mm_dd', 'Date' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 4, 'start_time', 4, 'time_hhmm', 'Time' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 5, 'station_callsign', 13, 'upper', 'My Call' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 6, 'rst_sent', 3, '', 'Rst Sent' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 7, 'stx', 6, 'padded_nr', 'Exch Sent' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 8, 'callsign', 13, 'upper', 'Call Rcvd' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 9, 'rst_rcvd', 3, '', 'Rst Rcvd' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 10, 'srx', 6, 'padded_nr', 'Exch Rcvd' FROM cabrillo_templates WHERE name = 'CQ WPX CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 11, '', 1, 'transmitter_id', 't' FROM cabrillo_templates WHERE name = 'CQ WPX CW';

DELETE FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_templates (name, contest_name_for_header, default_category_mode) VALUES ('CQ WPX SSB', 'CQ-WPX-SSB', 'SSB');
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 1, 'freq', 5, 'freq_khz', 'Freq' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 2, 'mode', 2, 'mode_cabrillo', 'Mo' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 3, 'start_time', 10, 'date_yyyy_mm_dd', 'Date' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 4, 'start_time', 4, 'time_hhmm', 'Time' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 5, 'station_callsign', 13, 'upper', 'My Call' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 6, 'rst_sent', 3, '', 'Rst Sent' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 7, 'stx', 6, 'padded_nr', 'Exch Sent' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 8, 'callsign', 13, 'upper', 'Call Rcvd' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 9, 'rst_rcvd', 3, '', 'Rst Rcvd' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 10, 'srx', 6, 'padded_nr', 'Exch Rcvd' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 11, '', 1, 'transmitter_id', 't' FROM cabrillo_templates WHERE name = 'CQ WPX SSB';

DELETE FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_templates (name, contest_name_for_header, default_category_mode) VALUES ('CQ WW CW', 'CQ-WW-CW', 'CW');
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 1, 'freq', 5, 'freq_khz', 'Freq' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 2, 'mode', 2, 'mode_cabrillo', 'Mo' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 3, 'start_time', 10, 'date_yyyy_mm_dd', 'Date' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 4, 'start_time', 4, 'time_hhmm', 'Time' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 5, 'station_callsign', 13, 'upper', 'My Call' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 6, 'rst_sent', 3, '', 'Rst Sent' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 7, 'my_cq_zone', 6, '', 'Exch Sent' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 8, 'callsign', 13, 'upper', 'Call Rcvd' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 9, 'rst_rcvd', 3, '', 'Rst Rcvd' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 10, 'cqz', 6, '', 'Exch Rcvd' FROM cabrillo_templates WHERE name = 'CQ WW CW';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 11, '', 1, 'transmitter_id', 't' FROM cabrillo_templates WHERE name = 'CQ WW CW';

DELETE FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_templates (name, contest_name_for_header, default_category_mode) VALUES ('CQ WW SSB', 'CQ-WW-SSB', 'SSB');
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 1, 'freq', 5, 'freq_khz', 'Freq' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 2, 'mode', 2, 'mode_cabrillo', 'Mo' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 3, 'start_time', 10, 'date_yyyy_mm_dd', 'Date' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 4, 'start_time', 4, 'time_hhmm', 'Time' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 5, 'station_callsign', 13, 'upper', 'My Call' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 6, 'rst_sent', 3, '', 'Rst Sent' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 7, 'my_cq_zone', 6, '', 'Exch Sent' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 8, 'callsign', 13, 'upper', 'Call Rcvd' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 9, 'rst_rcvd', 3, '', 'Rst Rcvd' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 10, 'cqz', 6, '', 'Exch Rcvd' FROM cabrillo_templates WHERE name = 'CQ WW SSB';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 11, '', 1, 'transmitter_id', 't' FROM cabrillo_templates WHERE name = 'CQ WW SSB';

DELETE FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_templates (name, contest_name_for_header, default_category_mode) VALUES ('IARU HF', 'IARU-HF', 'MIXED');
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 1, 'freq', 5, 'freq_khz', 'Freq' FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 2, 'mode', 2, 'mode_cabrillo', 'Mo' FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 3, 'start_time', 10, 'date_yyyy_mm_dd', 'Date' FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 4, 'start_time', 4, 'time_hhmm', 'Time' FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 5, 'station_callsign', 13, 'upper', 'My Call' FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 6, 'rst_sent', 3, '', 'Rst Sent' FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 7, 'stx_string', 6, '', 'Exch Sent' FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 8, 'callsign', 13, 'upper', 'Call Rcvd' FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 9, 'rst_rcvd', 3, '', 'Rst Rcvd' FROM cabrillo_templates WHERE name = 'IARU HF';
INSERT INTO cabrillo_template_columns (template_id, position, db_field, width, formatter, label) SELECT id, 10, 'srx_string', 6, '', 'Exch Rcvd' FROM cabrillo_templates WHERE name = 'IARU HF';
