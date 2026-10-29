#pragma once

#include <QObject>
#include <QTemporaryDir>

class QAudioInput;
class QMediaCaptureSession;
class QMediaRecorder;

/// Records a single temporary WAV; never requests permission until start().
class VoiceAudioRecorder : public QObject
{
    Q_OBJECT
public:
    explicit VoiceAudioRecorder(QObject* parent = nullptr);
    ~VoiceAudioRecorder() override;

    void start();
    void stop();
    bool isRecording() const { return _requested; }

signals:
    void statusChanged(const QString& message);
    void audioReady(const QString& path);

private:
    void _startGranted();

    QTemporaryDir _directory;
    QMediaCaptureSession* _session = nullptr;
    QAudioInput* _input = nullptr;
    QMediaRecorder* _recorder = nullptr;
    QString _path;
    bool _requested = false;
    bool _permissionPending = false;
    bool _finalizing = false;
    bool _failed = false;
};
