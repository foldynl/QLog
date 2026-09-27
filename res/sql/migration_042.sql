ALTER TABLE rig_profiles ADD cwkey_output INTEGER DEFAULT 0;

ALTER TABLE alert_rules ADD dx_alert_goal INTEGER;
ALTER TABLE alert_rules ADD dx_alert_context INTEGER;
ALTER TABLE alert_rules ADD notification_enabled INTEGER NOT NULL DEFAULT 1;
ALTER TABLE alert_rules ADD notification_sound_enabled INTEGER NOT NULL DEFAULT 1;
ALTER TABLE alert_rules ADD notification_repeat_minutes INTEGER NOT NULL DEFAULT 5;

INSERT OR IGNORE INTO log_param(name, value)
SELECT 'alert/notification/enabled',
       COALESCE((SELECT value FROM log_param WHERE name = 'mainwindow/alertbeep'), 1);
DELETE FROM log_param WHERE name = 'mainwindow/alertbeep';
