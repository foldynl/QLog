UPDATE contacts SET hrdlog_qso_upload_status = NULL
WHERE hrdlog_qso_upload_status IN ('R', 'I');
