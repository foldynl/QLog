INSERT INTO modes (name, submodes, rprt, dxcc, enabled) VALUES ('FSK', '["SCAMP_FAST", "SCAMP_SLOW", "SCAMP_VSLOW"]', NULL, 'DIGITAL', 0);
INSERT INTO modes (name, submodes, rprt, dxcc, enabled) VALUES ('MTONE', '["SCAMP_OO", "SCAMP_OO_SLW"]', NULL, 'DIGITAL', 0);

ALTER TABLE contacts ADD eqsl_ag TEXT;
