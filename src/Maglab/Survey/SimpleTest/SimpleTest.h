#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QString>
#include <QMetaObject>
#include <QStringListModel>
#include <QUdpSocket>
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
    Q_PROPERTY(qint64 lastSampleMs READ lastSampleMs NOTIFY dataUpdated)
    Q_PROPERTY(qint64 sampleCount READ sampleCount NOTIFY dataUpdated)
    Q_PROPERTY(int recordingElapsedSeconds READ recordingElapsedSeconds NOTIFY recordingElapsedChanged)
    Q_PROPERTY(QString recordingDirectory READ recordingDirectory NOTIFY saveFlagChanged)

    double rlon(){return lon;}
    double rlat(){return lat;}

    double scalar1 (){return _scalar1;}
    double scalar2 (){return _scalar2;}
    bool saveFlag (){return _saveFlag;}
    QString errorString() const { return _errorString; }
    bool udpListening() const { return client && client->state() == QAbstractSocket::BoundState; }
    qint64 lastSampleMs() const { return _lastSampleMs; }
    qint64 sampleCount() const { return _sampleCount; }
    int recordingElapsedSeconds() const;
    QString recordingDirectory() const;
    void setSaveFlag(bool saveFlag);
    bool requestPermission(const QString &perm);
    QStringList getAllStoragePaths();
    void save_data(QString data);
    Q_INVOKABLE     void saveStop();
    Q_INVOKABLE     void saveStart();
    Q_INVOKABLE QVariantMap magSeries(int windowSeconds, bool removeMean);


signals:
    void scalar1_changed();
    void scalar2_changed();
    void saveFlagChanged();
    void errorStringChanged();
    void udpStatusChanged();
    void dataUpdated();
    void recordingElapsedChanged();

private slots:
    void read_data();

private:
    struct MagSample {
        qint64 ms = 0;
        double s1 = qQNaN();
        double s2 = qQNaN();
    };

    QString _errorString;
    QByteArray buffer;
    std::deque<MagSample> _magSamples;
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
    double altitude;
    double posAccuracy;
    double grdClearance;
    double fluxgateX;
    double fluxgateY;
    double fluxgateZ;
    double yawAngle;
    double pitchAngle;
    double rollAngle;
    double Satellite;
    double Barometer;
    double AngularX;
    double AngularY;
    double AngularZ;
    double AccX;
    double AccY;
    double AccZ;
};
