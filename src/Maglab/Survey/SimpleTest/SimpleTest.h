#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QString>
#include <QMetaObject>
#include <QStringListModel>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QTcpServer>
#include <QTimer>

#include "MagSurveySettings.h"
#include "Vector/VectorFrameParser.h"
#include <QAbstractSocket>
#include <QTimer>
#include <QVector>
#include <deque>
#include <QVariantMap>

#include <QCoreApplication>
#include <QDebug>
//#include <QtAndroid>
//#include <QAndroidJniObject>
//#include <QAndroidJniEnvironment>
#include <QFile>

#include <QElapsedTimer>
#include <QDateTime>
#include <QTimer>
#include <QString>
#include <QHostAddress>
#include <QStandardPaths>
#include <QFileInfo>
#include <qdir.h>

// Fordward decls
class Vehicle;

class SimpleTest : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    SimpleTest();
    virtual ~SimpleTest();

    QUdpSocket* client;

    Q_PROPERTY(double    scalar1       READ scalar1   NOTIFY scalar1_changed)
    Q_PROPERTY(double    scalar2       READ scalar2   NOTIFY scalar2_changed)
    Q_PROPERTY(bool saveFlag READ saveFlag WRITE setSaveFlag NOTIFY saveFlagChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(bool udpListening READ udpListening NOTIFY udpStatusChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY udpStatusChanged)
    Q_PROPERTY(qint64 lastSampleMs READ lastSampleMs NOTIFY dataUpdated)
    Q_PROPERTY(qint64 sampleCount READ sampleCount NOTIFY statisticsChanged)
    Q_PROPERTY(qint64 packetCount READ packetCount NOTIFY statisticsChanged)
    Q_PROPERTY(double sampleRateHz READ sampleRateHz NOTIFY statisticsChanged)
    Q_PROPERTY(int heartbeatCount READ heartbeatCount NOTIFY heartbeatChanged)
    Q_PROPERTY(qint64 lastHeartbeatMs READ lastHeartbeatMs NOTIFY heartbeatChanged)
    Q_PROPERTY(int recordingElapsedSeconds READ recordingElapsedSeconds NOTIFY recordingElapsedChanged)
    Q_PROPERTY(QString recordingDirectory READ recordingDirectory NOTIFY saveFlagChanged)
    Q_PROPERTY(int surveyMode READ surveyMode NOTIFY surveyModeChanged)
    Q_PROPERTY(QString dataDirectory READ dataDirectory NOTIFY surveyModeChanged)
    Q_PROPERTY(double fg1x READ fg1x NOTIFY vectorChanged)
    Q_PROPERTY(double fg1y READ fg1y NOTIFY vectorChanged)
    Q_PROPERTY(double fg1z READ fg1z NOTIFY vectorChanged)
    Q_PROPERTY(double fg2x READ fg2x NOTIFY vectorChanged)
    Q_PROPERTY(double fg2y READ fg2y NOTIFY vectorChanged)
    Q_PROPERTY(double fg2z READ fg2z NOTIFY vectorChanged)
    Q_PROPERTY(double gyroX READ gyroX NOTIFY vectorChanged)
    Q_PROPERTY(double gyroY READ gyroY NOTIFY vectorChanged)
    Q_PROPERTY(double gyroZ READ gyroZ NOTIFY vectorChanged)
    Q_PROPERTY(double accX READ accX NOTIFY vectorChanged)
    Q_PROPERTY(double accY READ accY NOTIFY vectorChanged)
    Q_PROPERTY(double accZ READ accZ NOTIFY vectorChanged)
    Q_PROPERTY(double lidar READ lidar NOTIFY vectorChanged)
    Q_PROPERTY(int fixType READ fixType NOTIFY vectorChanged)
    Q_PROPERTY(double axisX READ axisX NOTIFY vectorChanged)
    Q_PROPERTY(double axisY READ axisY NOTIFY vectorChanged)
    Q_PROPERTY(double axisZ READ axisZ NOTIFY vectorChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY dataUpdated)
    Q_PROPERTY(double longitude READ longitude NOTIFY dataUpdated)
    Q_PROPERTY(double altitude READ altitudeValue NOTIFY dataUpdated)
    Q_PROPERTY(double roll READ rollValue NOTIFY dataUpdated)
    Q_PROPERTY(double pitch READ pitchValue NOTIFY dataUpdated)
    Q_PROPERTY(double yaw READ yawValue NOTIFY dataUpdated)

    double rlon(){return lon;}
    double rlat(){return lat;}

    double scalar1 (){return _scalar1;}
    double scalar2 (){return _scalar2;}
    bool saveFlag (){return _saveFlag;}
    QString errorString() const { return _errorString; }
    bool udpListening() const { return _listening; }
    QString connectionState() const;
    qint64 lastSampleMs() const { return _lastSampleMs; }
    qint64 sampleCount() const { return _sampleCount; }
    qint64 packetCount() const { return _sampleCount; }
    double sampleRateHz() const { return _sampleRateHz; }
    int heartbeatCount() const { return _heartbeatCount; }
    qint64 lastHeartbeatMs() const { return _lastHeartbeatMs; }
    int recordingElapsedSeconds() const;
    QString recordingDirectory() const;
    int surveyMode() const { return _mode; }
    QString dataDirectory() const;
    double fg1x() const { return _fg1x; }
    double fg1y() const { return _fg1y; }
    double fg1z() const { return _fg1z; }
    double fg2x() const { return _fg2x; }
    double fg2y() const { return _fg2y; }
    double fg2z() const { return _fg2z; }
    double gyroX() const { return _gyroX; }
    double gyroY() const { return _gyroY; }
    double gyroZ() const { return _gyroZ; }
    double accX() const { return _accX; }
    double accY() const { return _accY; }
    double accZ() const { return _accZ; }
    double lidar() const { return _lidar; }
    int fixType() const { return _fixType; }
    double axisX() const { return fluxgateX; }
    double axisY() const { return fluxgateY; }
    double axisZ() const { return fluxgateZ; }
    double latitude() const { return lat; }
    double longitude() const { return lon; }
    double altitudeValue() const { return altitude; }
    double rollValue() const { return rollAngle; }
    double pitchValue() const { return pitchAngle; }
    double yawValue() const { return yawAngle; }
    Q_INVOKABLE void reloadConnection();
    Q_INVOKABLE void startCollection();
    Q_INVOKABLE void stopCollection();
    Q_INVOKABLE void acceptTcpClient();
    void setSaveFlag(bool saveFlag);
    bool requestPermission(const QString &perm);
    QStringList getAllStoragePaths();
    void save_data(QString data);
    Q_INVOKABLE     void saveStop();
    Q_INVOKABLE     void saveStart();
    Q_INVOKABLE QVariantMap magSeries(int windowSeconds, bool removeMean);
    Q_INVOKABLE QVariantMap magVectorSeries(int windowSeconds, bool removeMean);


signals:
    void scalar1_changed();
    void scalar2_changed();
    void saveFlagChanged();
    void errorStringChanged();
    void udpStatusChanged();
    void dataUpdated();
    void recordingElapsedChanged();
    void surveyModeChanged();
    void vectorChanged();
    void heartbeatChanged();
    void statisticsChanged();

private slots:
    void read_data();
    void read_tcp_data();
    void _requestData();
    void _onTcpConnected();
    void _onTcpDisconnected();

private:
    struct MagSample {
        qint64 ms = 0;
        double s1 = qQNaN();
        double s2 = qQNaN();
    };
    struct VectorSample {
        qint64 ms = 0;
        double s1x = qQNaN(), s1y = qQNaN(), s1z = qQNaN();
        double s2x = qQNaN(), s2y = qQNaN(), s2z = qQNaN();
    };

    void _processBuffer();
    void _handleFrame(const QByteArray& frame);
    void _appendVectorSample(const VectorFrameParser::Frame& frame);
    void _updateStatistics();
    void _maybeAutoSave();

    QString _errorString;
    QByteArray buffer;
    QTcpSocket* _tcp = nullptr;
    QTcpServer* _tcpServer = nullptr;
    QTcpSocket* _serverClient = nullptr;
    QTimer _requestTimer;
    QTimer _statsTimer;
    bool _manualCollection = false;
    bool _autoSaveTriggered = false;
    int _heartbeatCount = 0;
    qint64 _lastHeartbeatMs = 0;
    double _sampleRateHz = 0.0;
    qint64 _lastStatsCount = 0;
    qint64 _lastStatsMs = 0;
    int _mode = 0;
    int _transport = 0;
    bool _listening = false;
    VectorFrameParser::Frame _lastVector;
    double _fg1x = qQNaN(), _fg1y = qQNaN(), _fg1z = qQNaN();
    double _fg2x = qQNaN(), _fg2y = qQNaN(), _fg2z = qQNaN();
    double _gyroX = qQNaN(), _gyroY = qQNaN(), _gyroZ = qQNaN();
    double _accX = qQNaN(), _accY = qQNaN(), _accZ = qQNaN();
    double _lidar = qQNaN();
    int _fixType = 0;
    std::deque<MagSample> _magSamples;
    std::deque<VectorSample> _vectorSamples;
    quint64 _sampleRevision = 0;
    quint64 _vectorRevision = 0;
    mutable QVariantMap _seriesCache;
    mutable quint64 _seriesCacheRevision = 0;
    mutable int _seriesCacheWindow = -1;
    mutable bool _seriesCacheRemoveMean = false;
    mutable QVariantMap _vectorSeriesCache;
    mutable quint64 _vectorSeriesCacheRevision = 0;
    mutable int _vectorSeriesCacheWindow = -1;
    mutable bool _vectorSeriesCacheRemoveMean = false;
    qint64 _lastSampleMs = 0;
    qint64 _sampleCount = 0;
    qint64 _recordingStartMs = 0;
    QTimer _recordingTimer;
    double _scalar1 = qQNaN();    // 光泵1（标量磁场读数）
    double _scalar2 = qQNaN();    // 光泵2（标量磁场读数）
    QStringList paths;
    bool _saveFlag = false;

    const int DECIMAL_PLACES = 7;
    double time;
    double lon = qQNaN();
    double lat = qQNaN();
    double altitude = qQNaN();
    double posAccuracy;
    double grdClearance;
    double fluxgateX;
    double fluxgateY;
    double fluxgateZ;
    double yawAngle = qQNaN();
    double pitchAngle = qQNaN();
    double rollAngle = qQNaN();
    double Satellite = 0;
    double Barometer = qQNaN();
    double Column1 = qQNaN();
    double Column2 = qQNaN();
    double Column3 = qQNaN();
    double AngularX;
    double AngularY;
    double AngularZ;
    double AccX;
    double AccY;
    double AccZ;
};
