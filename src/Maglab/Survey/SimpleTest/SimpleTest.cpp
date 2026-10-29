#include "SimpleTest.h"
#include "ScalarFrameParser.h"
#include <qdebug.h>
#include <QString>
#include <QHostAddress>
#include <QUdpSocket>
#include "DataRecorder.h"
#include "Common/MagLabPaths.h"

SimpleTest::SimpleTest()
{
    client = new QUdpSocket(this);
    // 标量航磁系统：监听 14551 接收标量数据（scalar1/scalar2 直接读数）
    if (!client->bind(QHostAddress::AnyIPv4, 14551)) {
        _errorString = tr("Unable to listen on UDP port 14551: %1").arg(client->errorString());
        qWarning() << _errorString;
    }
    connect(client, &QUdpSocket::readyRead, this, &SimpleTest::read_data);
    _recordingTimer.setInterval(1000);
    connect(&_recordingTimer, &QTimer::timeout, this, &SimpleTest::recordingElapsedChanged);
    emit udpStatusChanged();
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

        // ✅ 打印调试信息（可注释掉）
        // qDebug() << "UDP包来自:" << sender.toString() << ":" << senderPort
        //          << "大小:" << datagram.size();

        // 累加到缓冲区中
        buffer.append(datagram);
        // A missing newline or a noisy source must not grow memory indefinitely.
        if (buffer.size() > 65536) {
            buffer = buffer.right(4096);
        }
    }

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
        std::array<double, 22> parsed;
        if (!ScalarFrameParser::parse(frame, parsed)) {
            continue;
        }
        // 标量帧格式（maglab 标量航磁系统）：
        // UU+id+time+lon+lat+altitude+posAccuracy+grdClearance+
        //    scalar1(光泵1)+scalar2(光泵2)+fluxgateX/Y/Z+
        //    roll+pitch+yaw+Satellite+Barometer+
        //    AngularX/Y/Z+AccX/Y/Z
        time = parsed[0];
        lon = parsed[1];
        lat = parsed[2];
        altitude = parsed[3];
        posAccuracy = parsed[4];
        grdClearance = parsed[5];
        const double scalar = parsed[6];
        const double scalar2 = parsed[7];
        fluxgateX = parsed[8];
        fluxgateY = parsed[9];
        fluxgateZ = parsed[10];
        rollAngle = parsed[11];
        pitchAngle = parsed[12];
        yawAngle = parsed[13];
        Satellite = parsed[14];
        Barometer = parsed[15];
        AngularX = parsed[16];
        AngularY = parsed[17];
        AngularZ = parsed[18];
        AccX = parsed[19];
        AccY = parsed[20];
        AccZ = parsed[21];

        // 更新信号值
        _scalar1 = scalar;
        _scalar2 = scalar2;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        _magSamples.push_back({nowMs, scalar, scalar2});
        _lastSampleMs = nowMs;
        ++_sampleCount;
        emit dataUpdated();
        const qint64 oldestMs = nowMs - 105000;
        while (!_magSamples.empty() && (_magSamples.front().ms < oldestMs || _magSamples.size() > 50000)) {
            _magSamples.pop_front();
        }

        QString fullData = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20")
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
                               .arg(AngularX, 0, 'f')
                               .arg(AngularY, 0, 'f')
                               .arg(AngularZ, 0, 'f')
                               .arg(AccX, 0, 'f')
                               .arg(AccY, 0, 'f')
                               .arg(AccZ, 0, 'f');


        if(_saveFlag){
            save_data(fullData);
        }

        emit scalar1_changed();
        emit scalar2_changed();
        // ✅ 打印验证
        // qDebug() << "解析到帧: id=" << id << " lat=" << lat << " lon=" << lon << " val=" << scalar;
    }
}

QVariantMap SimpleTest::magSeries(int windowSeconds, bool removeMean)
{
    QVariantMap result;
    const int window = qBound(1, windowSeconds, 300);
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
        return result;
    }

    const int maxPoints = 600;
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
    return result;
}

void SimpleTest::saveStop()
{
    DataRecorder::instance()->stopRecord();
    _recordingTimer.stop();
    _saveFlag = false;
    _recordingStartMs = 0;
    emit saveFlagChanged();
    emit recordingElapsedChanged();
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
    return MagLabPaths::instance()->surveyDir();
}

void SimpleTest::saveStart()
{
    // 异步写盘：启动 DataRecorder 工作线程，写入表头；后续数据由 save_data 推入队列。
    // 文件名由 DataRecorder 按时间戳生成，不再手动拼接 + Append，避免同秒重复写表头。
    if (_saveFlag) {
        return;
    }
    _errorString.clear();
    if (client->state() != QAbstractSocket::BoundState) {
        _errorString = tr("Sensor UDP listener is unavailable.");
        emit errorStringChanged();
        return;
    }
    const QString directory = MagLabPaths::instance()->surveyDir();
    if (directory.isEmpty()) {
        _errorString = tr("Survey storage is unavailable.");
        emit errorStringChanged();
        return;
    }
    DataRecorder::instance()->startRecord(directory);
    if (!DataRecorder::instance()->isRecording()) {
        _errorString = tr("Unable to start the survey recorder.");
        emit errorStringChanged();
        return;
    }
    DataRecorder::instance()->push(QString::fromUtf8("时间(HHMMSS.ms),经度(°),纬度(°),海拔高度(m),定位精度,离地高度(m),1号磁传感器的磁场数据(nT),2号磁传感器的磁场数据(nT),磁补偿器X方向的磁场数据(nT),磁补偿器Y方向的磁场数据(nT),磁补偿器Z方向的磁场数据(nT),翻滚角(°),仰俯角(°),航偏角(°),角速度X(°/s),角速度Y(°/s),角速度Z(°/s),加速度X(m/s²),加速度Y(m/s²),加速度Z(m/s²)"));

    _saveFlag = true;
    _recordingStartMs = QDateTime::currentMSecsSinceEpoch();
    _recordingTimer.start();
    emit errorStringChanged();
    emit saveFlagChanged();
    emit recordingElapsedChanged();
}
