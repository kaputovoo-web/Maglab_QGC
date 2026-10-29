#include "VoiceAudioRecorder.h"

#include <QAudioDevice>
#include <QAudioInput>
#include <QCoreApplication>
#include <QFileInfo>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QPermissions>
#include <QUuid>
#include <QUrl>

VoiceAudioRecorder::VoiceAudioRecorder(QObject* parent)
    : QObject(parent)
{
}

VoiceAudioRecorder::~VoiceAudioRecorder()
{
    // Close the backend before QTemporaryDir removes its files.
    delete _session;
    delete _recorder;
    delete _input;
}

void VoiceAudioRecorder::start()
{
    if (_requested || _permissionPending || _finalizing) {
        return;
    }
    auto* app = QCoreApplication::instance();
    if (!app) {
        return;
    }
    const QMicrophonePermission permission;
    const auto status = app->checkPermission(permission);
    if (status == Qt::PermissionStatus::Denied) {
        emit statusChanged(tr("Microphone permission denied."));
        return;
    }
    _requested = true;
    if (status == Qt::PermissionStatus::Undetermined) {
        _permissionPending = true;
        app->requestPermission(permission, this, [this](const QPermission& result) {
            _permissionPending = false;
            // Releasing the microphone while the permission dialog is open cancels recording.
            if (!_requested) {
                return;
            }
            if (result.status() != Qt::PermissionStatus::Granted) {
                _requested = false;
                emit statusChanged(tr("Microphone permission denied."));
                return;
            }
            _startGranted();
        });
        return;
    }
    _startGranted();
}

void VoiceAudioRecorder::_startGranted()
{
    if (!_directory.isValid() || QMediaDevices::defaultAudioInput().isNull()) {
        _requested = false;
        emit statusChanged(tr("Microphone or temporary storage unavailable. Type your description instead."));
        return;
    }
    if (!_recorder) {
        _session = new QMediaCaptureSession(this);
        _input = new QAudioInput(this);
        _recorder = new QMediaRecorder(this);
        _session->setAudioInput(_input);
        _session->setRecorder(_recorder);
        connect(_recorder, &QMediaRecorder::errorOccurred, this,
                [this](QMediaRecorder::Error, const QString&) {
            _failed = true;
            _requested = false;
            _finalizing = false;
            _recorder->stop();
            emit statusChanged(tr("Audio recording failed. Type your description instead."));
        });
        connect(_recorder, &QMediaRecorder::recorderStateChanged, this,
                [this](QMediaRecorder::RecorderState state) {
            if (state != QMediaRecorder::StoppedState) {
                return;
            }
            const bool deliver = _finalizing && !_failed;
            _finalizing = false;
            _requested = false;
            if (!deliver) {
                return;
            }
            const QString actual = _recorder->actualLocation().toLocalFile();
            const QString path = actual.isEmpty() ? _path : actual;
            if (QFileInfo(path).size() <= 44) {
                emit statusChanged(tr("No audio captured. Type your description instead."));
                return;
            }
            emit audioReady(path);
        });
    }
    QMediaFormat format(QMediaFormat::Wave);
    format.setAudioCodec(QMediaFormat::AudioCodec::Wave);
    if (!format.isSupported(QMediaFormat::Encode)) {
        _requested = false;
        emit statusChanged(tr("WAV recording is not supported on this device. Type your description instead."));
        return;
    }
    _path = _directory.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".wav"));
    _failed = false;
    _recorder->setMediaFormat(format);
    _recorder->setAudioSampleRate(16000);
    _recorder->setAudioChannelCount(1);
    _recorder->setOutputLocation(QUrl::fromLocalFile(_path));
    _recorder->record();
    if (_requested) {
        emit statusChanged(tr("Listening… release to transcribe."));
    }
}

void VoiceAudioRecorder::stop()
{
    if (!_requested) {
        return;
    }
    _requested = false;
    if (_permissionPending || !_recorder) {
        return;
    }
    if (_recorder->recorderState() == QMediaRecorder::StoppedState) {
        emit statusChanged(tr("No audio captured. Type your description instead."));
        return;
    }
    _finalizing = true;
    emit statusChanged(tr("Finalizing audio…"));
    _recorder->stop();
}
