ALTER TABLE cwkey_profiles ADD paddle_swap INTEGER DEFAULT 0;
ALTER TABLE contacts_autovalue ADD wavelog_qso_upload_status TEXT REFERENCES adif_enum_qso_upload_status("value");
ALTER TABLE contacts_autovalue ADD wavelog_qso_upload_date TEXT;
