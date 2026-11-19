#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <QList>
#include <QtQml/qqmlregistration.h>

class MissionController;
class MissionItem;
class PlanMasterController;
class Vehicle;

/// Builds and uploads a B1 in-flight mission splice:
/// keep the flown prefix (including the whole current complex item), append the
/// edited plan suffix, then point MISSION_CURRENT at the first new item.
class InFlightMissionReplaceController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_MOC_INCLUDE("PlanMasterController.h")
    Q_MOC_INCLUDE("MissionController.h")

    Q_PROPERTY(PlanMasterController* planMasterController READ planMasterController WRITE setPlanMasterController NOTIFY planMasterControllerChanged)
    Q_PROPERTY(MissionController* currentMissionController READ currentMissionController WRITE setCurrentMissionController NOTIFY currentMissionControllerChanged)
    Q_PROPERTY(bool planOnly READ planOnly WRITE setPlanOnly NOTIFY planOnlyChanged)
    Q_PROPERTY(int updateMode READ updateMode WRITE setUpdateMode NOTIFY updateModeChanged)
    Q_PROPERTY(bool hasChanges READ hasChanges NOTIFY previewChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY previewChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QVariantMap preview READ preview NOTIFY previewChanged)

public:
    explicit InFlightMissionReplaceController(QObject* parent = nullptr);
    ~InFlightMissionReplaceController() override;

    PlanMasterController* planMasterController() const { return _planMasterController.data(); }
    void setPlanMasterController(QObject* controller);
    MissionController* currentMissionController() const { return _currentMissionController; }
    void setCurrentMissionController(MissionController* controller);
    bool planOnly() const { return _planOnly; }
    void setPlanOnly(bool planOnly);
    /// 0 = full replacement, 1 = MAVLink partial mission update.
    int updateMode() const { return _updateMode; }
    void setUpdateMode(int mode);
    bool hasChanges() const { return _hasChanges; }

    bool busy() const { return _busy; }
    bool ready() const { return _ready; }
    QString statusText() const { return _statusText; }
    QString errorString() const { return _errorString; }
    QVariantMap preview() const { return _preview; }

    Q_INVOKABLE void prepare();
    Q_INVOKABLE void uploadReplacement();
    Q_INVOKABLE void reset();

signals:
    void planMasterControllerChanged();
    void currentMissionControllerChanged();
    void planOnlyChanged();
    void updateModeChanged();
    void busyChanged();
    void previewChanged();
    void statusTextChanged();
    void errorStringChanged();
    void finished(bool success);

private slots:
    void _sendComplete(bool error);
    void _currentIndexChanged(int index);
    void _planError(int errorCode, const QString& errorMsg);

private:
    Vehicle* _vehicle() const;
    void _setBusy(bool busy);
    void _setStatus(const QString& status);
    void _setError(const QString& error);
    void _clearPending();
    void _finishWithError(const QString& error);
    void _finishSuccess(int currentIndex);
    void _finishPartialSuccess();
    void _startPartialUpload();
    bool _buildSplice(QString& error);
    bool _buildPartialSplice(QString& error);

    QPointer<PlanMasterController> _planMasterController;
    QPointer<MissionController> _currentMissionController;
    bool _planOnly = false;
    int _updateMode = 0;
    bool _hasChanges = false;
    QVariantMap _preview;
    QList<MissionItem*> _pendingItems;
    QList<MissionItem*> _partialReplaceItems;
    QList<MissionItem*> _partialAppendItems;
    bool _partialUploading = false;
    int _partialUpdateStart = -1;
    int _partialAppendStart = -1;
    QTimer _currentTimeout;
    bool _busy = false;
    bool _ready = false;
    bool _uploading = false;
    bool _currentCommandSent = false;
    int _targetCurrent = -1;
    int _preparedCurrent = -1;
    QString _statusText;
    QString _errorString;
    QString _lastVehicleError;
    int     _partialDeniedRetryCount = 0;
};
