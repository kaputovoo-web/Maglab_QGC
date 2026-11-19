/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VoicePlanStrategy.h"
#include "LLMComplexItem.h"
#include "Settings/LLMSettings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

#include <QHttpMultiPart>
#include <QHttpPart>
#include "VoiceAudioRecorder.h"
#include <QStandardPaths>
#include <QTimer>
#include <QRegularExpression>
#include <limits>


VoicePlanStrategy::VoicePlanStrategy(QObject* parent)
    : LLMPlanStrategy(parent)
{
    _loadConfig();

    _audioRecorder = new VoiceAudioRecorder(this);
    connect(_audioRecorder, &VoiceAudioRecorder::statusChanged, this, &VoicePlanStrategy::_setStatus);
    connect(_audioRecorder, &VoiceAudioRecorder::audioReady, this, [this](const QString& path) {
        _setStatus(tr("Transcribing…"));
        _uploadForTranscription(path);
    });

    _network = new QNetworkAccessManager(this);
    connect(_network, &QNetworkAccessManager::finished, this, &VoicePlanStrategy::_onReplyFinished);

    _sttNetwork = new QNetworkAccessManager(this);
    connect(_sttNetwork, &QNetworkAccessManager::finished, this, &VoicePlanStrategy::_onSttReplyFinished);

    _reviewNetwork = new QNetworkAccessManager(this);
    connect(_reviewNetwork, &QNetworkAccessManager::finished, this, &VoicePlanStrategy::_onReviewReplyFinished);

    _setStatus(tr("Ready. Hold the microphone to speak, or type in the text field."));
}

void VoicePlanStrategy::_loadConfig()
{
    // Environment overrides persisted Facts; defaults are used only when empty.
    LLMSettings settings;
    LLMSettings* s = &settings;

    _apiEndpoint   = s->apiEndpoint()->rawValue().toString();
    _modelName     = s->modelName()->rawValue().toString();
    _apiKey        = s->apiKey()->rawValue().toString();
    _systemPrompt  = s->systemPrompt()->rawValue().toString();
    _sttEndpoint   = s->sttEndpoint()->rawValue().toString();
    _sttModel      = s->sttModel()->rawValue().toString();
    _sttApiKey     = s->sttApiKey()->rawValue().toString();

    // Environment overrides take precedence
    const QByteArray envKey = qgetenv("QGC_LLM_API_KEY");
    if (!envKey.isEmpty()) { _apiKey = QString::fromUtf8(envKey); }
    const QByteArray envEndpoint = qgetenv("QGC_LLM_API_ENDPOINT");
    if (!envEndpoint.isEmpty()) { _apiEndpoint = QString::fromUtf8(envEndpoint); }
    const QByteArray envSttEndpoint = qgetenv("QGC_STT_ENDPOINT");
    if (!envSttEndpoint.isEmpty()) { _sttEndpoint = QString::fromUtf8(envSttEndpoint); }
    const QByteArray envSttKey = qgetenv("QGC_STT_API_KEY");
    if (!envSttKey.isEmpty()) { _sttApiKey = QString::fromUtf8(envSttKey); }
    const QByteArray envModel = qgetenv("QGC_LLM_MODEL");
    if (!envModel.isEmpty())  { _modelName = QString::fromUtf8(envModel); }

    // Fallback defaults
    if (_apiEndpoint.isEmpty()) { _apiEndpoint = QStringLiteral("https://api.siliconflow.cn/v1/chat/completions"); }
    if (_modelName.isEmpty())   { _modelName   = QStringLiteral("deepseek-ai/DeepSeek-V3"); }
    if (_sttEndpoint.isEmpty()) { _sttEndpoint = QStringLiteral("https://api.siliconflow.cn/v1/audio/transcriptions"); }
    if (_sttModel.isEmpty())    { _sttModel    = QStringLiteral("FunAudioLLM/SenseVoiceSmall"); }

    // Never forward an LLM credential to a different transcription provider.
    const QUrl llmUrl(_apiEndpoint);
    const QUrl sttUrl(_sttEndpoint);
    if (_sttApiKey.isEmpty() && llmUrl.scheme() == QStringLiteral("https") &&
            sttUrl.scheme() == llmUrl.scheme() && !llmUrl.host().isEmpty() &&
            sttUrl.host() == llmUrl.host() && sttUrl.port(443) == llmUrl.port(443)) {
        _sttApiKey = _apiKey;
    }
}

void VoicePlanStrategy::recomputeGeometry(const QGeoCoordinate& center)
{
    Q_UNUSED(center);
}

bool VoicePlanStrategy::loadParams(const QJsonObject& params)
{
    Q_UNUSED(params);
    return true;
}

QList<MissionItem*> VoicePlanStrategy::missionItems(double relAlt, MAV_FRAME frame, QObject* parent)
{
    Q_UNUSED(relAlt); Q_UNUSED(frame); Q_UNUSED(parent);
    return {};
}

void VoicePlanStrategy::sendToLLM(const QString& text)
{
    if (_llmReply) {
        _setStatus(tr("A planning request is already in progress."));
        return;
    }
    _loadConfig();
    if (text.trimmed().isEmpty()) {
        _setStatus(tr("Error: Please enter a description of your mission."));
        return;
    }

    // Combo parsing: try a fast local keyword match first; only call the LLM when
    // keywords are inconclusive. Saves a round-trip and works offline.
    QJsonObject kwIntent;
    if (_shell != nullptr && _tryKeywordParse(text, kwIntent)) {
        QString err;
        if (_shell->applyMissionIntent(kwIntent, err)) {
            _setStatus(tr("Parsed locally: %1 plan generated.")
                       .arg(kwIntent.value(QStringLiteral("planMode")).toString().toUpper()));
            return;
        }
        // Local apply failed — fall through to the LLM.
    }

    if (_apiKey.isEmpty()) {
        _setStatus(tr("Error: API key not configured. Use Application Settings > LLM or set QGC_LLM_API_KEY."));
        return;
    }

    _lastText    = text;
    _retriesLeft = 2;        // retry transient network failures up to twice
    _setStatus(tr("Sending to LLM..."));
    _post(text);
}

bool VoicePlanStrategy::_tryKeywordParse(const QString& text, QJsonObject& intent)
{
    const QString t = text.toLower();

    // Pull the first number captured by `pat` into `out`; return whether found.
    auto grab = [&t](const QString& pat, double& out) -> bool {
        QRegularExpression re(pat, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch m = re.match(t);
        if (!m.hasMatch()) { return false; }
        for (int i = 1; i <= m.lastCapturedIndex(); ++i) {
            bool ok = false;
            double v = m.captured(i).toDouble(&ok);
            if (ok) { out = v; return true; }
        }
        return false;
    };

    const bool isSurvey = t.contains(QStringLiteral("survey")) || t.contains(QStringLiteral("grid")) ||
                          t.contains(QStringLiteral("mapping")) || t.contains(QString::fromUtf8("测绘")) ||
                          t.contains(QString::fromUtf8("网格")) || t.contains(QString::fromUtf8("测区"));
    const bool isSquare = t.contains(QStringLiteral("square")) || t.contains(QStringLiteral("sar")) ||
                          t.contains(QString::fromUtf8("方形")) || t.contains(QString::fromUtf8("扩展搜索")) ||
                          t.contains(QString::fromUtf8("搜救"));
    const bool isSpiral = t.contains(QStringLiteral("spiral")) || t.contains(QString::fromUtf8("螺旋"));
    const bool isFom    = t.contains(QStringLiteral("fom"))    || t.contains(QString::fromUtf8("标定")) ||
                          t.contains(QString::fromUtf8("圆形"));

    QJsonObject params;
    double v = 0.0;

    if (isSurvey) {
        // width × height is mandatory for a confident local parse.
        QRegularExpression dim(QStringLiteral("(\\d+(?:\\.\\d+)?)\\s*(?:米|m)?\\s*[x×*by]+\\s*(\\d+(?:\\.\\d+)?)"),
                               QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch dm = dim.match(t);
        if (!dm.hasMatch()) { return false; }
        params[QStringLiteral("width")]  = dm.captured(1).toDouble();
        params[QStringLiteral("height")] = dm.captured(2).toDouble();
        if (grab(QString::fromUtf8("(?:间距|spacing)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v)) params[QStringLiteral("spacing")] = v;
        if (grab(QString::fromUtf8("(?:角度|angle)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v))   params[QStringLiteral("angle")]   = v;
        if (grab(QString::fromUtf8("(?:高度|altitude|alt)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v) ||
            grab(QString::fromUtf8("(\\d+(?:\\.\\d+)?)\\s*米?\\s*高"), v)) params[QStringLiteral("altitude")] = v;
        intent[QStringLiteral("planMode")] = QStringLiteral("survey");
        intent[QStringLiteral("params")]   = params;
        return true;
    }

    if (isSquare) {
        bool any = false;
        if (grab(QString::fromUtf8("(?:间距|spacing)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v)) { params[QStringLiteral("spacing")] = v; any = true; }
        if (grab(QString::fromUtf8("(\\d+)\\s*(?:条|段|legs?)"), v) ||
            grab(QString::fromUtf8("(?:legs?|腿|段数)\\s*[:：]?\\s*(\\d+)"), v)) { params[QStringLiteral("legs")] = (int)v; any = true; }
        if (grab(QString::fromUtf8("(?:高度|altitude|alt)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v) ||
            grab(QString::fromUtf8("(\\d+(?:\\.\\d+)?)\\s*米?\\s*高"), v)) params[QStringLiteral("altitude")] = v;
        if (t.contains(QString::fromUtf8("逆时针")) || t.contains(QStringLiteral("ccw"))) params[QStringLiteral("direction")] = 1;
        if (!any) { return false; }
        if (!params.contains(QStringLiteral("spacing"))) params[QStringLiteral("spacing")] = 25.0;
        if (!params.contains(QStringLiteral("legs")))    params[QStringLiteral("legs")]    = 12;
        intent[QStringLiteral("planMode")] = QStringLiteral("square");
        intent[QStringLiteral("params")]   = params;
        return true;
    }

    if (isSpiral) {
        bool any = false;
        if (grab(QString::fromUtf8("(?:间距|spacing)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v)) { params[QStringLiteral("spacing")] = v; any = true; }
        if (grab(QString::fromUtf8("(\\d+(?:\\.\\d+)?)\\s*(?:圈|turns?)"), v) ||
            grab(QString::fromUtf8("(?:turns?|圈数)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v)) { params[QStringLiteral("turns")] = v; any = true; }
        if (grab(QString::fromUtf8("(?:高度|altitude|alt)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v) ||
            grab(QString::fromUtf8("(\\d+(?:\\.\\d+)?)\\s*米?\\s*高"), v)) { params[QStringLiteral("altitude")] = v; any = true; }
        if (t.contains(QString::fromUtf8("逆时针")) || t.contains(QStringLiteral("ccw")) || t.contains(QStringLiteral("counter")))
            params[QStringLiteral("direction")] = 1;
        else if (t.contains(QString::fromUtf8("顺时针")) || t.contains(QStringLiteral("cw")))
            params[QStringLiteral("direction")] = 0;
        if (!any) { return false; }   // spiral keyword but no numbers → let the LLM try
        // spacing/turns are required by the schema; supply sensible defaults.
        if (!params.contains(QStringLiteral("spacing"))) params[QStringLiteral("spacing")] = 25.0;
        if (!params.contains(QStringLiteral("turns")))   params[QStringLiteral("turns")]   = 5.0;
        intent[QStringLiteral("planMode")] = QStringLiteral("spiral");
        intent[QStringLiteral("params")]   = params;
        return true;
    }

    if (isFom) {
        bool any = false;
        if (grab(QString::fromUtf8("(?:半径|radius)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v)) { params[QStringLiteral("sideLength")] = v * 2.0; any = true; }
        if (grab(QString::fromUtf8("(\\d+(?:\\.\\d+)?)\\s*(?:圈|loops?)"), v) ||
            grab(QString::fromUtf8("(?:loops?|圈数)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v)) { params[QStringLiteral("cycles")] = qRound(v); any = true; }
        if (grab(QString::fromUtf8("(?:高度|altitude|alt)\\s*[:：]?\\s*(\\d+(?:\\.\\d+)?)"), v) ||
            grab(QString::fromUtf8("(\\d+(?:\\.\\d+)?)\\s*米?\\s*高"), v)) params[QStringLiteral("altitude")] = v;
        if (!any) { return false; }
        if (!params.contains(QStringLiteral("sideLength"))) params[QStringLiteral("sideLength")] = 60.0;
        if (!params.contains(QStringLiteral("cycles")))     params[QStringLiteral("cycles")] = 3;
        intent[QStringLiteral("planMode")] = QStringLiteral("fom");
        intent[QStringLiteral("params")]   = params;
        return true;
    }

    return false;   // nothing matched confidently → use the LLM
}

void VoicePlanStrategy::startRecording()
{
    _audioRecorder->start();
}

void VoicePlanStrategy::stopRecordingAndTranscribe()
{
    _audioRecorder->stop();
}

bool VoicePlanStrategy::isRecording() const
{
    return _audioRecorder->isRecording();
}

void VoicePlanStrategy::_uploadForTranscription(const QString& filePath)
{
    if (_sttReply) {
        _setStatus(tr("A transcription request is already in progress."));
        return;
    }
    _loadConfig();
    if (_sttEndpoint.isEmpty()) {
        _setStatus(tr("Error: STT endpoint not set. Configure it in Application Settings > LLM or set QGC_STT_ENDPOINT."));
        return;
    }
    QFile* audio = new QFile(filePath);
    if (!audio->exists() || audio->size() < 256 || !audio->open(QIODevice::ReadOnly)) {
        if (audio->isOpen()) { audio->close(); }
        delete audio;
        // The most common cause on a WSL desktop is simply that there is no
        // microphone device available there at all.
        _setStatus(tr("No audio captured. On a WSL/Linux desktop there is usually no "
                      "microphone — type your description in the text box instead. "
                      "Voice input works on the Android device."));
        return;
    }

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("audio/wav")));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"audio.wav\"")));
    filePart.setBodyDevice(audio);
    audio->setParent(multiPart);   // multiPart takes ownership, closes/deletes file
    multiPart->append(filePart);

    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant(QStringLiteral("form-data; name=\"model\"")));
    modelPart.setBody(_sttModel.toUtf8());
    multiPart->append(modelPart);

    const QUrl endpoint(_sttEndpoint);
    if (!endpoint.isValid() || endpoint.scheme() != QStringLiteral("https") || endpoint.host().isEmpty() ||
            !endpoint.userInfo().isEmpty() || endpoint.hasFragment()) {
        delete multiPart;
        _setStatus(tr("Error: Configure a valid HTTPS transcription endpoint without embedded credentials."));
        return;
    }
    QNetworkRequest request{endpoint};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    if (!_sttApiKey.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + _sttApiKey.toUtf8());
    }
    request.setTransferTimeout(30000);

    QNetworkReply* reply = _sttNetwork->post(request, multiPart);
    _sttReply = reply;
    multiPart->setParent(reply);   // delete the multipart with the reply
}

void VoicePlanStrategy::_post(const QString& text)
{
    QUrl url(_apiEndpoint);
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty() ||
            !url.userInfo().isEmpty() || url.hasFragment()) {
        _setStatus(tr("Error: Configure a valid HTTPS endpoint without embedded credentials."));
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);


    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(30000);

    // Auto-detect API format from endpoint URL
    bool isAnthropic = _apiEndpoint.contains(QStringLiteral("anthropic"));

    if (isAnthropic) {
        request.setRawHeader("x-api-key", _apiKey.toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");
    } else {
        // OpenAI / SiliconFlow format
        request.setRawHeader("Authorization", QByteArray("Bearer ") + _apiKey.toUtf8());
    }

    QJsonObject body = _buildRequestBody(text);
    _llmReply = _network->post(request, QJsonDocument(body).toJson());
}

QJsonObject VoicePlanStrategy::_buildRequestBody(const QString& text) const
{
    QString dynamicPrompt = _systemPrompt + QStringLiteral(
        "\n\nCUSTOM TRAJECTORY SUPPORT: If the user describes an arbitrary shape or path "
        "(not a standard spiral/circle), output planMode \"custom\" with a list of east/north "
        "offset coordinates (meters from the center point). Offsets define the flight path.\n"
        "Format: {\"planMode\":\"custom\",\"params\":{\"altitude\":80,\"closeLoop\":true,"
        "\"offsets\":[{\"e\":0,\"n\":0},{\"e\":50,\"n\":30},{\"e\":100,\"n\":0},...]}}\n"
        "e = east (positive), west (negative). n = north (positive), south (negative). "
        "Generate enough offset points to densely trace the described shape (at least every 30m). "
        "closeLoop: true if the path should return to start. altitude: meters above takeoff.\n"
        "Examples: \"draw a square 200m per side\" → 5 offset points. "
        "\"draw a star shape\" → 10+ offset points. "
        "\"fly along a river curving east then north\" → smooth offset points tracing the curve.\n"
        "For standard scanning patterns, prefer spiral/fom planMode with their params.");

    bool isAnthropic = _apiEndpoint.contains(QStringLiteral("anthropic"));

    QJsonArray messages;

    if (isAnthropic) {
        // Anthropic Messages format: system at top level, messages = user/assistant only
        QJsonObject userMsg;
        userMsg[QStringLiteral("role")]    = QStringLiteral("user");
        userMsg[QStringLiteral("content")] = text;
        messages.append(userMsg);
    } else {
        // OpenAI Chat format: system message as first array element
        QJsonObject sysMsg;
        sysMsg[QStringLiteral("role")]    = QStringLiteral("system");
        sysMsg[QStringLiteral("content")] = dynamicPrompt;
        messages.append(sysMsg);

        QJsonObject userMsg;
        userMsg[QStringLiteral("role")]    = QStringLiteral("user");
        userMsg[QStringLiteral("content")] = text;
        messages.append(userMsg);
    }

    QJsonObject body;
    body[QStringLiteral("model")]       = _modelName;
    body[QStringLiteral("messages")]    = messages;
    body[QStringLiteral("max_tokens")]  = 4096;

    if (isAnthropic) {
        body[QStringLiteral("system")] = dynamicPrompt;
    }

    return body;
}

bool VoicePlanStrategy::_parseResponse(const QByteArray& responseBody, QJsonObject& intent)
{
    QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject()) { return false; }
    QJsonObject root = doc.object();

    if (root.contains(QStringLiteral("error"))) {
        QJsonObject err = root[QStringLiteral("error")].toObject();
        _setStatus(tr("API Error: %1").arg(err[QStringLiteral("message")].toString()));
        return false;
    }

    QString content;

    // Anthropic Messages format: {"content": [{"text": "...", "type": "text"}]}
    QJsonArray anthropicContent = root[QStringLiteral("content")].toArray();
    if (!anthropicContent.isEmpty()) {
        content = anthropicContent[0].toObject()[QStringLiteral("text")].toString();
    }

    // OpenAI Chat format (fallback): {"choices": [{"message": {"content": "..."}}]}
    if (content.isEmpty()) {
        QJsonArray choices = root[QStringLiteral("choices")].toArray();
        if (!choices.isEmpty()) {
            content = choices[0].toObject()[QStringLiteral("message")].toObject()[QStringLiteral("content")].toString();
        }
    }

    if (content.isEmpty()) { return false; }

    content = content.trimmed();
    // Strip an optional ```json … ``` fence first.
    int fence = content.indexOf(QStringLiteral("```json"));
    if (fence >= 0) {
        fence += 7;
        int fenceEnd = content.indexOf(QStringLiteral("```"), fence);
        content = (fenceEnd > fence) ? content.mid(fence, fenceEnd - fence).trimmed()
                                     : content.mid(fence).trimmed();
    }
    // Extract the FIRST balanced {...} object via brace counting (string-aware).
    // More robust than lastIndexOf('}'), which can swallow trailing prose/objects.
    content = _extractFirstJsonObject(content);
    if (content.isEmpty()) { return false; }

    QJsonDocument intentDoc = QJsonDocument::fromJson(content.toUtf8());
    if (!intentDoc.isObject()) { return false; }
    intent = intentDoc.object();
    return intent.contains(QStringLiteral("planMode")) && intent.contains(QStringLiteral("params"));
}

QString VoicePlanStrategy::_extractFirstJsonObject(const QString& text)
{
    int depth = 0;
    int start = -1;
    bool inStr = false;
    bool escaped = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (inStr) {
            if (escaped)            { escaped = false; }
            else if (c == '\\')     { escaped = true; }
            else if (c == '"')      { inStr = false; }
            continue;
        }
        if (c == '"')      { inStr = true; }
        else if (c == '{') { if (depth++ == 0) { start = i; } }
        else if (c == '}') {
            if (depth > 0 && --depth == 0 && start >= 0) {
                return text.mid(start, i - start + 1);
            }
        }
    }
    return QString();
}

QString VoicePlanStrategy::_extractAssistantText(const QByteArray& responseBody)
{
    QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject()) { return QString(); }
    QJsonObject root = doc.object();
    QString content;
    QJsonArray anthropicContent = root.value(QStringLiteral("content")).toArray();
    if (!anthropicContent.isEmpty()) {
        content = anthropicContent[0].toObject().value(QStringLiteral("text")).toString();
    }
    if (content.isEmpty()) {
        QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty()) {
            content = choices[0].toObject().value(QStringLiteral("message"))
                      .toObject().value(QStringLiteral("content")).toString();
        }
    }
    return content.trimmed();
}

void VoicePlanStrategy::reviewSafety(const QString& missionContext)
{
    if (_reviewBusy) {
        return;
    }
    _loadConfig();
    const QUrl endpoint(_apiEndpoint);
    if (!endpoint.isValid() || endpoint.scheme() != QStringLiteral("https") || endpoint.host().isEmpty() ||
            !endpoint.userInfo().isEmpty() || endpoint.hasFragment()) {
        _reviewText = tr("Error: Configure a valid HTTPS endpoint without embedded credentials.");
        emit reviewChanged();
        return;
    }
    if (_apiKey.isEmpty()) {
        _reviewText = tr("Error: API key not configured. Use Application Settings > LLM or set QGC_LLM_API_KEY.");
        emit reviewChanged();
        return;
    }

    _reviewBusy = true;
    _reviewText = tr("AI 正在复核任务安全…");
    emit reviewChanged();

    const QString sys = QStringLiteral(
        "You are a UAV flight-safety reviewer. Given a mission summary, identify concrete safety "
        "risks and give actionable advice. This is advisory only, not flight clearance. "
        "Do not assume a universal legal altitude limit or infer missing terrain/datum data. "
        "Consider applicable local restrictions, terrain/ground clearance, battery/endurance vs "
        "estimated flight time, attitude stability (high speed on tight "
        "spiral spacing causes violent tilt), altitude-mode correctness, geofence, and takeoff/land "
        "setup. Reply in Simplified Chinese as a short prioritised list using 🔴 high / 🟡 medium / "
        "🟢 ok markers. Be concise and specific with numbers. Do NOT output JSON.");

    bool isAnthropic = _apiEndpoint.contains(QStringLiteral("anthropic"));
    QJsonArray messages;

    if (isAnthropic) {
        QJsonObject userMsg;
        userMsg[QStringLiteral("role")]    = QStringLiteral("user");
        userMsg[QStringLiteral("content")] = missionContext;
        messages.append(userMsg);
    } else {
        QJsonObject sysMsg;
        sysMsg[QStringLiteral("role")]    = QStringLiteral("system");
        sysMsg[QStringLiteral("content")] = sys;
        messages.append(sysMsg);
        QJsonObject userMsg;
        userMsg[QStringLiteral("role")]    = QStringLiteral("user");
        userMsg[QStringLiteral("content")] = missionContext;
        messages.append(userMsg);
    }

    QJsonObject body;
    body[QStringLiteral("model")]      = _modelName;
    body[QStringLiteral("messages")]   = messages;
    body[QStringLiteral("max_tokens")] = 1024;
    if (isAnthropic) {
        body[QStringLiteral("system")] = sys;
    }

    QNetworkRequest request{QUrl(_apiEndpoint)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (isAnthropic) {
        request.setRawHeader("x-api-key", _apiKey.toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");
    } else {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + _apiKey.toUtf8());
    }
    request.setTransferTimeout(40000);
    _reviewNetwork->post(request, QJsonDocument(body).toJson());
}

void VoicePlanStrategy::_onReviewReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    _reviewBusy = false;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300 && status < 400) {
        _reviewText = tr("Redirect refused. Configure the final HTTPS endpoint directly.");
        emit reviewChanged();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        _reviewText = tr("复核网络错误：%1").arg(reply->errorString());
        emit reviewChanged();
        return;
    }
    QString text = _extractAssistantText(reply->readAll());
    _reviewText = text.isEmpty() ? tr("未返回复核结果，请重试。") : text;
    emit reviewChanged();
}

void VoicePlanStrategy::_onSttReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    if (_sttReply != reply) {
        return;
    }
    _sttReply.clear();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300 && status < 400) {
        _setStatus(tr("Redirect refused. Configure the final HTTPS endpoint directly."));
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        _setStatus(tr("Transcription error: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        _setStatus(tr("Transcription failed: unexpected response."));
        return;
    }
    QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("error"))) {
        _setStatus(tr("STT API Error: %1").arg(root[QStringLiteral("error")].toObject()
                       [QStringLiteral("message")].toString()));
        return;
    }
    // Whisper-style { "text": "..." }
    const QString transcript = root.value(QStringLiteral("text")).toString().trimmed();
    if (transcript.isEmpty()) {
        _setStatus(tr("Heard nothing — please try again."));
        return;
    }
    // Don't auto-parse: surface the transcript for the user to review/edit, then
    // they press "Send to LLM" to parse it.
    _setStatus(tr("Recognised — edit if needed, then tap Send."));
    emit transcriptReady(transcript);
}

void VoicePlanStrategy::_onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    if (_llmReply != reply) {
        return;
    }
    _llmReply.clear();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300 && status < 400) {
        _setStatus(tr("Redirect refused. Configure the final HTTPS endpoint directly."));
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        // Retry transient failures (timeout, connection reset) a couple of times.
        const QNetworkReply::NetworkError e = reply->error();
        const bool transient = (e == QNetworkReply::TimeoutError ||
                                e == QNetworkReply::TemporaryNetworkFailureError ||
                                e == QNetworkReply::ConnectionRefusedError ||
                                e == QNetworkReply::RemoteHostClosedError);
        if (transient && _retriesLeft > 0) {
            --_retriesLeft;
            _setStatus(tr("Network hiccup, retrying… (%1 left)").arg(_retriesLeft + 1));
            _post(_lastText);
            return;
        }
        _setStatus(tr("Network Error: %1").arg(reply->errorString()));
        return;
    }

    QByteArray responseBody = reply->readAll();
    QJsonObject intent;

    if (!_parseResponse(responseBody, intent)) {
        _setStatus(tr("Failed to parse LLM response. Try again with a clearer description."));
        return;
    }

    if (_shell != nullptr) {
        QString err;
        if (_shell->applyMissionIntent(intent, err)) {
            QString mode = intent[QStringLiteral("planMode")].toString();
            _setStatus(tr("Done! %1 plan generated. Switch to the %1 tab.").arg(mode.toUpper()));
        } else {
            _setStatus(tr("Failed: %1").arg(err));
        }
    } else {
        _setStatus(tr("LLM response received but no shell connected."));
    }
}

void VoicePlanStrategy::_setStatus(const QString& msg)
{
    if (_statusText != msg) {
        _statusText = msg;
        emit statusChanged();
    }
}
