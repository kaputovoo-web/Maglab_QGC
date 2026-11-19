#include "InFlightMissionReplaceController.h"

#include "MissionController.h"
#include "MissionManager.h"
#include "PlanManager.h"
#include "MissionItem.h"
#include "PlanMasterController.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "LinkConfiguration.h"
#include "VisualMissionItem.h"
#include "ComplexMissionItem.h"
#include "MissionSettingsItem.h"
#include "Planning/MaglabMissionValidation.h"

#include <QDebug>

namespace {
bool missionItemsEqual(const MissionItem* a, const MissionItem* b)
{
    if (!a || !b) return false;
    if (a->command() != b->command() ||
        a->frame() != b->frame() ||
        a->autoContinue() != b->autoContinue()) {
        return false;
    }
    for (int param = 1; param <= 7; ++param) {
        const double av = param == 1 ? a->param1() : param == 2 ? a->param2() :
                          param == 3 ? a->param3() : param == 4 ? a->param4() :
                          param == 5 ? a->param5() : param == 6 ? a->param6() : a->param7();
        const double bv = param == 1 ? b->param1() : param == 2 ? b->param2() :
                          param == 3 ? b->param3() : param == 4 ? b->param4() :
                          param == 5 ? b->param5() : param == 6 ? b->param6() : b->param7();
        if (!qFuzzyCompare(av + 1.0, bv + 1.0)) {
            return false;
        }
    }
    return true;
}
}

InFlightMissionReplaceController::InFlightMissionReplaceController(QObject* parent)
    : QObject(parent)
{
    _currentTimeout.setSingleShot(true);
    _currentTimeout.setInterval(3000);
    connect(&_currentTimeout, &QTimer::timeout, this, [this]() {
        if (!_busy) {
            return;
        }
        _setBusy(false);
        _setStatus(tr("任务已上传；未在超时内确认新的当前航点"));
    });
}

InFlightMissionReplaceController::~InFlightMissionReplaceController()
{
    _clearPending();
}

void InFlightMissionReplaceController::setPlanMasterController(QObject* controller)
{
    if (_planMasterController == controller) {
        return;
    }
    _clearPending();
    _ready = false;
    _preview.clear();
    _planMasterController = qobject_cast<PlanMasterController*>(controller);
    emit planMasterControllerChanged();
    emit previewChanged();
}

void InFlightMissionReplaceController::setCurrentMissionController(MissionController* controller)
{
    if (_currentMissionController == controller) {
        return;
    }
    _currentMissionController = controller;
    emit currentMissionControllerChanged();
    if (_ready) {
        _ready = false;
        _preview.clear();
        emit previewChanged();
    }
}

void InFlightMissionReplaceController::setPlanOnly(bool planOnly)
{
    if (_planOnly != planOnly) {
        _planOnly = planOnly;
        _ready = false;
        _hasChanges = false;
        _preview.clear();
        if (_planOnly && _updateMode == 1) {
            _updateMode = 0;
            emit updateModeChanged();
        }
        emit planOnlyChanged();
        emit previewChanged();
    }
}

void InFlightMissionReplaceController::setUpdateMode(int mode)
{
    mode = mode == 1 ? 1 : 0;
    if (_updateMode != mode) {
        _updateMode = mode;
        _ready = false;
        _hasChanges = false;
        _preview.clear();
        _clearPending();
        emit updateModeChanged();
        emit previewChanged();
    }
}

Vehicle* InFlightMissionReplaceController::_vehicle() const
{
    return _planMasterController ? _planMasterController->managerVehicle() : nullptr;
}

void InFlightMissionReplaceController::_setBusy(bool busy)
{
    if (_busy != busy) {
        _busy = busy;
        emit busyChanged();
    }
}

void InFlightMissionReplaceController::_setStatus(const QString& status)
{
    if (_statusText != status) {
        _statusText = status;
        emit statusTextChanged();
    }
}

void InFlightMissionReplaceController::_setError(const QString& error)
{
    if (_errorString != error) {
        _errorString = error;
        emit errorStringChanged();
    }
}

void InFlightMissionReplaceController::_clearPending()
{
    qDeleteAll(_pendingItems);
    _pendingItems.clear();
    qDeleteAll(_partialReplaceItems);
    _partialReplaceItems.clear();
    qDeleteAll(_partialAppendItems);
    _partialAppendItems.clear();
    _partialUploading = false;
    _partialUpdateStart = -1;
    _partialAppendStart = -1;
}

void InFlightMissionReplaceController::_finishWithError(const QString& error)
{
    _clearPending();
    _ready = false;
    _uploading = false;
    _currentCommandSent = false;
    _hasChanges = false;
    _partialUploading = false;
    _preview.clear();
    _setBusy(false);
    _setError(error);
    _setStatus(tr("替换未完成"));
    emit previewChanged();
    emit finished(false);
}

void InFlightMissionReplaceController::reset()
{
    if (_uploading) {
        return;
    }
    _currentCommandSent = false;
    _clearPending();
    _ready = false;
    _preview.clear();
    _preparedCurrent = -1;
    _targetCurrent = -1;
    _hasChanges = false;
    _setBusy(false);
    _setError({});
    _setStatus({});
    emit previewChanged();
}

bool InFlightMissionReplaceController::_buildSplice(QString& error)
{
    _clearPending();
    _ready = false;
    _preview.clear();
    _preparedCurrent = -1;
    _targetCurrent = -1;
    _hasChanges = false;

    PlanMasterController* pmc = _planMasterController.data();
    Vehicle* vehicle = _vehicle();
    if (!pmc || !vehicle) {
        error = tr("没有可用的飞控连接");
        return false;
    }
    if (pmc->offline() || vehicle->isOfflineEditingVehicle()) {
        error = tr("当前没有在线飞控");
        return false;
    }
    // A full replacement can be prepared on the ground. If the vehicle is
    // armed, it must not be in the middle of a non-mission flight mode.
    if (vehicle->armed() && vehicle->flightMode() != vehicle->missionFlightMode()) {
        error = tr("全量替换只能在未解锁状态或任务模式下执行");
        return false;
    }
    if (vehicle->firmwareType() != MAV_AUTOPILOT_ARDUPILOTMEGA) {
        error = tr("飞行中航线替换首版仅支持 ArduPilot");
        return false;
    }

    MissionManager* manager = vehicle->missionManager();
    MissionController* mission = pmc->missionController();
    if (!manager || !mission) {
        error = tr("任务控制器不可用");
        return false;
    }
    if (manager->inProgress() || pmc->syncInProgress()) {
        error = tr("飞控任务正在同步，请稍后再试");
        return false;
    }
    if (!MaglabMissionValidation::validate(mission->visualItems())) {
        error = tr("计划包含无效的 LLM 输出，无法上传");
        return false;
    }
    const SharedLinkInterfacePtr sharedLink = vehicle->vehicleLinkManager()->primaryLink().lock();
    if (!sharedLink || sharedLink->linkConfiguration()->isHighLatency()) {
        error = tr("当前链路不支持可靠的任务替换");
        return false;
    }

    int current = manager->currentIndex();
    if (current < 0) {
        current = 0;
    }
    const int oldCount = manager->missionItems().count();
    if (oldCount <= 0) {
        error = tr("飞控任务没有可用的 Home 位置");
        return false;
    }

    MissionItem* oldHome = nullptr;
    for (MissionItem* item : manager->missionItems()) {
        if (item->sequenceNumber() == 0) {
            oldHome = item;
            break;
        }
    }
    if (!oldHome) {
        error = tr("飞控任务没有可用的 Home 位置");
        return false;
    }

    QObject owner;
    const QList<MissionItem*> newItems = mission->createMissionItems(&owner);

    // Full replacement re-executes the uploaded route from its first
    // waypoint. Keep the vehicle Home as sequence 0, and upload every new
    // route item after it. A Takeoff item is intentionally omitted: the
    // operator is either already flying or preparing a mission on the ground.
    _pendingItems.append(new MissionItem(*oldHome));
    int nextSequence = 1;
    int routeCount = 0;
    for (const MissionItem* newItem : newItems) {
        if (newItem->sequenceNumber() == 0) {
            continue;
        }
        if (newItem->command() == MAV_CMD_NAV_TAKEOFF ||
            newItem->command() == MAV_CMD_NAV_VTOL_TAKEOFF) {
            continue;
        }
        if (newItem->command() == MAV_CMD_DO_JUMP) {
            error = tr("暂不支持新航线中的 DO_JUMP 指令");
            _clearPending();
            return false;
        }
        MissionItem* copy = new MissionItem(*newItem);
        copy->setSequenceNumber(nextSequence++);
        _pendingItems.append(copy);
        ++routeCount;
    }
    if (routeCount <= 0) {
        error = tr("新航线没有可执行航点");
        _clearPending();
        return false;
    }

    // Full replacement always sets current to the first uploaded route item.
    const int newCurrent = 1;
    _hasChanges = true;

    _preview[QStringLiteral("armed")] = vehicle->armed();
    _preview[QStringLiteral("mode")] = vehicle->flightMode();
    _preview[QStringLiteral("currentIndex")] = current;
    _preview[QStringLiteral("oldCount")] = oldCount;
    _preview[QStringLiteral("prefixEnd")] = 0;
    _preview[QStringLiteral("newStart")] = 1;
    _preview[QStringLiteral("newCurrent")] = newCurrent;
    _preview[QStringLiteral("newCount")] = _pendingItems.count();
    _preview[QStringLiteral("complexBoundary")] = false;
    _preview[QStringLiteral("complexName")] = QString();
    _preview[QStringLiteral("planOnly")] = _planOnly;
    _preview[QStringLiteral("hasChanges")] = _hasChanges;

    _preparedCurrent = current;
    _targetCurrent = newCurrent;
    _ready = true;
    _setError({});
    _setStatus(tr("全量替换：保留飞控 Home，重新上传 %1 个航点，并从序号 1 重新执行")
               .arg(routeCount));
    emit previewChanged();
    return true;
}

bool InFlightMissionReplaceController::_buildPartialSplice(QString& error)
{
    _clearPending();
    _ready = false;
    _preview.clear();
    _preparedCurrent = -1;
    _targetCurrent = -1;
    _hasChanges = false;

    PlanMasterController* pmc = _planMasterController.data();
    Vehicle* vehicle = _vehicle();
    if (!pmc || !vehicle) {
        error = tr("没有可用的飞控连接");
        return false;
    }
    if (pmc->offline() || vehicle->isOfflineEditingVehicle()) {
        error = tr("当前没有在线飞控");
        return false;
    }
    if (_planOnly) {
        error = tr("部分更新要求编辑计划与飞控当前任务对齐；独立新航线请使用全量替换");
        return false;
    }
    // Ground preparation is safe and useful for SITL/mission setup. In
    // flight, partial writes are only allowed in mission mode.
    if (vehicle->armed() && vehicle->flightMode() != vehicle->missionFlightMode()) {
        error = tr("部分更新只能在未解锁状态或任务模式下执行");
        return false;
    }
    if (vehicle->firmwareType() != MAV_AUTOPILOT_ARDUPILOTMEGA) {
        error = tr("部分任务更新首版仅支持 ArduPilot");
        return false;
    }

    MissionManager* manager = vehicle->missionManager();
    MissionController* mission = pmc->missionController();
    if (!manager || !mission) {
        error = tr("任务控制器不可用");
        return false;
    }
    if (manager->inProgress() || pmc->syncInProgress()) {
        error = tr("飞控任务正在同步，请稍后再试");
        return false;
    }
    if (!MaglabMissionValidation::validate(mission->visualItems())) {
        error = tr("计划包含无效的 LLM 输出，无法更新");
        return false;
    }
    const SharedLinkInterfacePtr sharedLink = vehicle->vehicleLinkManager()->primaryLink().lock();
    if (!sharedLink || sharedLink->linkConfiguration()->isHighLatency()) {
        error = tr("当前链路不支持可靠的部分任务更新");
        return false;
    }

    int current = manager->currentIndex();
    const int oldCount = manager->missionItems().count();
    if (current < 0 && !vehicle->armed()) {
        current = 0;
    }
    if (current < 0 || oldCount <= 0 || current >= oldCount) {
        error = tr("无法读取有效的当前任务序号");
        return false;
    }

    auto findItem = [](const QList<MissionItem*>& items, int sequence) -> MissionItem* {
        for (MissionItem* item : items) {
            if (item->sequenceNumber() == sequence) return item;
        }
        return nullptr;
    };
    auto isTakeoff = [](const MissionItem* item) {
        return item && (item->command() == MAV_CMD_NAV_TAKEOFF ||
                        item->command() == MAV_CMD_NAV_VTOL_TAKEOFF);
    };

    MissionItem* oldHome = findItem(manager->missionItems(), 0);
    if (!oldHome) {
        error = tr("飞控任务没有可用的 Home 位置");
        return false;
    }

    QObject owner;
    const QList<MissionItem*> sourceItems = mission->createMissionItems(&owner);
    if (sourceItems.count() < 2) {
        error = tr("新计划没有可用的任务项");
        return false;
    }

    // Align an independently edited plan that omitted the vehicle Takeoff.
    // The vehicle Takeoff stays at sequence 1 and all edited route items are
    // shifted one slot. Prefix items are never written by a partial update.
    QList<MissionItem*> newItems = sourceItems;
    MissionItem* oldTakeoff = findItem(manager->missionItems(), 1);
    MissionItem* newSeq1 = findItem(sourceItems, 1);
    if (isTakeoff(oldTakeoff) && newSeq1 && !isTakeoff(newSeq1)) {
        newItems.clear();
        if (MissionItem* newHome = findItem(sourceItems, 0)) {
            MissionItem* homeCopy = new MissionItem(*newHome, &owner);
            homeCopy->setSequenceNumber(0);
            newItems.append(homeCopy);
        }
        MissionItem* takeoffCopy = new MissionItem(*oldTakeoff, &owner);
        takeoffCopy->setSequenceNumber(1);
        newItems.append(takeoffCopy);
        int nextSeq = 2;
        for (MissionItem* item : sourceItems) {
            if (item->sequenceNumber() == 0) {
                continue;
            }
            MissionItem* copy = new MissionItem(*item, &owner);
            copy->setSequenceNumber(nextSeq++);
            newItems.append(copy);
        }
    }

    // Partial update owns only the suffix after current. Prefix and current
    // are never compared or written; this avoids rejecting an otherwise valid
    // update because of terrain altitude, Home altitude or Takeoff metadata.
    const int updateStart = vehicle->armed() ? qMin(oldCount, current + 1) : 1;
    const int oldSuffixCount = oldCount - updateStart;
    const int newCount = newItems.count();
    const int newSuffixCount = newCount - updateStart;
    if (newSuffixCount < 0 || newSuffixCount < oldSuffixCount) {
        error = tr("部分更新不支持减少任务项；请使用全量替换");
        return false;
    }

    QList<MissionItem*> replacement;
    QList<MissionItem*> appended;
    bool suffixChanged = false;

    for (int i = 0; i < oldSuffixCount; ++i) {
        const int seq = updateStart + i;
        MissionItem* oldItem = findItem(manager->missionItems(), seq);
        MissionItem* newItem = findItem(newItems, seq);
        if (!oldItem || !newItem) {
            error = tr("任务序号不连续，无法进行部分更新");
            return false;
        }
        if (isTakeoff(newItem)) {
            error = tr("新航线不能包含起飞指令");
            return false;
        }
        if (newItem->command() == MAV_CMD_DO_JUMP) {
            error = tr("首版不支持部分更新中的 DO_JUMP 指令");
            return false;
        }
        MissionItem* copy = new MissionItem(*newItem);
        replacement.append(copy);
        if (!missionItemsEqual(oldItem, newItem)) {
            suffixChanged = true;
        }
    }

    for (int seq = oldCount; seq < newCount; ++seq) {
        MissionItem* newItem = findItem(newItems, seq);
        if (!newItem) {
            error = tr("新增范围缺少任务项");
            return false;
        }
        if (isTakeoff(newItem)) {
            error = tr("新航线不能包含起飞指令");
            return false;
        }
        if (newItem->command() == MAV_CMD_DO_JUMP) {
            error = tr("首版不支持部分更新中的 DO_JUMP 指令");
            return false;
        }
        appended.append(new MissionItem(*newItem));
    }

    _partialReplaceItems = replacement;
    _partialAppendItems = appended;
    _partialUpdateStart = updateStart;
    _partialAppendStart = oldCount;
    _preparedCurrent = current;
    _targetCurrent = current;
    _hasChanges = suffixChanged || !appended.isEmpty();

    _preview[QStringLiteral("armed")] = vehicle->armed();
    _preview[QStringLiteral("mode")] = vehicle->flightMode();
    _preview[QStringLiteral("currentIndex")] = current;
    _preview[QStringLiteral("oldCount")] = oldCount;
    _preview[QStringLiteral("prefixEnd")] = current;
    _preview[QStringLiteral("newStart")] = updateStart;
    _preview[QStringLiteral("newCurrent")] = current;
    _preview[QStringLiteral("newCount")] = newCount;
    _preview[QStringLiteral("updateStart")] = updateStart;
    _preview[QStringLiteral("updateEnd")] = oldCount - 1;
    _preview[QStringLiteral("replaceCount")] = _partialReplaceItems.count();
    _preview[QStringLiteral("appendStart")] = oldCount;
    _preview[QStringLiteral("appendCount")] = _partialAppendItems.count();
    _preview[QStringLiteral("partial")] = true;
    _preview[QStringLiteral("appendOnly")] = !suffixChanged && !appended.isEmpty();
    _preview[QStringLiteral("complexBoundary")] = false;
    _preview[QStringLiteral("complexName")] = QString();
    _preview[QStringLiteral("planOnly")] = false;
    _preview[QStringLiteral("hasChanges")] = _hasChanges;
    _ready = true;
    _setError({});
    _setStatus(_hasChanges
               ? tr("部分更新：更新序号 %1..%2，新增 %3 项；保持飞控当前序号 %4")
                     .arg(updateStart).arg(qMax(updateStart, oldCount - 1)).arg(_partialAppendItems.count()).arg(current)
               : tr("未检测到 current 之后的任务项变化"));
    emit previewChanged();
    return true;
}

void InFlightMissionReplaceController::prepare()
{
    _setBusy(true);
    QString error;
    const bool ok = _updateMode == 1 ? _buildPartialSplice(error) : _buildSplice(error);
    _setBusy(false);
    if (!ok) {
        _finishWithError(error);
    }
}

void InFlightMissionReplaceController::uploadReplacement()
{
    if (_busy) {
        return;
    }
    const bool partialRequested = _updateMode == 1;
    if (!_ready || (partialRequested
                    ? (_partialReplaceItems.isEmpty() && _partialAppendItems.isEmpty())
                    : _pendingItems.isEmpty())) {
        _finishWithError(tr("请先预览并确认替换方案"));
        return;
    }
    if (!_hasChanges) {
        _finishWithError(tr("未检测到航线修改，不需要上传"));
        return;
    }

    // Rebuild immediately before upload and require the same structural
    // preview. This prevents uploading a stale plan after the user edits it
    // without pressing "重新预览" again.
    const bool partial = _updateMode == 1;
    const int previewTarget = _targetCurrent;
    const int previewCount = _preview.value(QStringLiteral("newCount")).toInt();
    const int previewCurrent = _preview.value(QStringLiteral("currentIndex")).toInt();
    const int previewPrefixEnd = _preview.value(QStringLiteral("prefixEnd")).toInt();
    const int previewUpdateStart = _preview.value(QStringLiteral("updateStart")).toInt();
    const int previewAppendCount = _preview.value(QStringLiteral("appendCount")).toInt();
    QString rebuildError;
    const bool rebuilt = partial ? _buildPartialSplice(rebuildError) : _buildSplice(rebuildError);
    if (!rebuilt) {
        _finishWithError(rebuildError);
        return;
    }
    if (_targetCurrent != previewTarget ||
        _preview.value(QStringLiteral("newCount")).toInt() != previewCount ||
        _preview.value(QStringLiteral("currentIndex")).toInt() != previewCurrent ||
        _preview.value(QStringLiteral("prefixEnd")).toInt() != previewPrefixEnd ||
        (partial && (_preview.value(QStringLiteral("updateStart")).toInt() != previewUpdateStart ||
                     _preview.value(QStringLiteral("appendCount")).toInt() != previewAppendCount))) {
        _ready = false;
        _clearPending();
        _preview.clear();
        _hasChanges = false;
        _setStatus(tr("计划已变化，请重新预览并确认"));
        emit previewChanged();
        return;
    }
    if (!_hasChanges) {
        _finishWithError(tr("未检测到航线修改，不需要上传"));
        return;
    }

    Vehicle* vehicle = _vehicle();
    MissionManager* manager = vehicle ? vehicle->missionManager() : nullptr;
    if (!vehicle || !manager) {
        _finishWithError(tr("飞控任务管理器不可用"));
        return;
    }
    const int managerCurrent = manager->currentIndex();
    if (manager->inProgress() ||
        (managerCurrent >= 0 && managerCurrent != _preparedCurrent)) {
        _finishWithError(tr("当前任务序号已变化，请重新预览"));
        return;
    }

    _setBusy(true);
    _uploading = true;
    _partialUploading = partialRequested;
    _setError({});
    _setStatus(partialRequested ? tr("正在执行 MAVLink 部分任务更新…")
                                : tr("正在全量上传替换后的任务…"));
    emit previewChanged();

    disconnect(manager, &MissionManager::sendComplete, this, &InFlightMissionReplaceController::_sendComplete);
    disconnect(manager, &MissionManager::currentIndexChanged, this, &InFlightMissionReplaceController::_currentIndexChanged);
    disconnect(manager, &PlanManager::error, this, &InFlightMissionReplaceController::_planError);
    _lastVehicleError.clear();
    _partialDeniedRetryCount = 0;
    connect(manager, &MissionManager::sendComplete, this, &InFlightMissionReplaceController::_sendComplete);
    connect(manager, &MissionManager::currentIndexChanged, this, &InFlightMissionReplaceController::_currentIndexChanged);
    connect(manager, &PlanManager::error, this, &InFlightMissionReplaceController::_planError);

    if (partialRequested) {
        _startPartialUpload();
        return;
    }

    // PlanManager takes ownership of the items; do not delete them after this call.
    const QList<MissionItem*> items = _pendingItems;
    _pendingItems.clear();
    manager->writeMissionItems(items);
}

void InFlightMissionReplaceController::_startPartialUpload()
{
    MissionManager* manager = _vehicle() ? _vehicle()->missionManager() : nullptr;
    if (!manager) {
        _finishWithError(tr("飞控任务管理器不可用"));
        return;
    }

    // Replace an existing contiguous range first.
    if (!_partialReplaceItems.isEmpty()) {
        const QList<MissionItem*> items = _partialReplaceItems;
        _partialReplaceItems.clear();
        _setStatus(tr("部分更新：替换序号 %1..%2")
                   .arg(_partialUpdateStart).arg(_partialUpdateStart + items.count() - 1));
        manager->writePartialMissionItems(_partialUpdateStart, items);
        return;
    }

    // ArduPilot only permits one appended item per partial-list transaction.
    if (!_partialAppendItems.isEmpty()) {
        MissionItem* item = _partialAppendItems.takeFirst();
        const int sequence = manager->missionItems().count();
        _setStatus(tr("部分更新：追加序号 %1").arg(sequence));
        manager->writePartialMissionItems(sequence, QList<MissionItem*>{ item });
        return;
    }

    _finishPartialSuccess();
}

void InFlightMissionReplaceController::_finishPartialSuccess()
{
    MissionManager* manager = _vehicle() ? _vehicle()->missionManager() : nullptr;
    if (manager) {
        disconnect(manager, &MissionManager::sendComplete,
                   this, &InFlightMissionReplaceController::_sendComplete);
        disconnect(manager, &MissionManager::currentIndexChanged,
                   this, &InFlightMissionReplaceController::_currentIndexChanged);
    }
    _busy = false;
    _uploading = false;
    _partialUploading = false;
    _ready = false;
    _preview.clear();
    _setBusy(false);
    _setStatus(tr("部分更新完成，保持飞控当前序号 %1").arg(_preparedCurrent));
    emit previewChanged();
    if (_planMasterController) {
        _planMasterController->loadFromVehicle();
    }
    emit finished(true);
}

void InFlightMissionReplaceController::_finishSuccess(int currentIndex)
{
    _currentTimeout.stop();
    MissionManager* manager = _vehicle() ? _vehicle()->missionManager() : nullptr;
    if (manager) {
        disconnect(manager, &MissionManager::currentIndexChanged,
                   this, &InFlightMissionReplaceController::_currentIndexChanged);
    }
    _busy = false;
    _uploading = false;
    _currentCommandSent = false;
    _ready = false;
    _preview.clear();
    _setBusy(false);
    _setStatus(tr("替换完成，当前任务序号为 %1").arg(currentIndex));
    emit previewChanged();
    if (_planMasterController) {
        _planMasterController->loadFromVehicle();
    }
    emit finished(true);
}

void InFlightMissionReplaceController::_planError(int errorCode, const QString& errorMsg)
{
    Q_UNUSED(errorCode)
    _lastVehicleError = errorMsg;
}

void InFlightMissionReplaceController::_sendComplete(bool error)
{
    if (!_uploading) {
        return;
    }
    if (_partialUploading) {
        if (error) {
            // ArduPilot returns MAV_MISSION_DENIED ("Not accepting any mission
            // commands") if its previous upload receiver has not timed out
            // yet, or if another GCS is uploading. Retry the idempotent
            // partial suffix a couple of times after waiting for the receiver
            // state to clear before reporting a hard failure.
            const bool temporarilyDenied =
                _lastVehicleError.contains(QStringLiteral("Not accepting"), Qt::CaseInsensitive) ||
                _lastVehicleError.contains(QStringLiteral("mission commands"), Qt::CaseInsensitive) ||
                _lastVehicleError.contains(QStringLiteral("不接受")) ||
                _lastVehicleError.contains(QStringLiteral("任务命令"));
            if (temporarilyDenied && _partialDeniedRetryCount < 2) {
                ++_partialDeniedRetryCount;
                _setStatus(tr("飞控暂时不接受任务命令，正在等待任务接收状态释放后重试（%1/2）…")
                           .arg(_partialDeniedRetryCount));
                _setError({});
                // ArduPilot holds its mission receiver for 8 seconds after a
                // failed/aborted transfer. Retrying earlier causes a second
                // partial-list request while the old receiver is still
                // waiting, followed by "Mission upload timeout". Wait beyond
                // that timeout before retrying.
                const int delayMs = 9000 + (2000 * (_partialDeniedRetryCount - 1));
                QTimer::singleShot(delayMs, this, [this]() {
                    if (!_busy || !_partialUploading) {
                        return;
                    }
                    QString rebuildError;
                    _lastVehicleError.clear();
                    if (!_buildPartialSplice(rebuildError)) {
                        _partialUploading = false;
                        _uploading = false;
                        _finishWithError(rebuildError);
                        return;
                    }
                    _startPartialUpload();
                });
                return;
            }
            _partialUploading = false;
            _uploading = false;
            _finishWithError(_lastVehicleError.isEmpty()
                             ? tr("部分任务更新失败")
                             : tr("部分任务更新失败：%1").arg(_lastVehicleError));
            return;
        }
        Vehicle* partialVehicle = _vehicle();
        if (!partialVehicle) {
            _partialUploading = false;
            _uploading = false;
            _finishWithError(tr("飞控连接已断开"));
            return;
        }
        // ArduPilot clears its partial-transfer receiver after sending the
        // final ACK. Give that state time to settle before starting the next
        // append transaction; otherwise the next write can be rejected with
        // MAV_MISSION_DENIED or the old transfer can time out.
        QTimer::singleShot(750, this, [this]() {
            if (_busy && _partialUploading) {
                _startPartialUpload();
            }
        });
        return;
    }

    _uploading = false;
    MissionManager* manager = _vehicle() ? _vehicle()->missionManager() : nullptr;
    if (manager) {
        disconnect(manager, &MissionManager::sendComplete,
                   this, &InFlightMissionReplaceController::_sendComplete);
    }

    if (error) {
        _finishWithError(tr("全量上传失败，未设置当前航点"));
        return;
    }

    Vehicle* vehicle = _vehicle();
    if (!vehicle) {
        _finishWithError(tr("飞控连接已断开"));
        return;
    }

    if (vehicle->armed() && vehicle->flightMode() != vehicle->missionFlightMode()) {
        _finishWithError(tr("上传完成前已退出任务模式，未设置当前航点"));
        return;
    }

    _setStatus(tr("上传完成，正在设置当前航点…"));
    vehicle->setCurrentMissionSequence(_targetCurrent);
    _currentCommandSent = true;

    // A disarmed SITL/ground vehicle may not publish a new MISSION_CURRENT
    // for the newly written list. The upload itself succeeded; report it and
    // leave the vehicle ready to start from the requested first item.
    if (!vehicle->armed()) {
        _finishSuccess(_targetCurrent);
        return;
    }

    if (_targetCurrent == _preparedCurrent) {
        // A complex-item boundary keeps the same current index. The command was sent,
        // but mission current will not necessarily change, so do not wait forever.
        QTimer::singleShot(400, this, [this]() {
            if (_busy) {
                _finishSuccess(_targetCurrent);
            }
        });
    } else {
        _currentTimeout.start();
    }
}

void InFlightMissionReplaceController::_currentIndexChanged(int index)
{
    if (!_busy || !_currentCommandSent || _targetCurrent < 0 || index != _targetCurrent) {
        return;
    }
    _finishSuccess(index);
}
