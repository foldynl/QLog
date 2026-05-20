ALTER TABLE contacts ADD qrzcalleu_qso_upload_date TEXT;
ALTER TABLE contacts ADD qrzcalleu_qso_upload_status CHECK(qrzcalleu_qso_upload_status IN ('N', 'Y', 'M')) DEFAULT 'N';
UPDATE contacts SET qrzcalleu_qso_upload_status = 'N' WHERE qrzcalleu_qso_upload_status IS NULL;
