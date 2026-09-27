ALTER TABLE alert_rules ADD dx_logstatus_scope INTEGER DEFAULT 0;

UPDATE alert_rules SET dx_logstatus = CASE
    WHEN (dx_logstatus & 32) <> 0 THEN 127
    WHEN (dx_logstatus & 12) <> 0 THEN 15 | (dx_logstatus & 16)
    WHEN (dx_logstatus & 2) <> 0 THEN 3 | (dx_logstatus & 16)
    ELSE 1 | (dx_logstatus & 16)
END;

UPDATE alert_rules SET dx_logstatus_scope = CASE
    WHEN dx_logstatus IN (1, 17) THEN 2
    WHEN dx_logstatus IN (3, 19) THEN 1
    ELSE 0
END;
