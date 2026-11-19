#include "SimpleTest.h"
#include "ScalarFrameParser.h"
#include <qdebug.h>
#include <QString>
#include <QHostAddress>
#include <QUdpSocket>
#include "DataRecorder.h"
#include "Common/MagLabPaths.h"
#include "Archive/MissionArchiveManager.h"

SimpleTest::SimpleTest()
{
    client = new QUdpSocket(this);
    _tcp = new QTcpSocket(this);
    _tcpServer = new QTcpServer(this);
    connect(client, &QUdpSocket::readyRead, this, &SimpleTest::read_data);
    connect(_tcp, &QTcpSocket::readyRead, this, &SimpleTest::read_tcp_data);
    connect(_tcp, &QTcpSocket::connected, this, &SimpleTest::_onTcpConnected);
    connect(_tcp, &QTcpSocket::disconnected, this, &SimpleTest::_onTcpDisconnected);
    connect(_tcpServer, &QTcpServer::newConnection, this, &SimpleTest::acceptTcpClient);
    connect(_tcp, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        _listening = false;
        _errorString = tr("TCP connection failed: %1").arg(_tcp->errorString());
        emit errorStringChanged();
        emit udpStatusChanged();
    });
    _recordingTimer.setInterval(1000);
    connect(&_recordingTimer, &QTimer::timeout, this, &SimpleTest::recordingElapsedChanged);
    connect(&_requestTimer, &QTimer::timeout, this, &SimpleTest::_requestData);
    _statsTimer.setInterval(1000);
    connect(&_statsTimer, &QTimer::timeout, this, &SimpleTest::_updateStatistics);
    _statsTimer.start();
    connect(MagSurveySettings::instance(), &MagSurveySettings::configurationChanged,
            this, &SimpleTest::reloadConnection);
    reloadConnection();
}

void SimpleTest::reloadConnection()
{
    if (_saveFlag) {
        _errorString = tr("Stop recording before changing the survey connection mode.");
        emit errorStringChanged();
        return;
    }
    _requestTimer.stop();
    _listening = false;
    _heartbeatCount = 0;
    _lastHeartbeatMs = 0;
    _autoSaveTriggered = false;
    emit heartbeatChanged();
    buffer.clear();
    if (client) client->abort();
    if (_tcp) _tcp->abort();
    _mode = MagSurveySettings::instance()->mode();
    _transport = MagSurveySettings::instance()->transport();
    _errorString.clear();

    if (!MagSurveySettings::instance()->autoCollect() && !_manualCollection) {
        emit surveyModeChanged();
        emit errorStringChanged();
        emit udpStatusChanged();
        return;
    }

    const QHostAddress localAddress(MagSurveySettings::instance()->localAddress());
    _tcpServer->close();
    if (_serverClient) {
        _serverClient->disconnect(this);
        _serverClient->deleteLater();
        _serverClient = nullptr;
    }
    if (_transport == MagSurveySettings::Udp) {
        // Vector probes often have a fixed destination port. Use an exclusive
        // bind so a second MagLab/probe process reports a real conflict instead
        // of silently stealing unicast datagrams.
        const auto flags = QUdpSocket::DontShareAddress;
        if (!client->bind(localAddress.isNull() ? QHostAddress::AnyIPv4 : localAddress,
                          quint16(MagSurveySettings::instance()->localPort()), flags)) {
            _errorString = tr("UDP 端口 %1 已被其他程序占用（请关闭旧版 Maglab、其他探杆软件或改为其他端口）：%2")
                    .arg(MagSurveySettings::instance()->localPort()).arg(client->errorString());
        } else {
            _listening = true;
        }
    } else if (_transport == MagSurveySettings::TcpClient) {
        // A TCP client should not bind the probe/server port locally. Binding
        // the same port is a common source of "connection refused/occupied"
        // failures and is not required by a network debug assistant client.
        _listening = false;
        _tcp->connectToHost(MagSurveySettings::instance()->remoteAddress(),
                            quint16(MagSurveySettings::instance()->remotePort()));
    } else {
        if (!_tcpServer->listen(localAddress.isNull() ? QHostAddress::AnyIPv4 : localAddress,
                                quint16(MagSurveySettings::instance()->localPort()))) {
            _errorString = tr("TCP 服务端监听失败：%1").arg(_tcpServer->errorString());
        } else {
            _listening = true;
        }
    }

    if (MagSurveySettings::instance()->heartbeatEnabled()) {
        _requestTimer.setInterval(MagSurveySettings::instance()->requestIntervalMs());
        _requestTimer.start();
    }
    emit surveyModeChanged();
    emit errorStringChanged();
    emit udpStatusChanged();
    if (MagSurveySettings::instance()->heartbeatEnabled()) {
        _requestData();
    }
}

void SimpleTest::startCollection()
{
    _manualCollection = true;
    reloadConnection();
}

void SimpleTest::stopCollection()
{
    _manualCollection = false;
    _requestTimer.stop();
    _listening = false;
    buffer.clear();
    if (client) client->abort();
    if (_tcpServer) _tcpServer->close();
    if (_serverClient) {
        _serverClient->disconnect(this);
        _serverClient->abort();
        _serverClient->deleteLater();
        _serverClient = nullptr;
    }
    if (_tcp) _tcp->abort();
    _heartbeatCount = 0;
    _lastHeartbeatMs = 0;
    emit heartbeatChanged();
    emit udpStatusChanged();
    emit errorStringChanged();
}

void SimpleTest::_requestData()
{
    if (!MagSurveySettings::instance()->heartbeatEnabled()) {
        return;
    }
    const QByteArray request("F8");
    qint64 sent = 0;
    if (_transport == MagSurveySettings::TcpClient || _transport == MagSurveySettings::TcpServer) {
        QTcpSocket* active = _transport == MagSurveySettings::TcpServer ? _serverClient : _tcp;
        if (active && active->state() == QAbstractSocket::ConnectedState) {
            sent = active->write(request);
        }
    } else {
        sent = client->writeDatagram(request,
                                     QHostAddress(MagSurveySettings::instance()->remoteAddress()),
                                     quint16(MagSurveySettings::instance()->remotePort()));
    }
    if (sent > 0) {
        ++_heartbeatCount;
        _lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();
        emit heartbeatChanged();
    }
}

void SimpleTest::acceptTcpClient()
{
    if (_transport != MagSurveySettings::TcpServer || !_tcpServer) return;
    while (_tcpServer->hasPendingConnections()) {
        QTcpSocket* socket = _tcpServer->nextPendingConnection();
        if (_serverClient) {
            _serverClient->disconnect(this);
            _serverClient->deleteLater();
        }
        _serverClient = socket;
        connect(socket, &QTcpSocket::readyRead, this, &SimpleTest::read_tcp_data);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            if (_serverClient == socket) _serverClient = nullptr;
        });
        _listening = true;
        _errorString.clear();
        emit errorStringChanged();
        emit udpStatusChanged();
        if (MagSurveySettings::instance()->heartbeatEnabled()) _requestData();
    }
}

QString SimpleTest::connectionState() const
{
    if (_transport == MagSurveySettings::TcpClient) {
        return _tcp && _tcp->state() == QAbstractSocket::ConnectedState
                ? tr("TCP 客户端已连接") : tr("TCP 客户端未连接");
    }
    if (_transport == MagSurveySettings::TcpServer) {
        if (!_tcpServer || !_tcpServer->isListening()) return tr("TCP 服务端未监听");
        return _serverClient && _serverClient->state() == QAbstractSocket::ConnectedState
                ? tr("TCP 客户端已连接")
                : tr("TCP 服务端已监听，等待客户端");
    }
    return _listening ? tr("UDP 已监听") : tr("UDP 未监听");
}

void SimpleTest::_onTcpConnected()
{
    if (_transport != MagSurveySettings::TcpClient) return;
    _listening = true;
    _errorString.clear();
    emit errorStringChanged();
    emit udpStatusChanged();
    if (MagSurveySettings::instance()->heartbeatEnabled()) _requestData();
}

void SimpleTest::_onTcpDisconnected()
{
    if (_transport != MagSurveySettings::TcpClient) return;
    _listening = false;
    emit udpStatusChanged();
}

void SimpleTest::_updateStatistics()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (_lastStatsMs > 0 && nowMs > _lastStatsMs) {
        const qint64 delta = _sampleCount - _lastStatsCount;
        _sampleRateHz = double(delta) * 1000.0 / double(nowMs - _lastStatsMs);
    } else {
        _sampleRateHz = 0.0;
    }
    _lastStatsMs = nowMs;
    _lastStatsCount = _sampleCount;
    emit statisticsChanged();
}

void SimpleTest::_maybeAutoSave()
{
    if (!_autoSaveTriggered && !_saveFlag && _listening &&
        MagSurveySettings::instance()->autoSave()) {
        // Start once per connection. A manual stop must not be immediately
        // undone by the next incoming frame.
        _autoSaveTriggered = true;
        saveStart();
    }
}

QString SimpleTest::dataDirectory() const
{
    return MagLabPaths::instance()->surveyDirForMode(_mode);
}

SimpleTest::~SimpleTest()
{
    if (_saveFlag) {
        saveStop();
    }
}

void SimpleTest::setSaveFlag(bool saveFlag)
{
    if (saveFlag) {
        saveStart();
    } else if (_saveFlag) {
        saveStop();
    }
}

//bool SimpleTest::requestPermission(const QString &perm)
//{
//    auto result = QtAndroid::checkPermission(perm);
//    if (result == QtAndroid::PermissionResult::Denied) {
//        auto r = QtAndroid::requestPermissionsSync({perm});
//        if (r[perm] == QtAndroid::PermissionResult::Denied) {
//            qWarning() << "Permission denied:" << perm;
//            return false;
//        }
//    }
//    return true;
//}

//QStringList SimpleTest::getAllStoragePaths()
//{
//    QStringList storagePaths;

//    // 1. 获取 Android 上的 context 对象
//    QAndroidJniObject activity = QtAndroid::androidActivity();
//    if (!activity.isValid()) {
//        qWarning() << "Invalid Android activity!";
//        return storagePaths;
//    }

//    // 2. 调用 getExternalFilesDirs(null)
//    QAndroidJniObject dirs = activity.callObjectMethod(
//        "getExternalFilesDirs",
//        "(Ljava/lang/String;)[Ljava/io/File;",
//        nullptr
//        );

//    if (!dirs.isValid()) {
//        qWarning() << "getExternalFilesDirs returned invalid object!";
//        return storagePaths;
//    }

//    QAndroidJniEnvironment env;
//    jobjectArray filesArray = dirs.object<jobjectArray>();
//    jsize len = env->GetArrayLength(filesArray);

//    for (jsize i = 0; i < len; ++i) {
//        jobject fileObj = env->GetObjectArrayElement(filesArray, i);
//        QAndroidJniObject file(fileObj);

//        // 调用 File.getAbsolutePath()
//        QAndroidJniObject absPath = file.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;");
//        QString path = absPath.toString();

//        if (!path.isEmpty() && !storagePaths.contains(path))
//            storagePaths << path;

//        env->DeleteLocalRef(fileObj);
//    }

//    return storagePaths;
//}

void SimpleTest::save_data(QString data)
{
    // 异步写盘：数据交给 DataRecorder 的工作线程，避免在 UDP 高频回调里做磁盘 IO
    DataRecorder::instance()->push(data);
}

void SimpleTest::read_data()
{
    while (client->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(client->pendingDatagramSize()));

        QHostAddress sender;
        quint16 senderPort;
        client->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        // 累加到缓冲区中
        buffer.append(datagram);
        // A missing newline or a noisy source must not grow memory indefinitely.
        if (buffer.size() > 65536) {
            buffer = buffer.right(4096);
        }
    }
    _processBuffer();
}

void SimpleTest::read_tcp_data()
{
    QTcpSocket* active = (_transport == MagSurveySettings::TcpServer) ? _serverClient : _tcp;
    if (!active) return;
    buffer.append(active->readAll());
    if (buffer.size() > 65536) {
        buffer = buffer.right(4096);
    }
    _processBuffer();
}

void SimpleTest::_processBuffer()
{
    // === 逐帧解析 ===
    while (true) {
        //找帧头 "UU"
        int headPos = buffer.indexOf("UU");
        if (headPos == -1) {
            // 没有头，清空无效数据
            if (buffer.size() > 1024) buffer.clear();
            return;
        }

        //查找帧尾 '\n'
        int tailPos = buffer.indexOf('\n', headPos);
        if (tailPos == -1) {
            // 没有完整帧，先等下一个UDP包再解析
            if (headPos > 0)
                buffer.remove(0, headPos);  // 清除头之前的垃圾数据
            if (buffer.size() > 4096) {
                buffer = buffer.endsWith('U') ? QByteArray("U") : QByteArray();
            }
            return;
        }

        //提取完整一帧
        QByteArray frame = buffer.mid(headPos, tailPos - headPos + 1);

        //移除已解析的部分
        buffer.remove(0, tailPos + 1);

        //将字节转字符串
        QString str = QString::fromUtf8(frame).trimmed();

        // 检查是否以 "UU" 开头
        if (!str.startsWith("UU")) {
            qDebug() << "无效帧:" << str.left(10);
                                        continue;
        }

        // === 解析数据 ===
        if (_mode == MagSurveySettings::Scalar &&
            str.split(QLatin1Char('+'), Qt::KeepEmptyParts).size() == 25) {
            // A 25-field UU+len frame is the vector protocol. Scalar frames are
            // the 29-field variant and must continue to the scalar parser.
            if (_errorString != tr("检测到矢量帧，但当前为标量模式；请切换到矢量 Vector")) {
                _errorString = tr("检测到矢量帧，但当前为标量模式；请切换到矢量 Vector");
                emit errorStringChanged();
            }
            continue;
        }
        if (_mode == MagSurveySettings::Vector) {
            VectorFrameParser::Frame vf;
            if (!VectorFrameParser::parse(frame, vf)) {
                continue;
            }
            _lastVector = vf;
            time = vf.time;
            lon = vf.lon;
            lat = vf.lat;
            altitude = vf.altitude;
            rollAngle = vf.roll;
            pitchAngle = vf.pitch;
            yawAngle = vf.yaw;
            _fg1x = vf.fg1x; _fg1y = vf.fg1y; _fg1z = vf.fg1z;
            _fg2x = vf.fg2x; _fg2y = vf.fg2y; _fg2z = vf.fg2z;
            _gyroX = vf.gyroX; _gyroY = vf.gyroY; _gyroZ = vf.gyroZ;
            _accX = vf.accX; _accY = vf.accY; _accZ = vf.accZ;
            _lidar = vf.lidar;
            _fixType = vf.fixType;
            _scalar1 = std::sqrt(_fg1x * _fg1x + _fg1y * _fg1y + _fg1z * _fg1z);
            _scalar2 = std::sqrt(_fg2x * _fg2x + _fg2y * _fg2y + _fg2z * _fg2z);
            // Scalar three-axis card can show the first gate's raw components
            // without changing the vector protocol.
            fluxgateX = _fg1x;
            fluxgateY = _fg1y;
            fluxgateZ = _fg1z;
            const QString vectorData = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23")
                    .arg(vf.time, 0, 'f')
                    .arg(vf.lon, 0, 'f', DECIMAL_PLACES)
                    .arg(vf.lat, 0, 'f', DECIMAL_PLACES)
                    .arg(vf.altitude, 0, 'f')
                    .arg(vf.fixType)
                    .arg(vf.lidar, 0, 'f')
                    .arg(vf.fg1x, 0, 'f').arg(vf.fg1y, 0, 'f').arg(vf.fg1z, 0, 'f')
                    .arg(vf.fg2x, 0, 'f').arg(vf.fg2y, 0, 'f').arg(vf.fg2z, 0, 'f')
                    .arg(vf.gyroX, 0, 'f').arg(vf.gyroY, 0, 'f').arg(vf.gyroZ, 0, 'f')
                    .arg(vf.accX, 0, 'f').arg(vf.accY, 0, 'f').arg(vf.accZ, 0, 'f')
                    .arg(vf.roll, 0, 'f').arg(vf.pitch, 0, 'f').arg(vf.yaw, 0, 'f')
                    .arg(vf.count)
                    .arg(vf.crc);
            _maybeAutoSave();
            if (_saveFlag) {
                save_data(vectorData);
            }
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            _vectorSamples.push_back({nowMs, vf.fg1x, vf.fg1y, vf.fg1z,
                                      vf.fg2x, vf.fg2y, vf.fg2z});
            ++_vectorRevision;
            const qint64 vectorOldestMs = nowMs - 105000;
            while (!_vectorSamples.empty() &&
                   (_vectorSamples.front().ms < vectorOldestMs || _vectorSamples.size() > 50000)) {
                _vectorSamples.pop_front();
            }
            _appendVectorSample(vf);
            emit scalar1_changed();
            emit scalar2_changed();
            emit vectorChanged();
            continue;
        }
        ScalarFrameParser::Frame scalarFrame;
        if (!ScalarFrameParser::parse(frame, scalarFrame)) {
            continue;
        }
        // UU + len + time/lon/lat/alt/fix/lidar +
        // op1/op2 + mx/my/mz + gyroXYZ + accXYZ + roll/pitch/yaw + count + CRC
        time = scalarFrame.time;
        lon = scalarFrame.lon;
        lat = scalarFrame.lat;
        altitude = scalarFrame.altitude;
        posAccuracy = scalarFrame.fixType;
        grdClearance = scalarFrame.lidar;
        const double scalar = scalarFrame.op1;
        const double scalar2 = scalarFrame.op2;
        fluxgateX = scalarFrame.mx;
        fluxgateY = scalarFrame.my;
        fluxgateZ = scalarFrame.mz;
        rollAngle = scalarFrame.roll;
        pitchAngle = scalarFrame.pitch;
        yawAngle = scalarFrame.yaw;
        Satellite = scalarFrame.satellites;
        Barometer = scalarFrame.barometer;
        Column1 = scalarFrame.column1;
        Column2 = scalarFrame.column2;
        Column3 = scalarFrame.column3;
        AngularX = scalarFrame.gyroX;
        AngularY = scalarFrame.gyroY;
        AngularZ = scalarFrame.gyroZ;
        AccX = scalarFrame.accX;
        AccY = scalarFrame.accY;
        AccZ = scalarFrame.accZ;

        // 更新信号值
        _scalar1 = scalar;
        _scalar2 = scalar2;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        _magSamples.push_back({nowMs, scalar, scalar2});
        _vectorSamples.push_back({nowMs, fluxgateX, fluxgateY, fluxgateZ,
                                  fluxgateX, fluxgateY, fluxgateZ});
        _lastSampleMs = nowMs;
        ++_sampleCount;
        ++_sampleRevision;
        ++_vectorRevision;
        const qint64 vectorOldestMs = nowMs - 105000;
        while (!_vectorSamples.empty() &&
               (_vectorSamples.front().ms < vectorOldestMs || _vectorSamples.size() > 50000)) {
            _vectorSamples.pop_front();
        }
        emit dataUpdated();
        const qint64 oldestMs = nowMs - 105000;
        while (!_magSamples.empty() && (_magSamples.front().ms < oldestMs || _magSamples.size() > 50000)) {
            _magSamples.pop_front();
        }

        QString fullData = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25")
                               .arg(time, 0, 'f')
                               .arg(lon, 0, 'f', DECIMAL_PLACES)
                               .arg(lat, 0, 'f', DECIMAL_PLACES)
                               .arg(altitude, 0, 'f')
                               .arg(posAccuracy, 0, 'f')
                               .arg(grdClearance, 0, 'f')
                               .arg(_scalar1, 0, 'f')
                               .arg(_scalar2, 0, 'f')
                               .arg(fluxgateX, 0, 'f')
                               .arg(fluxgateY, 0, 'f')
                               .arg(fluxgateZ, 0, 'f')
                               .arg(rollAngle, 0, 'f')
                               .arg(pitchAngle, 0, 'f')
                               .arg(yawAngle, 0, 'f')
                               .arg(Satellite, 0, 'f')
                               .arg(Barometer, 0, 'f')
                               .arg(Column1, 0, 'f')
                               .arg(Column2, 0, 'f')
                               .arg(Column3, 0, 'f')
                               .arg(AngularX, 0, 'f')
                               .arg(AngularY, 0, 'f')
                               .arg(AngularZ, 0, 'f')
                               .arg(AccX, 0, 'f')
                               .arg(AccY, 0, 'f')
                               .arg(AccZ, 0, 'f');


        _maybeAutoSave();
        if(_saveFlag){
            save_data(fullData);
        }

        emit scalar1_changed();
        emit scalar2_changed();
        emit vectorChanged();
        // ✅ 打印验证
        // qDebug() << "解析到帧: id=" << id << " lat=" << lat << " lon=" << lon << " val=" << scalar;
    }
}

QVariantMap SimpleTest::magSeries(int windowSeconds, bool removeMean)
{
    const int window = qBound(1, windowSeconds, 300);
    if (_seriesCacheWindow == window &&
        _seriesCacheRemoveMean == removeMean &&
        _seriesCacheRevision == _sampleRevision) {
        return _seriesCache;
    }
    QVariantMap result;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 fromMs = nowMs - qint64(window) * 1000;

    const int total = static_cast<int>(_magSamples.size());
    int first = 0;
    while (first < total && _magSamples.at(first).ms < fromMs) {
        ++first;
    }
    const int count = total - first;
    if (count <= 0) {
        result[QStringLiteral("count")] = 0;
        result[QStringLiteral("s1")] = QVariantList();
        result[QStringLiteral("s2")] = QVariantList();
        result[QStringLiteral("diff")] = QVariantList();
        result[QStringLiteral("mean1")] = 0.0;
        result[QStringLiteral("mean2")] = 0.0;
        result[QStringLiteral("meanDiff")] = 0.0;
        _seriesCache = result;
        _seriesCacheWindow = window;
        _seriesCacheRemoveMean = removeMean;
        _seriesCacheRevision = _sampleRevision;
        return _seriesCache;
    }

    const int maxPoints = 400;
    const int stride = count > maxPoints ? (count + maxPoints - 1) / maxPoints : 1;
    QVariantList s1;
    QVariantList s2;
    QVariantList diff;
    s1.reserve(count / stride + 1);
    s2.reserve(count / stride + 1);
    diff.reserve(count / stride + 1);

    double sum1 = 0.0;
    double sum2 = 0.0;
    for (int i = first; i < total; ++i) {
        sum1 += _magSamples.at(i).s1;
        sum2 += _magSamples.at(i).s2;
    }
    const double mean1 = sum1 / count;
    const double mean2 = sum2 / count;
    const double meanDiff = mean1 - mean2;

    for (int i = first; i < total; i += stride) {
        const int end = qMin(i + stride, total);
        double bin1 = 0.0;
        double bin2 = 0.0;
        int binCount = 0;
        for (int k = i; k < end; ++k) {
            bin1 += _magSamples.at(k).s1;
            bin2 += _magSamples.at(k).s2;
            ++binCount;
        }
        if (binCount <= 0) {
            continue;
        }
        bin1 /= binCount;
        bin2 /= binCount;
        const double offset1 = removeMean ? mean1 : 0.0;
        const double offset2 = removeMean ? mean2 : 0.0;
        s1.append(bin1 - offset1);
        s2.append(bin2 - offset2);
        diff.append((bin1 - bin2) - (removeMean ? meanDiff : 0.0));
    }

    result[QStringLiteral("count")] = count;
    result[QStringLiteral("s1")] = s1;
    result[QStringLiteral("s2")] = s2;
    result[QStringLiteral("diff")] = diff;
    result[QStringLiteral("mean1")] = mean1;
    result[QStringLiteral("mean2")] = mean2;
    result[QStringLiteral("meanDiff")] = meanDiff;
    _seriesCache = result;
    _seriesCacheWindow = window;
    _seriesCacheRemoveMean = removeMean;
    _seriesCacheRevision = _sampleRevision;
    return _seriesCache;
}

QVariantMap SimpleTest::magVectorSeries(int windowSeconds, bool removeMean)
{
    const int window = qBound(1, windowSeconds, 300);
    if (_vectorSeriesCacheWindow == window &&
        _vectorSeriesCacheRemoveMean == removeMean &&
        _vectorSeriesCacheRevision == _vectorRevision) {
        return _vectorSeriesCache;
    }

    QVariantMap result;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 fromMs = nowMs - qint64(window) * 1000;
    const int total = int(_vectorSamples.size());
    int first = 0;
    while (first < total && _vectorSamples.at(first).ms < fromMs) {
        ++first;
    }
    const int count = total - first;
    QVariantList s1x, s1y, s1z, s2x, s2y, s2z, dx, dy, dz;
    if (count > 0) {
        const int maxPoints = 400;
        const int stride = count > maxPoints ? (count + maxPoints - 1) / maxPoints : 1;
        QVariantList* lists[] = {&s1x, &s1y, &s1z, &s2x, &s2y, &s2z, &dx, &dy, &dz};
        for (QVariantList* list : lists) {
            list->reserve(count / stride + 1);
        }
        double mean[9] = {};
        for (int i = first; i < total; ++i) {
            const VectorSample& v = _vectorSamples.at(i);
            mean[0] += v.s1x; mean[1] += v.s1y; mean[2] += v.s1z;
            mean[3] += v.s2x; mean[4] += v.s2y; mean[5] += v.s2z;
            mean[6] += v.s1x - v.s2x; mean[7] += v.s1y - v.s2y; mean[8] += v.s1z - v.s2z;
        }
        for (double& value : mean) value /= count;
        for (int i = first; i < total; i += stride) {
            const int end = qMin(i + stride, total);
            double sum[9] = {};
            int bins = 0;
            for (int k = i; k < end; ++k) {
                const VectorSample& v = _vectorSamples.at(k);
                sum[0] += v.s1x; sum[1] += v.s1y; sum[2] += v.s1z;
                sum[3] += v.s2x; sum[4] += v.s2y; sum[5] += v.s2z;
                sum[6] += v.s1x - v.s2x; sum[7] += v.s1y - v.s2y; sum[8] += v.s1z - v.s2z;
                ++bins;
            }
            if (bins <= 0) continue;
            for (int c = 0; c < 9; ++c) {
                sum[c] /= bins;
                if (removeMean) sum[c] -= mean[c];
                lists[c]->append(sum[c]);
            }
        }
    }
    result[QStringLiteral("count")] = count;
    result[QStringLiteral("s1x")] = s1x; result[QStringLiteral("s1y")] = s1y; result[QStringLiteral("s1z")] = s1z;
    result[QStringLiteral("s2x")] = s2x; result[QStringLiteral("s2y")] = s2y; result[QStringLiteral("s2z")] = s2z;
    result[QStringLiteral("dx")] = dx; result[QStringLiteral("dy")] = dy; result[QStringLiteral("dz")] = dz;
    _vectorSeriesCache = result;
    _vectorSeriesCacheWindow = window;
    _vectorSeriesCacheRemoveMean = removeMean;
    _vectorSeriesCacheRevision = _vectorRevision;
    return _vectorSeriesCache;
}

void SimpleTest::_appendVectorSample(const VectorFrameParser::Frame& frame)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    _magSamples.push_back({nowMs, _scalar1, _scalar2});
    _lastSampleMs = nowMs;
    ++_sampleCount;
    ++_sampleRevision;
    const qint64 oldestMs = nowMs - 105000;
    while (!_magSamples.empty() && (_magSamples.front().ms < oldestMs || _magSamples.size() > 50000)) {
        _magSamples.pop_front();
    }
    Q_UNUSED(frame)
    emit dataUpdated();
}

void SimpleTest::saveStop()
{
    DataRecorder::instance()->stopRecord();
    _recordingTimer.stop();
    _saveFlag = false;
    _recordingStartMs = 0;
    emit saveFlagChanged();
    emit recordingElapsedChanged();
    // A settings change made while recording was rejected; apply it once the
    // recorder has safely stopped instead of leaving the old transport active.
    if (_mode != MagSurveySettings::instance()->mode() ||
        _transport != MagSurveySettings::instance()->transport()) {
        reloadConnection();
    }
}

int SimpleTest::recordingElapsedSeconds() const
{
    if (!_saveFlag || _recordingStartMs <= 0) {
        return 0;
    }
    return int((QDateTime::currentMSecsSinceEpoch() - _recordingStartMs) / 1000);
}

QString SimpleTest::recordingDirectory() const
{
    return MagLabPaths::instance()->surveyDirForMode(_mode);
}

void SimpleTest::saveStart()
{
    // 异步写盘：启动 DataRecorder 工作线程，写入表头；后续数据由 save_data 推入队列。
    // 文件名由 DataRecorder 按时间戳生成，不再手动拼接 + Append，避免同秒重复写表头。
    if (_saveFlag) {
        return;
    }
    _errorString.clear();
    if (!_listening) {
        _errorString = tr("Sensor listener is unavailable.");
        emit errorStringChanged();
        return;
    }
    const QString directory = MagLabPaths::instance()->surveyDirForMode(_mode);
    if (directory.isEmpty()) {
        _errorString = tr("Survey storage is unavailable.");
        emit errorStringChanged();
        return;
    }
    DataRecorder::instance()->startRecord(directory, MissionArchiveManager::instance()->currentSessionName());
    if (!DataRecorder::instance()->isRecording()) {
        _errorString = tr("Unable to start the survey recorder.");
        emit errorStringChanged();
        return;
    }
    if (_mode == MagSurveySettings::Vector) {
        DataRecorder::instance()->push(QString::fromUtf8("时间(HHMMSS.ms),经度(°),纬度(°),海拔高度(m),定位类型,激光雷达高度(m),磁通门1_X(nT),磁通门1_Y(nT),磁通门1_Z(nT),磁通门2_X(nT),磁通门2_Y(nT),磁通门2_Z(nT),陀螺仪X(°/s),陀螺仪Y(°/s),陀螺仪Z(°/s),加速度X(m/s²),加速度Y(m/s²),加速度Z(m/s²),翻滚角(°),俯仰角(°),偏航角(°),计数,CRC16"));
    } else {
        DataRecorder::instance()->push(QString::fromUtf8("时间(HHMMSS.ms),经度(°),纬度(°),海拔高度(m),定位类型,激光雷达高度(m),光泵1(nT),光泵2(nT),磁通门X(nT),磁通门Y(nT),磁通门Z(nT),翻滚角(°),俯仰角(°),偏航角(°),卫星数,气压高度(m),column1,column2,column3,陀螺仪X(°/s),陀螺仪Y(°/s),陀螺仪Z(°/s),加速度X(m/s²),加速度Y(m/s²),加速度Z(m/s²)"));
    }

    _saveFlag = true;
    _recordingStartMs = QDateTime::currentMSecsSinceEpoch();
    _recordingTimer.start();
    emit errorStringChanged();
    emit saveFlagChanged();
    emit recordingElapsedChanged();
}
