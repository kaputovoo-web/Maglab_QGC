#include "MagSurveySettings.h"

#include <QCoreApplication>

MagSurveySettings* MagSurveySettings::instance()
{
    static MagSurveySettings settings;
    return &settings;
}

MagSurveySettings::MagSurveySettings(QObject* parent)
    : QObject(parent)
    , _settings(QSettings::IniFormat, QSettings::UserScope,
                QCoreApplication::organizationName(), QCoreApplication::applicationName())
{
    _load();
}

void MagSurveySettings::_load()
{
    _settings.beginGroup(QStringLiteral("MagLab/Survey"));
    _mode = qBound(0, _settings.value(QStringLiteral("mode"), _mode).toInt(), 1);
    _transport = qBound(0, _settings.value(QStringLiteral("transport"), _transport).toInt(), 2);
    _localAddress = _settings.value(QStringLiteral("localAddress"), _localAddress).toString();
    _localPort = qBound(1, _settings.value(QStringLiteral("localPort"), _localPort).toInt(), 65535);
    _remoteAddress = _settings.value(QStringLiteral("remoteAddress"), _remoteAddress).toString();
    _remotePort = qBound(1, _settings.value(QStringLiteral("remotePort"), _remotePort).toInt(), 65535);
    _requestIntervalMs = qBound(100, _settings.value(QStringLiteral("requestIntervalMs"), _requestIntervalMs).toInt(), 60000);
    // Scalar installations historically do not send F8; keep that behavior
    // unless the user explicitly enables the optional scalar heartbeat.
    _heartbeatEnabled = _settings.value(QStringLiteral("heartbeatEnabled"), _mode == Vector).toBool();
    _autoCollect = _settings.value(QStringLiteral("autoCollect"), _autoCollect).toBool();
    _autoSave = _settings.value(QStringLiteral("autoSave"), _autoSave).toBool();
    _settings.endGroup();

    // Migrate the old shared default remotePort=13551 for scalar installs.
    if (_mode == Scalar && _localPort == 14551 && _remotePort == 13551) {
        _remotePort = 14551;
        _save(QStringLiteral("remotePort"), _remotePort);
    } else if (_mode == Vector && _localPort == 13551 && _remotePort == 14551) {
        _remotePort = 13551;
        _save(QStringLiteral("remotePort"), _remotePort);
    }
}

void MagSurveySettings::_save(const QString& key, const QVariant& value)
{
    _settings.beginGroup(QStringLiteral("MagLab/Survey"));
    _settings.setValue(key, value);
    _settings.endGroup();
    _settings.sync();
}

void MagSurveySettings::setMode(int value)
{
    value = qBound(0, value, 1);
    if (_mode == value) return;
    _mode = value;
    _save(QStringLiteral("mode"), value);
    // Follow the legacy default ports when the user has not chosen custom ones.
    // Scalar keeps the historical 14551; Vector uses 13551.
    if (_mode == Vector) {
        if (_localPort == 14551) {
            setLocalPort(13551);
        }
        if (_remotePort == 14551) {
            setRemotePort(13551);
        }
    } else {
        if (_localPort == 13551) {
            setLocalPort(14551);
        }
        if (_remotePort == 13551) {
            setRemotePort(14551);
        }
    }
    emit modeChanged();
    emit configurationChanged();
}

void MagSurveySettings::setTransport(int value)
{
    value = qBound(0, value, 2);
    if (_transport == value) return;
    _transport = value;
    _save(QStringLiteral("transport"), value);
    emit transportChanged();
    emit configurationChanged();
}

void MagSurveySettings::setLocalAddress(const QString& value)
{
    if (_localAddress == value) return;
    _localAddress = value;
    _save(QStringLiteral("localAddress"), value);
    emit localAddressChanged();
    emit configurationChanged();
}

void MagSurveySettings::setLocalPort(int value)
{
    value = qBound(1, value, 65535);
    if (_localPort == value) return;
    _localPort = value;
    _save(QStringLiteral("localPort"), value);
    emit localPortChanged();
    emit configurationChanged();
}

void MagSurveySettings::setRemoteAddress(const QString& value)
{
    if (_remoteAddress == value) return;
    _remoteAddress = value;
    _save(QStringLiteral("remoteAddress"), value);
    emit remoteAddressChanged();
    emit configurationChanged();
}

void MagSurveySettings::setRemotePort(int value)
{
    value = qBound(1, value, 65535);
    if (_remotePort == value) return;
    _remotePort = value;
    _save(QStringLiteral("remotePort"), value);
    emit remotePortChanged();
    emit configurationChanged();
}

void MagSurveySettings::setRequestIntervalMs(int value)
{
    value = qBound(100, value, 60000);
    if (_requestIntervalMs == value) return;
    _requestIntervalMs = value;
    _save(QStringLiteral("requestIntervalMs"), value);
    emit requestIntervalChanged();
    emit configurationChanged();
}

void MagSurveySettings::setHeartbeatEnabled(bool value)
{
    if (_heartbeatEnabled == value) return;
    _heartbeatEnabled = value;
    _save(QStringLiteral("heartbeatEnabled"), value);
    emit heartbeatEnabledChanged();
    emit configurationChanged();
}

void MagSurveySettings::setAutoCollect(bool value)
{
    if (_autoCollect == value) return;
    _autoCollect = value;
    _save(QStringLiteral("autoCollect"), value);
    emit autoCollectChanged();
    emit configurationChanged();
}

void MagSurveySettings::setAutoSave(bool value)
{
    if (_autoSave == value) return;
    _autoSave = value;
    _save(QStringLiteral("autoSave"), value);
    emit autoSaveChanged();
    emit configurationChanged();
}

void MagSurveySettings::applyConfiguration(int mode, int transport,
                                           const QString& localAddress, int localPort,
                                           const QString& remoteAddress, int remotePort,
                                           int requestIntervalMs, bool heartbeatEnabled,
                                           bool autoCollect, bool autoSave)
{
    const int newMode = qBound(0, mode, 1);
    const int newTransport = qBound(0, transport, 2);
    const int newLocalPort = qBound(1, localPort, 65535);
    const int newRemotePort = qBound(1, remotePort, 65535);
    const int newInterval = qBound(100, requestIntervalMs, 60000);

    const bool modeChanged = _mode != newMode;
    const bool transportChanged = _transport != newTransport;
    const bool localAddressChanged = _localAddress != localAddress;
    const bool localPortChanged = _localPort != newLocalPort;
    const bool remoteAddressChanged = _remoteAddress != remoteAddress;
    const bool remotePortChanged = _remotePort != newRemotePort;
    const bool intervalChanged = _requestIntervalMs != newInterval;
    const bool heartbeatChanged = _heartbeatEnabled != heartbeatEnabled;
    const bool autoCollectChanged = _autoCollect != autoCollect;
    const bool autoSaveChanged = _autoSave != autoSave;

    _mode = newMode;
    _transport = newTransport;
    _localAddress = localAddress.trimmed();
    _localPort = newLocalPort;
    _remoteAddress = remoteAddress.trimmed();
    _remotePort = newRemotePort;
    _requestIntervalMs = newInterval;
    _heartbeatEnabled = heartbeatEnabled;
    _autoCollect = autoCollect;
    _autoSave = autoSave;

    _settings.beginGroup(QStringLiteral("MagLab/Survey"));
    _settings.setValue(QStringLiteral("mode"), _mode);
    _settings.setValue(QStringLiteral("transport"), _transport);
    _settings.setValue(QStringLiteral("localAddress"), _localAddress);
    _settings.setValue(QStringLiteral("localPort"), _localPort);
    _settings.setValue(QStringLiteral("remoteAddress"), _remoteAddress);
    _settings.setValue(QStringLiteral("remotePort"), _remotePort);
    _settings.setValue(QStringLiteral("requestIntervalMs"), _requestIntervalMs);
    _settings.setValue(QStringLiteral("heartbeatEnabled"), _heartbeatEnabled);
    _settings.setValue(QStringLiteral("autoCollect"), _autoCollect);
    _settings.setValue(QStringLiteral("autoSave"), _autoSave);
    _settings.endGroup();
    _settings.sync();

    if (modeChanged) emit this->modeChanged();
    if (transportChanged) emit this->transportChanged();
    if (localAddressChanged) emit this->localAddressChanged();
    if (localPortChanged) emit this->localPortChanged();
    if (remoteAddressChanged) emit this->remoteAddressChanged();
    if (remotePortChanged) emit this->remotePortChanged();
    if (intervalChanged) emit this->requestIntervalChanged();
    if (heartbeatChanged) emit this->heartbeatEnabledChanged();
    if (autoCollectChanged) emit this->autoCollectChanged();
    if (autoSaveChanged) emit this->autoSaveChanged();
    emit configurationChanged();
}
