ALTER TABLE alert_rules ADD alarm INTEGER DEFAULT 0;
ALTER TABLE alert_rules ADD alarm_command TEXT;
ALTER TABLE alert_rules ADD alarm_backoff INTEGER DEFAULT 300;
