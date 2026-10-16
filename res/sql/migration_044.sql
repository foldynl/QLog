DROP TRIGGER IF EXISTS update_contacts_upload_status;

UPDATE contacts SET
    qsl_rcvd = CASE
        WHEN upper(trim(qsl_rcvd, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qsl_rcvd)
        THEN upper(trim(qsl_rcvd, char(9, 10, 11, 12, 13, 32))) ELSE 'N' END,
    lotw_qsl_rcvd = CASE
        WHEN upper(trim(lotw_qsl_rcvd, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qsl_rcvd)
        THEN upper(trim(lotw_qsl_rcvd, char(9, 10, 11, 12, 13, 32))) ELSE 'N' END,
    eqsl_qsl_rcvd = CASE
        WHEN upper(trim(eqsl_qsl_rcvd, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qsl_rcvd)
        THEN upper(trim(eqsl_qsl_rcvd, char(9, 10, 11, 12, 13, 32))) ELSE 'N' END,
    qsl_sent = CASE
        WHEN upper(trim(qsl_sent, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qsl_sent)
        THEN upper(trim(qsl_sent, char(9, 10, 11, 12, 13, 32))) ELSE 'N' END,
    lotw_qsl_sent = CASE
        WHEN upper(trim(lotw_qsl_sent, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qsl_sent)
        THEN upper(trim(lotw_qsl_sent, char(9, 10, 11, 12, 13, 32))) ELSE 'N' END,
    eqsl_qsl_sent = CASE
        WHEN upper(trim(eqsl_qsl_sent, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qsl_sent)
        THEN upper(trim(eqsl_qsl_sent, char(9, 10, 11, 12, 13, 32))) ELSE 'N' END,
    clublog_qso_upload_status = CASE
        WHEN upper(trim(clublog_qso_upload_status, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qso_upload_status)
        THEN upper(trim(clublog_qso_upload_status, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    hrdlog_qso_upload_status = CASE
        WHEN upper(trim(hrdlog_qso_upload_status, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qso_upload_status)
        THEN upper(trim(hrdlog_qso_upload_status, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    qrzcom_qso_upload_status = CASE
        WHEN upper(trim(qrzcom_qso_upload_status, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qso_upload_status)
        THEN upper(trim(qrzcom_qso_upload_status, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    hamlogeu_qso_upload_status = CASE
        WHEN upper(trim(hamlogeu_qso_upload_status, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qso_upload_status)
        THEN upper(trim(hamlogeu_qso_upload_status, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    hamqth_qso_upload_status = CASE
        WHEN upper(trim(hamqth_qso_upload_status, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qso_upload_status)
        THEN upper(trim(hamqth_qso_upload_status, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    force_init = CASE
        WHEN upper(trim(force_init, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_boolean)
        THEN upper(trim(force_init, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    qso_random = CASE
        WHEN upper(trim(qso_random, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_boolean)
        THEN upper(trim(qso_random, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    silent_key = CASE
        WHEN upper(trim(silent_key, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_boolean)
        THEN upper(trim(silent_key, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    swl = CASE
        WHEN upper(trim(swl, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_boolean)
        THEN upper(trim(swl, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    qsl_rcvd_via = CASE
        WHEN upper(trim(qsl_rcvd_via, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qsl_via)
        THEN upper(trim(qsl_rcvd_via, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    qsl_sent_via = CASE
        WHEN upper(trim(qsl_sent_via, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qsl_via)
        THEN upper(trim(qsl_sent_via, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    ant_path = CASE
        WHEN upper(trim(ant_path, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_ant_path)
        THEN upper(trim(ant_path, char(9, 10, 11, 12, 13, 32))) ELSE NULL END,
    qso_complete = CASE
        WHEN upper(trim(qso_complete, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qso_complete)
        THEN upper(trim(qso_complete, char(9, 10, 11, 12, 13, 32))) ELSE NULL END
WHERE qsl_rcvd NOT IN (SELECT value FROM adif_enum_qsl_rcvd)
   OR lotw_qsl_rcvd NOT IN (SELECT value FROM adif_enum_qsl_rcvd)
   OR eqsl_qsl_rcvd NOT IN (SELECT value FROM adif_enum_qsl_rcvd)
   OR qsl_sent NOT IN (SELECT value FROM adif_enum_qsl_sent)
   OR lotw_qsl_sent NOT IN (SELECT value FROM adif_enum_qsl_sent)
   OR eqsl_qsl_sent NOT IN (SELECT value FROM adif_enum_qsl_sent)
   OR clublog_qso_upload_status NOT IN (SELECT value FROM adif_enum_qso_upload_status)
   OR hrdlog_qso_upload_status NOT IN (SELECT value FROM adif_enum_qso_upload_status)
   OR qrzcom_qso_upload_status NOT IN (SELECT value FROM adif_enum_qso_upload_status)
   OR hamlogeu_qso_upload_status NOT IN (SELECT value FROM adif_enum_qso_upload_status)
   OR hamqth_qso_upload_status NOT IN (SELECT value FROM adif_enum_qso_upload_status)
   OR force_init NOT IN (SELECT value FROM adif_enum_boolean)
   OR qso_random NOT IN (SELECT value FROM adif_enum_boolean)
   OR silent_key NOT IN (SELECT value FROM adif_enum_boolean)
   OR swl NOT IN (SELECT value FROM adif_enum_boolean)
   OR qsl_rcvd_via NOT IN (SELECT value FROM adif_enum_qsl_via)
   OR qsl_sent_via NOT IN (SELECT value FROM adif_enum_qsl_via)
   OR ant_path NOT IN (SELECT value FROM adif_enum_ant_path)
   OR qso_complete NOT IN (SELECT value FROM adif_enum_qso_complete);

UPDATE contacts_autovalue SET wavelog_qso_upload_status = CASE
    WHEN upper(trim(wavelog_qso_upload_status, char(9, 10, 11, 12, 13, 32))) IN (SELECT value FROM adif_enum_qso_upload_status)
    THEN upper(trim(wavelog_qso_upload_status, char(9, 10, 11, 12, 13, 32))) ELSE NULL END
WHERE wavelog_qso_upload_status NOT IN (SELECT value FROM adif_enum_qso_upload_status);

DELETE FROM contacts_autovalue
WHERE contactid IS NOT NULL
  AND NOT EXISTS (SELECT 1 FROM contacts WHERE contacts.id = contacts_autovalue.contactid);

DELETE FROM contacts_qsl_cards
WHERE contactid IS NOT NULL
  AND NOT EXISTS (SELECT 1 FROM contacts WHERE contacts.id = contacts_qsl_cards.contactid);

DELETE FROM qso_filter_rules
WHERE filter_name IS NOT NULL
  AND NOT EXISTS (SELECT 1 FROM qso_filters WHERE qso_filters.filter_name = qso_filter_rules.filter_name);

DELETE FROM cabrillo_template_columns
WHERE template_id IS NOT NULL
  AND NOT EXISTS (SELECT 1 FROM cabrillo_templates WHERE cabrillo_templates.id = cabrillo_template_columns.template_id);
