ALTER TABLE rig_profiles ADD ptt_type TEXT;
ALTER TABLE rig_profiles ADD ptt_port_pathname TEXT;

UPDATE rig_profiles SET ptt_type = 'cat' WHERE driver = 1;
