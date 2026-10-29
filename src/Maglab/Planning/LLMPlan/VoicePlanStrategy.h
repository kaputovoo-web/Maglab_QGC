/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "LLMPlanStrategy.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QPointer>

class LLMComplexItem;
class VoiceAudioRecorder;

/// Voice AI Planning — mic button → text → LLM API → populate strategy params.
/// LLM config is read from Application Settings > LLM,
/// with optional QGC_LLM_* environment variable overrides.
class VoicePlanStrategy : public LLMPlanStrategy
{
    Q_OBJECT
public:
    explicit VoicePlanStrategy(QObject* parent = nullptr);

    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    QString statusText() const { return _statusText; }

    Q_PROPERTY(QString reviewText  READ reviewText  NOTIFY reviewChanged)
    Q_PROPERTY(bool    reviewBusy  READ reviewBusy  NOTIFY reviewChanged)
    QString reviewText() const { return _reviewText; }
    bool    reviewBusy() const { return _reviewBusy; }

    QList<Fact*> facts() override { return {}; }
    void recomputeGeometry(const QGeoCoordinate& center) override;
    QList<QGeoCoordinate> path() const override { return {}; }
    QString editorQmlName() const override { return QStringLiteral("LLMVoiceEditor.qml"); }
    QJsonObject saveParams() const override { return {}; }
    bool loadParams(const QJsonObject& params) override;
    double complexDistance() const override { return 0.0; }
    int lastSequenceNumber() const override { return 0; }
    double minAMSLAltitude() const override { return 0.0; }
    double maxAMSLAltitude() const override { return 0.0; }
    QList<MissionItem*> missionItems(double relAlt, MAV_FRAME frame, QObject* parent) override;
    Fact* flightSpeed() override { return nullptr; }
    double specifiedFlightSpeed() const override { return std::numeric_limits<double>::quiet_NaN(); }
    Fact* altitudeMode() override { return nullptr; }
    Fact* altitude() override { return nullptr; }

    /// Send natural language text to the configured LLM API.
    Q_INVOKABLE void sendToLLM(const QString& text);

    /// Voice (STT route): record mic audio, transcribe via the configured STT
    /// endpoint, then feed the transcript into sendToLLM().
    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecordingAndTranscribe();
    Q_INVOKABLE bool isRecording() const;

    /// AI pre-flight safety review: send a mission-context summary to the LLM and
    /// receive a free-text risk assessment (shown in reviewText).
    Q_INVOKABLE void reviewSafety(const QString& missionContext);

    void setShell(LLMComplexItem* shell) { _shell = shell; }

signals:
    void statusChanged();
    void reviewChanged();
    /// Emitted with the recognised text after STT, for the user to review/edit
    /// before it is sent to the LLM for parsing.
    void transcriptReady(const QString& text);

private slots:
    void _onReplyFinished(QNetworkReply* reply);
    void _onSttReplyFinished(QNetworkReply* reply);
    void _onReviewReplyFinished(QNetworkReply* reply);

private:
    void _loadConfig();
    void _setStatus(const QString& msg);
    QJsonObject _buildRequestBody(const QString& text) const;
    bool _parseResponse(const QByteArray& responseBody, QJsonObject& intent);
    /// Extract the first balanced top-level {...} object from arbitrary text
    /// (string-literal aware). Returns "" if none found.
    static QString _extractFirstJsonObject(const QString& text);
    /// Fast local keyword/number extraction for common commands. Returns true
    /// and fills `intent` when confident; otherwise the caller falls back to the
    /// LLM. This is the "keyword" half of the combo parsing strategy.
    static bool _tryKeywordParse(const QString& text, QJsonObject& intent);
    void _post(const QString& text);   ///< Issue the HTTP POST (used for retries).
    void _uploadForTranscription(const QString& filePath);
    /// Pull the assistant's plain text out of an Anthropic/OpenAI-style reply.
    static QString _extractAssistantText(const QByteArray& responseBody);

    QPointer<QNetworkReply> _llmReply;
    QPointer<QNetworkReply> _sttReply;

    QString _lastText;                 ///< Last prompt, for transient-error retry.
    int     _retriesLeft = 0;          ///< Remaining automatic retries.

    QString _apiEndpoint;
    QString _modelName;
    QString _apiKey;
    QString _systemPrompt;

    // Speech-to-text (Whisper-style multipart /audio/transcriptions) config.
    QString _sttEndpoint;
    QString _sttApiKey;
    QString _sttModel;

    QNetworkAccessManager* _network       = nullptr;
    QNetworkAccessManager* _sttNetwork    = nullptr;
    QNetworkAccessManager* _reviewNetwork = nullptr;
    LLMComplexItem*        _shell         = nullptr;
    QString                _statusText;
    QString                _reviewText;
    bool                   _reviewBusy = false;

    VoiceAudioRecorder* _audioRecorder = nullptr;
};
