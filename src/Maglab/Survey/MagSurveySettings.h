#pragma once

#include <QObject>
#include <QSettings>
#include <QtQml/QQmlEngine>
#include <QtQml/qqmlregistration.h>

/// Persisted transport/protocol configuration for the MagLab survey receiver.
/// Values are written through QSettings so they survive application restarts.
class MagSurveySettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(int transport READ transport WRITE setTransport NOTIFY transportChanged)
    Q_PROPERTY(QString localAddress READ localAddress WRITE setLocalAddress NOTIFY localAddressChanged)
    Q_PROPERTY(int localPort READ localPort WRITE setLocalPort NOTIFY localPortChanged)
    Q_PROPERTY(QString remoteAddress READ remoteAddress WRITE setRemoteAddress NOTIFY remoteAddressChanged)
    Q_PROPERTY(int remotePort READ remotePort WRITE setRemotePort NOTIFY remotePortChanged)
    Q_PROPERTY(int requestIntervalMs READ requestIntervalMs WRITE setRequestIntervalMs NOTIFY requestIntervalChanged)
    Q_PROPERTY(bool heartbeatEnabled READ heartbeatEnabled WRITE setHeartbeatEnabled NOTIFY heartbeatEnabledChanged)
    Q_PROPERTY(bool autoCollect READ autoCollect WRITE setAutoCollect NOTIFY autoCollectChanged)
    Q_PROPERTY(bool autoSave READ autoSave WRITE setAutoSave NOTIFY autoSaveChanged)

public:
    enum Mode { Scalar = 0, Vector = 1 };
    enum Transport { Udp = 0, TcpClient = 1, TcpServer = 2 };
    Q_ENUM(Mode)
    Q_ENUM(Transport)

    static MagSurveySettings* instance();
    static MagSurveySettings* create(QQmlEngine*, QJSEngine*)
    {
        MagSurveySettings* settings = instance();
        QQmlEngine::setObjectOwnership(settings, QQmlEngine::CppOwnership);
        return settings;
    }

    int mode() const { return _mode; }
    int transport() const { return _transport; }
    QString localAddress() const { return _localAddress; }
    int localPort() const { return _localPort; }
    QString remoteAddress() const { return _remoteAddress; }
    int remotePort() const { return _remotePort; }
    int requestIntervalMs() const { return _requestIntervalMs; }
    bool heartbeatEnabled() const { return _heartbeatEnabled; }
    bool autoCollect() const { return _autoCollect; }
    bool autoSave() const { return _autoSave; }

    void setMode(int value);
    void setTransport(int value);
    void setLocalAddress(const QString& value);
    void setLocalPort(int value);
    void setRemoteAddress(const QString& value);
    void setRemotePort(int value);
    void setRequestIntervalMs(int value);
    void setHeartbeatEnabled(bool value);
    void setAutoCollect(bool value);
    void setAutoSave(bool value);
    Q_INVOKABLE void applyConfiguration(int mode, int transport,
                                        const QString& localAddress, int localPort,
                                        const QString& remoteAddress, int remotePort,
                                        int requestIntervalMs, bool heartbeatEnabled,
                                        bool autoCollect, bool autoSave);

signals:
    void modeChanged();
    void transportChanged();
    void localAddressChanged();
    void localPortChanged();
    void remoteAddressChanged();
    void remotePortChanged();
    void requestIntervalChanged();
    void heartbeatEnabledChanged();
    void autoCollectChanged();
    void autoSaveChanged();
    void configurationChanged();

private:
    explicit MagSurveySettings(QObject* parent = nullptr);
    void _load();
    void _save(const QString& key, const QVariant& value);

    QSettings _settings;
    int _mode = Scalar;
    int _transport = Udp;
    QString _localAddress = QStringLiteral("0.0.0.0");
    int _localPort = 14551;
    QString _remoteAddress = QStringLiteral("127.0.0.1");
    int _remotePort = 14551;
    int _requestIntervalMs = 1000;
    bool _heartbeatEnabled = true;
    bool _autoCollect = true;
    bool _autoSave = false;
};
