#include "RigRuntimeProvider.h"
#include "core/debug.h"
#include <QtMath>

MODULE_IDENTIFICATION("qlog.rig.rigruntimeprovider");

RuntimeProvider *createRigRuntimeProvider(QObject *owner, const Rig::Status &status)
{
    FCT_IDENTIFICATION;

    auto provider = new RuntimeProvider(owner);
    provider->addBoolean("rig_connected",
                         QStringLiteral("Whether QLog's radio driver has a ready connection to the controlled "
                                         "transceiver. False means no ready radio connection; this value does "
                                         "not indicate whether the radio is powered on or transmitting. "
                                         "Null if the status could not be collected."),
                        [&status]() { return QJsonValue(status.isConnected); });

    provider->addString("rig_profile",
                        QStringLiteral("Name of the radio-control profile used for the connected transceiver "
                                       "(device and driver connection settings). This is not a logging station "
                                       "profile or callsign and does not select a QSO scope. Null if disconnected "
                                       "or no profile name is known."),
                        [&status]()
                        {
                            return status.isConnected && !status.profile.isEmpty() ? QJsonValue(status.profile)
                                                                                   : QJsonValue();
                        });

    provider->addNumber("rig_frequency",
                        QStringLiteral("Main VFO frequency most recently reported by the radio driver, in "
                                       "hertz (Hz): 14074000 means 14.074 MHz. This is the main VFO value, "
                                       "before receive/transmit tuning offsets; use rig_rx_frequency "
                                       "or rig_tx_frequency for QLog's adjusted RX/TX values. Null if disconnected "
                                       "or no valid frequency is available."), "Hz",
                        [&status]()
                        {
                            // Driver notifications and Rig::Status use MHz.
                            const double frequencyHz = status.freq * 1000000.0;
                            return status.isConnected && frequencyHz > 0.0 && qIsFinite(frequencyHz) ? QJsonValue(frequencyHz)
                                                                                                     : QJsonValue();
                        });
    provider->addNumber("rig_rx_frequency",
                        QStringLiteral("Receive frequency most recently supplied to QLog by the driver, "
                                       "in hertz (Hz). Includes the receive adjustment already applied "
                                       "by that driver, such as a profile RX offset or CAT RIT. "
                                       "Do not add rig_rx_offset or rig_rit again. Not a measured RF "
                                       "frequency or a manually edited QSO frequency. Null when "
                                       "disconnected or no valid RX frequency has arrived."), "Hz",
                        [&status]()
                        {
                            const double frequencyHz = status.rxFreq * 1000000.0;
                            return status.isConnected && status.freq > 0.0 && qIsFinite(status.freq * 1000000.0)
                                   && frequencyHz > 0.0 && qIsFinite(frequencyHz)
                                   ? QJsonValue(frequencyHz)
                                   : QJsonValue();
                        });

    provider->addNumber("rig_rx_offset",
                        QStringLiteral("Manual receive-frequency offset configured in the connected "
                                       "QLog radio profile, in hertz (Hz). A signed configuration "
                                       "value, not the radio's CAT RIT setting, even though QLog's "
                                       "rig panel may label it RIT. Zero means no profile RX offset. "
                                       "Whether it is applied depends on the driver and CAT reading "
                                       "settings; use rig_rx_frequency for the driver's adjusted "
                                       "value instead of adding offsets yourself. Null when "
                                       "disconnected or the configured value is invalid."), "Hz",
                        [&status]()
                        {
                            const double offsetHz = status.rxOffset * 1000000.0;
                            return status.isConnected && qIsFinite(offsetHz) ? QJsonValue(offsetHz)
                                                                            : QJsonValue();
                        });

    provider->addNumber("rig_tx_offset",
                        QStringLiteral("Manual transmit-frequency offset configured in the connected "
                                       "QLog radio profile, in hertz (Hz). A signed configuration "
                                       "value, not the radio's CAT XIT setting or a split VFO, even "
                                       "though QLog's rig panel may label it XIT. Zero means no "
                                       "profile TX offset. Whether it is applied depends on the "
                                       "driver and CAT reading settings; use rig_tx_frequency for "
                                       "QLog's TX value instead of adding offsets yourself. Null "
                                       "when disconnected or the configured value is invalid."), "Hz",
                        [&status]()
                        {
                            const double offsetHz = status.txOffset * 1000000.0;
                            return status.isConnected && qIsFinite(offsetHz) ? QJsonValue(offsetHz)
                                                                            : QJsonValue();
                        });

    provider->addString("rig_mode",
                        QStringLiteral("Operating-mode label most recently reported by the radio driver. "
                                       "Uses the more specific submode when available (for example USB "
                                       "instead of SSB), otherwise the mode (for example CW). Null if "
                                       "disconnected or no mode is known."),
                        [&status]()
                        {
                            const QString &mode = status.submode.isEmpty() ? status.mode
                                                                           : status.submode;
                            return status.isConnected && !mode.isEmpty() ? QJsonValue(mode)
                                                                         : QJsonValue();
                        });

    provider->addString("rig_backend",
                        QStringLiteral("Radio-control backend used by QLog's connected driver: "
                                       "Hamlib, Omnirig v1, Omnirig v2, FLRig or TCI. This is "
                                       "the control software interface, not the radio model. "
                                       "Supported readings also depend on the radio model and "
                                       "profile settings. Null when disconnected or unknown."),
                        [&status]()
                        {
                            return status.isConnected && !status.backend.isEmpty() ? QJsonValue(status.backend)
                                                                                   : QJsonValue();
                        });

    provider->addString("rig_model",
                        QStringLiteral("Configured Hamlib radio-driver label: manufacturer, "
                                       "radio model and Hamlib driver version in parentheses. "
                                       "Identifies the model selected for radio control, not "
                                       "a device identity verified by querying the radio, "
                                       "serial number, radio firmware version or QLog profile "
                                       "name. Local rigctld sharing preserves the original "
                                       "model. Null when disconnected, the model is unknown, "
                                       "the selected driver is NET rigctl without a known "
                                       "underlying radio model, or the backend is not Hamlib. "
                                       "Rig-slot labels such as Rig 1 are not radio models."),
                        [&status]()
                        {
                            return status.isConnected && !status.modelName.isEmpty() ? QJsonValue(status.modelName)
                                                                                     : QJsonValue();
                        });

    provider->addNumber("rig_power",
                        QStringLiteral("RF output-power setting most recently reported by the "
                                       "radio-control backend, in watts (W). Not an instantaneous "
                                       "RF power-meter reading. TCI estimates watts from drive "
                                       "percentage and the profile's configured default power "
                                       "(100 W when unset). Zero is valid when reported. Null "
                                       "when disconnected, reading is disabled or unsupported, "
                                       "or no valid report has arrived."), "W",
                        [&status]()
                        {
                            return status.isConnected && status.powerKnown && status.power >= 0.0
                                   && qIsFinite(status.power) ? QJsonValue(status.power)
                                                              : QJsonValue();
                        });

    provider->addNumber("rig_key_speed",
                        QStringLiteral("CW keying speed most recently reported by the radio's "
                                       "control backend, in words per minute (WPM). Describes "
                                       "the transceiver's CAT keyer, not a separate WinKeyer "
                                       "or other external keyer. Null when disconnected, "
                                       "reading is disabled or unsupported, or no positive "
                                       "speed has been reported."), "WPM",
                        [&status]()
                        {
                            return status.isConnected && status.keySpeedKnown && status.keySpeed > 0
                                   ? QJsonValue(status.keySpeed)
                                   : QJsonValue();
                        });

    provider->addString("rig_vfo",
                        QStringLiteral("Active VFO label most recently reported by the backend. "
                                       "Labels depend on the backend and radio, for example A, "
                                       "B, Main or Sub. Curr is a current-VFO alias and does "
                                       "not identify A or B. Null when disconnected, reading "
                                       "is disabled or unsupported, or no label was reported."),
                        [&status]()
                        {
                            return status.isConnected && status.vfoKnown && !status.vfo.isEmpty() ? QJsonValue(status.vfo)
                                                                                                  : QJsonValue();
                        });

    provider->addNumber("rig_rit",
                        QStringLiteral("Effective receive incremental tuning (RIT) offset most "
                                       "recently reported by the backend, in hertz (Hz). "
                                       "Independent of the manual profile offset rig_rx_offset. "
                                       "A signed offset relative to the main VFO, not an "
                                       "absolute frequency. Zero is valid and does not prove "
                                       "whether the RIT switch is enabled. Null when disconnected, "
                                       "reading is disabled or unsupported, or no valid report "
                                       "has arrived."), "Hz",
                        [&status]()
                        {
                            const double offsetHz = status.rit * 1000000.0;
                            return status.isConnected && status.ritKnown && qIsFinite(offsetHz)
                                   ? QJsonValue(offsetHz)
                                   : QJsonValue();
                        });

    provider->addNumber("rig_xit",
                        QStringLiteral("Effective transmit incremental tuning (XIT) offset most "
                                       "recently reported by the backend, in hertz (Hz). "
                                       "Independent of the manual profile offset rig_tx_offset. "
                                       "A signed offset relative to the main VFO, not an "
                                       "absolute frequency or the split TX VFO. Zero is valid "
                                       "and does not prove whether the XIT switch is enabled. "
                                       "Null when disconnected, reading is disabled or "
                                       "unsupported, or no valid report has arrived."), "Hz",
                        [&status]()
                        {
                            const double offsetHz = status.xit * 1000000.0;
                            return status.isConnected && status.xitKnown && qIsFinite(offsetHz)
                                   ? QJsonValue(offsetHz)
                                   : QJsonValue();
                        });

    provider->addNumber("rig_bandwidth",
                        QStringLiteral("Receiver filter passband width most recently reported "
                                       "with the operating mode, in hertz (Hz). For example "
                                       "2400 means a 2.4 kHz filter, not an amateur-band width "
                                       "or guaranteed emitted signal bandwidth. Null when "
                                       "disconnected, mode reading is disabled, or the backend "
                                       "has not supplied a positive width. TCI currently does "
                                       "not supply this width to QLog."), "Hz",
                        [&status]()
                        {
                            return status.isConnected && status.bandwidthKnown && status.bandwidth > 0
                                   ? QJsonValue(status.bandwidth)
                                   : QJsonValue();
                        });

    provider->addBoolean("rig_split_enabled",
                         QStringLiteral("Whether the backend last reported split operation "
                                        "enabled: receive and transmit use separate VFOs. "
                                        "False is a reported split-off state, not proof that "
                                        "QLog can read split. This does not indicate PTT or "
                                        "whether the radio is transmitting. Null when disconnected, "
                                        "reading is disabled or unsupported, or no split report "
                                        "has arrived."),
                         [&status]()
                         {
                             return status.isConnected && status.splitKnown ? QJsonValue(status.splitEnabled)
                                                                            : QJsonValue();
                         });

    provider->addNumber("rig_tx_frequency",
                        QStringLiteral("Transmit frequency most recently supplied to QLog by the driver, "
                                       "in hertz (Hz). During a known active split session, this is "
                                       "the separate TX VFO report, without additional offset "
                                       "correction. Otherwise it is the main-VFO TX value already "
                                       "adjusted by the driver, such as by a profile TX offset or "
                                       "CAT XIT. Do not add rig_tx_offset or rig_xit again. When "
                                       "rig_split_enabled is null, this describes QLog's main-VFO "
                                       "TX value without verifying the radio's actual split state. "
                                       "Not a measured RF frequency, PTT state or manually edited "
                                       "QSO frequency. Null when disconnected, no valid frequency "
                                       "has arrived, or split is enabled without a valid TX report "
                                       "for that session. Do not interpret null as zero."), "Hz",
                        [&status]()
                        {
                            if ( status.splitEnabled && ( !status.splitKnown || !status.txFrequencyKnown ) )
                                return QJsonValue();
                            if ( !status.splitEnabled && ( status.freq <= 0.0 || !qIsFinite(status.freq * 1000000.0) ) )
                                return QJsonValue();
                            const double frequencyHz = (status.splitEnabled ? status.txFreq : status.mainTxFreq) * 1000000.0;
                            return status.isConnected && frequencyHz > 0.0 && qIsFinite(frequencyHz)
                                   ? QJsonValue(frequencyHz)
                                   : QJsonValue();
                        });

    return provider;
}
