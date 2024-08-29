ALTER TABLE ant_profiles ADD selected INTEGER;
ALTER TABLE cwkey_profiles ADD selected INTEGER;
ALTER TABLE cwshortcut_profiles ADD selected INTEGER;
ALTER TABLE rig_profiles ADD selected INTEGER;
ALTER TABLE rot_profiles ADD selected INTEGER;
ALTER TABLE rot_user_buttons_profiles ADD selected INTEGER;
ALTER TABLE station_profiles ADD selected INTEGER;
ALTER TABLE main_layout_profiles ADD selected INTEGER;

CREATE INDEX IF NOT EXISTS contacts_pota_idx ON contacts (pota_ref);
CREATE INDEX IF NOT EXISTS contacts_my_pota_idx ON contacts (my_pota_ref);
CREATE INDEX IF NOT EXISTS contacts_sota_idx ON contacts (sota_ref);
CREATE INDEX IF NOT EXISTS contacts_my_sota_idx ON contacts (my_sota_ref);
CREATE INDEX IF NOT EXISTS contacts_wwff_ref_idx ON contacts (wwff_ref);
CREATE INDEX IF NOT EXISTS contacts_my_wwff_ref_idx ON contacts (my_wwff_ref);
CREATE INDEX IF NOT EXISTS contacts_iota_idx ON contacts (iota);
CREATE INDEX IF NOT EXISTS contacts_sig_intl_idx ON contacts (sig_intl);

DROP INDEX band_idx;
CREATE INDEX IF NOT EXISTS contacts_band_idx ON contacts (band);

DROP INDEX callsign_idx;
CREATE INDEX IF NOT EXISTS contacts_callsign_idx ON contacts (callsign);

DROP INDEX dxcc_idx;
CREATE INDEX IF NOT EXISTS contacts_dxcc_idx ON contacts (dxcc);

DROP INDEX mode_idx;
CREATE INDEX IF NOT EXISTS contacts_mode_idx ON contacts (mode);

DROP INDEX start_time_idx;
CREATE INDEX IF NOT EXISTS contacts_start_time_idx ON contacts (start_time);

DROP INDEX prefix_idx;
CREATE INDEX IF NOT EXISTS dxcc_prefixes_prefix_idx ON dxcc_prefixes (prefix);

ALTER TABLE rig_profiles ADD ptt_type TEXT;
ALTER TABLE rig_profiles ADD ptt_port_pathname TEXT;

UPDATE rig_profiles SET ptt_type = 'RIG' WHERE driver = 1;
