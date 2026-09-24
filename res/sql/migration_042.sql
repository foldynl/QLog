ALTER TABLE rig_profiles ADD cwkey_output INTEGER DEFAULT 0;

ALTER TABLE alert_rules ADD dx_alert_goal INTEGER;
ALTER TABLE alert_rules ADD dx_alert_context INTEGER;
