/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MissionArchiveManager.h"

#include "Common/MagLabPaths.h"
#include "MissionController.h"
#include "PlanMasterController.h"
#include "QmlObjectListModel.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "MissionManager.h"
#include "MissionItem.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace {

const char* kArchiveVersionKey = "version";
const char* kArchiveEntriesKey = "entries";
const int kArchiveVersion = 1;

QString displayDate(const QString& iso)
{
    const QDateTime value = QDateTime::fromString(iso, Qt::ISODate);
    return value.isValid() ? value.toLocalTime().toString(QStringLiteral("yyyy-MM-dd")) : QString();
}

QString displayTime(const QString& iso)
{
    const QDateTime value = QDateTime::fromString(iso, Qt::ISODate);
    return value.isValid() ? value.toLocalTime().toString(QStringLiteral("HH:mm:ss")) : QStringLiteral("--:--:--");
}

QString makeTimestampName(const QDateTime& now)
{
    return now.toString(QStringLiteral("yyyy-MM-dd-HHmmss"));
}

QString executionName(const QString& preparedIso,
                      const QString& startIso,
                      const QString& endIso,
                      const QString& status)
{
    const QDateTime prepared = QDateTime::fromString(preparedIso, Qt::ISODate).toLocalTime();
    const QDateTime start = QDateTime::fromString(startIso, Qt::ISODate).toLocalTime();
    const QDateTime end = QDateTime::fromString(endIso, Qt::ISODate).toLocalTime();
    const QDateTime dateSource = start.isValid() ? start : prepared;
    const QString date = dateSource.isValid()
            ? dateSource.toString(QStringLiteral("yyyy-MM-dd"))
            : QStringLiteral("unknown-date");

    if (!start.isValid()) {
        return date + QStringLiteral("-WAITING");
    }
    if (end.isValid()) {
        return date + QStringLiteral("-")
                + start.toString(QStringLiteral("HHmmss"))
                + QStringLiteral("-")
                + end.toString(QStringLiteral("HHmmss"));
    }
    return date + QStringLiteral("-")
            + start.toString(QStringLiteral("HHmmss"))
            + QStringLiteral("-")
            + status.toUpper();
}

QString durationText(const QString& startIso, const QString& endIso)
{
    const QDateTime start = QDateTime::fromString(startIso, Qt::ISODate);
    const QDateTime end = QDateTime::fromString(endIso, Qt::ISODate);
    if (!start.isValid() || !end.isValid() || end < start) {
        return QStringLiteral("--:--:--");
    }

    qint64 seconds = start.secsTo(end);
    const qint64 hours = seconds / 3600;
    seconds %= 3600;
    const qint64 minutes = seconds / 60;
    const qint64 remainingSeconds = seconds % 60;
    return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(remainingSeconds, 2, 10, QLatin1Char('0'));
}

} // namespace

MissionArchiveManager::MissionArchiveManager(QObject* parent)
    : QObject(parent)
{
    _autosaveTimer.setSingleShot(true);
    _autosaveTimer.setInterval(1200);
    connect(&_autosaveTimer, &QTimer::timeout, this, &MissionArchiveManager::_autosaveTimeout);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        _endObservedExecution();
        saveNow();
    });
    _loadIndex();
    auto* manager = MultiVehicleManager::instance();
    connect(manager, &MultiVehicleManager::activeVehicleChanged, this, &MissionArchiveManager::observeVehicle);
    observeVehicle(manager->activeVehicle());
}

QVariantList MissionArchiveManager::entries() const
{
    QVariantList result;
    result.reserve(_archiveEntries.size());
    for (const QJsonValue& value : _archiveEntries) {
        result.append(_entryVariant(value.toObject()));
    }
    return result;
}

QVariantList MissionArchiveManager::executionRecords() const
{
    QVariantList result;
    for (const QJsonValue& entryValue : _archiveEntries) {
        const QJsonObject entry = entryValue.toObject();
        const QString planName = entry.value(QStringLiteral("name")).toString();
        const QString routeType = entry.value(QStringLiteral("type")).toString(tr("Mission"));
        const QString fallbackIcon = entry.value(QStringLiteral("icon"))
                .toString(QStringLiteral("/qmlimages/PaperPlane.svg"));
        const QJsonArray executions = entry.value(QStringLiteral("executions")).toArray();

        for (const QJsonValue& executionValue : executions) {
            const QJsonObject execution = executionValue.toObject();
            const QString prepared = execution.value(QStringLiteral("prepared")).toString();
            const QString start = execution.value(QStringLiteral("start")).toString();
            const QString end = execution.value(QStringLiteral("end")).toString();
            if (start.isEmpty()) {
                continue;
            }
            const QString status = execution.value(QStringLiteral("status")).toString(QStringLiteral("running"));
            const QString effectiveName = execution.value(QStringLiteral("executionName")).toString(
                        executionName(prepared, start, end, status));

            QVariantMap record;
            record[QStringLiteral("id")] = execution.value(QStringLiteral("id")).toString();
            record[QStringLiteral("executionName")] = effectiveName;
            record[QStringLiteral("planName")] = execution.value(QStringLiteral("planName")).toString(planName);
            record[QStringLiteral("routeType")] = execution.value(QStringLiteral("routeType")).toString(routeType);
            record[QStringLiteral("icon")] = execution.value(QStringLiteral("icon")).toString(fallbackIcon);
            record[QStringLiteral("status")] = status;
            record[QStringLiteral("date")] = displayDate(start.isEmpty() ? prepared : start);
            record[QStringLiteral("preparedTime")] = displayTime(prepared);
            record[QStringLiteral("startTime")] = displayTime(start);
            record[QStringLiteral("endTime")] = displayTime(end);
            record[QStringLiteral("startDateTime")] = start.isEmpty()
                    ? QStringLiteral("--")
                    : QDateTime::fromString(start, Qt::ISODate).toLocalTime()
                          .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            record[QStringLiteral("endDateTime")] = end.isEmpty()
                    ? QStringLiteral("--")
                    : QDateTime::fromString(end, Qt::ISODate).toLocalTime()
                          .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            record[QStringLiteral("duration")] = durationText(start, end);
            record[QStringLiteral("progress")] = execution.value(QStringLiteral("progress")).toDouble(0.0);
            record[QStringLiteral("currentItem")] = execution.value(QStringLiteral("currentItem")).toInt(0);
            record[QStringLiteral("totalItems")] = execution.value(QStringLiteral("totalItems")).toInt(0);
            record[QStringLiteral("sortKey")] = execution.value(QStringLiteral("lastUpdate")).toString(prepared);
            record[QStringLiteral("isWaiting")] = status == QStringLiteral("waiting");
            record[QStringLiteral("isRunning")] = status == QStringLiteral("running");
            record[QStringLiteral("isCompleted")] = status == QStringLiteral("completed");
            result.append(record);
        }
    }

    std::stable_sort(result.begin(), result.end(), [](const QVariant& left, const QVariant& right) {
        return left.toMap().value(QStringLiteral("sortKey")).toString()
                > right.toMap().value(QStringLiteral("sortKey")).toString();
    });
    return result;
}

QVariantMap MissionArchiveManager::latestRecord() const
{
    const int index = _latestRecordIndex();
    return index >= 0 ? _entryVariant(_archiveEntries[index].toObject()) : QVariantMap();
}

QVariantMap MissionArchiveManager::currentEntry() const
{
    const int index = _findEntryById(_workingEntryId);
    return index >= 0 ? _entryVariant(_archiveEntries[index].toObject()) : QVariantMap();
}

void MissionArchiveManager::attachPlanController(PlanMasterController* controller)
{
    if (_planController == controller) {
        return;
    }

    if (_planController) {
        disconnect(_planController, nullptr, this, nullptr);
    }

    _planController = controller;
    if (!_planController) {
        return;
    }

    connect(_planController, &PlanMasterController::dirtyForSaveChanged,
            this, &MissionArchiveManager::_scheduleAutosave);
    connect(_planController, &PlanMasterController::containsItemsChanged,
            this, [this]() {
        if (_planController) {
            _planContainsItemsChanged(_planController->containsItems());
        }
    });
    connect(_planController, &PlanMasterController::currentPlanFileChanged,
            this, &MissionArchiveManager::_currentPlanFileChanged);

    if (_planController->containsItems()) {
        _scheduleAutosave();
    }
}

void MissionArchiveManager::saveNow()
{
    _autosaveTimer.stop();
    _snapshotNow();
}

bool MissionArchiveManager::loadEntry(int index, PlanMasterController* controller)
{
    if (!controller || index < 0 || index >= _archiveEntries.size()) {
        return false;
    }

    const QJsonObject entry = _archiveEntries[index].toObject();
    const QString id = entry.value(QStringLiteral("id")).toString();
    const QString filePath = _planPath(id);
    if (id.isEmpty() || !QFileInfo::exists(filePath)) {
        emit archiveError(tr("The archived plan file is missing."));
        return false;
    }

    attachPlanController(controller);
    _loadingArchive = true;
    _workingEntryId = id;
    controller->loadFromFile(filePath);
    _loadingArchive = false;
    emit currentEntryChanged();
    return true;
}

void MissionArchiveManager::updateEntry(int index, const QString& name, const QString& iconSource)
{
    if (index < 0 || index >= _archiveEntries.size()) {
        return;
    }

    QJsonObject entry = _archiveEntries[index].toObject();
    const QString trimmed = name.trimmed();
    if (!trimmed.isEmpty()) {
        entry[QStringLiteral("name")] = trimmed.left(80);
    }
    if (!iconSource.trimmed().isEmpty()) {
        entry[QStringLiteral("icon")] = iconSource;
        entry[QStringLiteral("customIcon")] = true;
    }
    entry[QStringLiteral("modified")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    _archiveEntries[index] = entry;
    _sortEntries();
    _writeIndex();
    _notifyAll();
}
void MissionArchiveManager::renameEntry(int index, const QString& name)
{
    if (index < 0 || index >= _archiveEntries.size()) {
        return;
    }

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    QJsonObject entry = _archiveEntries[index].toObject();
    entry[QStringLiteral("name")] = trimmed.left(80);
    entry[QStringLiteral("modified")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    _archiveEntries[index] = entry;
    _sortEntries();
    _writeIndex();
    _notifyAll();
}

void MissionArchiveManager::setEntryIcon(int index, const QString& iconSource)
{
    if (index < 0 || index >= _archiveEntries.size() || iconSource.trimmed().isEmpty()) {
        return;
    }

    QJsonObject entry = _archiveEntries[index].toObject();
    entry[QStringLiteral("icon")] = iconSource;
    entry[QStringLiteral("customIcon")] = true;
    entry[QStringLiteral("modified")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    _archiveEntries[index] = entry;
    _sortEntries();
    _writeIndex();
    _notifyAll();
}

void MissionArchiveManager::removeEntry(int index)
{
    if (index < 0 || index >= _archiveEntries.size()) {
        return;
    }

    const QJsonObject entry = _archiveEntries[index].toObject();
    const QString id = entry.value(QStringLiteral("id")).toString();
    if (id == _executionEntryId) {
        return;
    }

    const QString path = _planPath(id);
    if (path.isEmpty() || (QFileInfo::exists(path) && !QFile::remove(path))) {
        emit archiveError(tr("Unable to remove the archived plan."));
        return;
    }
    _archiveEntries.removeAt(index);
    if (_workingEntryId == id) {
        _workingEntryId.clear();
    }
    _writeIndex();
    _notifyAll();
}

void MissionArchiveManager::_endObservedExecution()
{
    if (_observedExecution) {
        // Disarm, mode exit, disconnect and vehicle replacement do not prove completion.
        finishExecution(false);
        _observedExecution = false;
    }
}

void MissionArchiveManager::observeVehicle(Vehicle* vehicle)
{
    _endObservedExecution();
    for (const auto& connection : _vehicleConnections) {
        disconnect(connection);
    }
    _vehicleConnections.clear();
    _observedVehicle = vehicle;
    if (!vehicle) {
        return;
    }
    auto checkState = [this]() {
        if (!_observedVehicle || !_observedVehicle->armed() ||
            _observedVehicle->flightMode() != _observedVehicle->missionFlightMode() ||
            _observedVehicle->vehicleLinkManager()->communicationLost()) {
            _endObservedExecution();
        } else {
            _observeMissionIndex(_observedVehicle->missionManager()->currentIndex());
        }
    };
    _vehicleConnections.append(connect(vehicle, &Vehicle::armedChanged, this, checkState));
    _vehicleConnections.append(connect(vehicle, &Vehicle::flightModeChanged, this, checkState));
    _vehicleConnections.append(connect(vehicle->vehicleLinkManager(), &VehicleLinkManager::communicationLostChanged, this, checkState));
    _vehicleConnections.append(connect(vehicle, &QObject::destroyed, this, [this]() { _endObservedExecution(); }));
    _vehicleConnections.append(connect(vehicle->missionManager(), &MissionManager::currentIndexChanged,
                                       this, &MissionArchiveManager::_observeMissionIndex));
    _vehicleConnections.append(connect(vehicle->missionManager(), &MissionManager::newMissionItemsAvailable,
                                       this, [this]() { _endObservedExecution(); }));
    checkState();
}

void MissionArchiveManager::_observeMissionIndex(int index)
{
    Vehicle* vehicle = _observedVehicle.data();
    if (!vehicle || index < 0 || !vehicle->armed() || vehicle->missionFlightMode().isEmpty() ||
        vehicle->flightMode() != vehicle->missionFlightMode() ||
        vehicle->vehicleLinkManager()->communicationLost()) {
        return;
    }
    const auto& items = vehicle->missionManager()->missionItems();
    if (items.isEmpty()) {
        return;
    }
    if (!executionActive()) {
        int first = -1;
        int last = -1;
        for (const MissionItem* item : items) {
            if (item && item->command() < MAV_CMD_NAV_LAST && item->sequenceNumber() >= 0) {
                if (first < 0) first = item->sequenceNumber();
                last = item->sequenceNumber();
            }
        }
        if (first < 0 || index > last) return;
        const QString localWorkingEntry = _workingEntryId;
        _startingObservedExecution = true;
        startExecution(items.size(), first, last);
        _startingObservedExecution = false;
        _workingEntryId = localWorkingEntry;
        _observedExecution = executionActive();
        if (_observedExecution) {
            // Start means observed in armed mission mode, not "first waypoint reached".
            missionItemReached(first);
        }
    }
    if (_observedExecution) {
        updateExecution(index, items.size());
    }
}

void MissionArchiveManager::startExecution(int totalItems, int firstNavigationIndex, int lastNavigationIndex)
{
    if (executionActive() || firstNavigationIndex < 0 || lastNavigationIndex < firstNavigationIndex) {
        return;
    }

    if (!_startingObservedExecution && _planController && _planController->containsItems()) {
        _snapshotNow();
    }

    // A received vehicle mission is not proven identical to the locally edited plan.
    int entryIndex = _startingObservedExecution ? -1 : _findEntryById(_workingEntryId);
    if (!_startingObservedExecution && entryIndex < 0 && !_archiveEntries.isEmpty()) {
        entryIndex = 0;
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (entryIndex < 0) {
        QJsonObject entry;
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        entry[QStringLiteral("id")] = id;
        entry[QStringLiteral("name")] = makeTimestampName(now);
        entry[QStringLiteral("type")] = tr("Vehicle mission");
        entry[QStringLiteral("icon")] = QStringLiteral("/qmlimages/PaperPlane.svg");
        entry[QStringLiteral("created")] = now.toString(Qt::ISODate);
        entry[QStringLiteral("modified")] = now.toString(Qt::ISODate);
        entry[QStringLiteral("executions")] = QJsonArray();
        _archiveEntries.prepend(entry);
        entryIndex = 0;
    }

    QJsonObject entry = _archiveEntries[entryIndex].toObject();
    const QString preparedIso = now.toString(Qt::ISODate);
    const QString runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject execution;
    execution[QStringLiteral("id")] = runId;
    execution[QStringLiteral("prepared")] = preparedIso;
    execution[QStringLiteral("start")] = QString();
    execution[QStringLiteral("end")] = QString();
    execution[QStringLiteral("executionName")] = executionName(preparedIso, QString(), QString(), QStringLiteral("waiting"));
    execution[QStringLiteral("planName")] = entry.value(QStringLiteral("name")).toString();
    execution[QStringLiteral("routeType")] = entry.value(QStringLiteral("type")).toString(tr("Mission"));
    execution[QStringLiteral("icon")] = entry.value(QStringLiteral("icon"))
            .toString(QStringLiteral("/qmlimages/PaperPlane.svg"));
    execution[QStringLiteral("lastUpdate")] = preparedIso;
    execution[QStringLiteral("status")] = QStringLiteral("waiting");
    execution[QStringLiteral("progress")] = 0.0;
    execution[QStringLiteral("currentItem")] = 0;
    execution[QStringLiteral("totalItems")] = qMax(0, totalItems);
    execution[QStringLiteral("firstNavigationIndex")] = firstNavigationIndex;
    execution[QStringLiteral("lastNavigationIndex")] = lastNavigationIndex;

    QJsonArray executions = entry.value(QStringLiteral("executions")).toArray();
    executions.prepend(execution);
    entry[QStringLiteral("executions")] = executions;
    entry[QStringLiteral("start")] = QString();
    entry[QStringLiteral("end")] = QString();
    entry[QStringLiteral("executionName")] = execution.value(QStringLiteral("executionName")).toString();
    entry[QStringLiteral("lastUpdate")] = preparedIso;
    entry[QStringLiteral("status")] = QStringLiteral("waiting");
    entry[QStringLiteral("progress")] = 0.0;
    entry[QStringLiteral("currentItem")] = 0;
    entry[QStringLiteral("totalItems")] = qMax(0, totalItems);
    entry[QStringLiteral("firstNavigationIndex")] = firstNavigationIndex;
    entry[QStringLiteral("lastNavigationIndex")] = lastNavigationIndex;
    entry[QStringLiteral("modified")] = preparedIso;
    _archiveEntries[entryIndex] = entry;

    _workingEntryId = entry.value(QStringLiteral("id")).toString();
    _executionEntryId = _workingEntryId;
    _executionRunId = runId;
    _executionProgress = 0.0;
    _sortEntries();
    _writeIndex();
    _notifyAll();
}

void MissionArchiveManager::updateExecution(int currentItem, int totalItems)
{
    const int entryIndex = _findEntryById(_executionEntryId);
    if (entryIndex < 0) {
        return;
    }

    QJsonObject entry = _archiveEntries[entryIndex].toObject();
    const bool started = !entry.value(QStringLiteral("start")).toString().isEmpty();
    const int first = entry.value(QStringLiteral("firstNavigationIndex")).toInt(-1);
    const int last = entry.value(QStringLiteral("lastNavigationIndex")).toInt(-1);
    const int current = qMax(0, currentItem);
    if (started && first >= 0 && last >= first) {
        const int denominator = qMax(1, last - first);
        _executionProgress = qBound(0.0, 100.0 * (current - first) / denominator, 99.0);
    } else {
        _executionProgress = 0.0;
    }

    const QString updateIso = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonArray executions = entry.value(QStringLiteral("executions")).toArray();
    for (int i = 0; i < executions.size(); ++i) {
        QJsonObject execution = executions[i].toObject();
        if (execution.value(QStringLiteral("id")).toString() == _executionRunId) {
            execution[QStringLiteral("lastUpdate")] = updateIso;
            execution[QStringLiteral("progress")] = _executionProgress;
            execution[QStringLiteral("currentItem")] = current;
            execution[QStringLiteral("totalItems")] = qMax(0, totalItems);
            executions[i] = execution;
            break;
        }
    }

    entry[QStringLiteral("executions")] = executions;
    entry[QStringLiteral("lastUpdate")] = updateIso;
    entry[QStringLiteral("progress")] = _executionProgress;
    entry[QStringLiteral("currentItem")] = current;
    entry[QStringLiteral("totalItems")] = qMax(0, totalItems);
    _archiveEntries[entryIndex] = entry;
    _writeIndex();
    _notifyAll();
}

void MissionArchiveManager::missionItemReached(int sequenceNumber)
{
    const int entryIndex = _findEntryById(_executionEntryId);
    if (entryIndex < 0) {
        return;
    }

    QJsonObject entry = _archiveEntries[entryIndex].toObject();
    const int first = entry.value(QStringLiteral("firstNavigationIndex")).toInt(-1);
    const int last = entry.value(QStringLiteral("lastNavigationIndex")).toInt(-1);
    if (sequenceNumber < first || sequenceNumber > last) {
        return;
    }

    const QString reachedIso = QDateTime::currentDateTime().toString(Qt::ISODate);
    QString startIso = entry.value(QStringLiteral("start")).toString();
    if (sequenceNumber == first && startIso.isEmpty()) {
        startIso = reachedIso;
        entry[QStringLiteral("start")] = startIso;
        entry[QStringLiteral("status")] = QStringLiteral("running");
        entry[QStringLiteral("executionName")] = executionName(
                    entry.value(QStringLiteral("lastUpdate")).toString(reachedIso),
                    startIso,
                    QString(),
                    QStringLiteral("running"));
    }
    if (startIso.isEmpty()) {
        return;
    }

    const int denominator = qMax(1, last - first);
    _executionProgress = sequenceNumber >= last
            ? (_observedExecution ? 99.0 : 100.0)
            : qBound(0.0, 100.0 * (sequenceNumber - first) / denominator, 99.0);

    QJsonArray executions = entry.value(QStringLiteral("executions")).toArray();
    for (int i = 0; i < executions.size(); ++i) {
        QJsonObject execution = executions[i].toObject();
        if (execution.value(QStringLiteral("id")).toString() == _executionRunId) {
            if (execution.value(QStringLiteral("start")).toString().isEmpty()) {
                execution[QStringLiteral("start")] = startIso;
                execution[QStringLiteral("status")] = QStringLiteral("running");
            }
            execution[QStringLiteral("executionName")] = executionName(
                        execution.value(QStringLiteral("prepared")).toString(),
                        startIso,
                        QString(),
                        QStringLiteral("running"));
            execution[QStringLiteral("lastUpdate")] = reachedIso;
            execution[QStringLiteral("progress")] = _executionProgress;
            execution[QStringLiteral("currentItem")] = sequenceNumber;
            executions[i] = execution;
            break;
        }
    }

    entry[QStringLiteral("executions")] = executions;
    entry[QStringLiteral("lastUpdate")] = reachedIso;
    entry[QStringLiteral("progress")] = _executionProgress;
    entry[QStringLiteral("currentItem")] = sequenceNumber;
    entry[QStringLiteral("modified")] = reachedIso;
    _archiveEntries[entryIndex] = entry;

    if (sequenceNumber >= last && !_observedExecution) {
        finishExecution(true);
    } else {
        _sortEntries();
        _writeIndex();
        _notifyAll();
    }
}

void MissionArchiveManager::finishExecution(bool completed)
{
    const int entryIndex = _findEntryById(_executionEntryId);
    if (entryIndex < 0) {
        _executionEntryId.clear();
        _executionRunId.clear();
        _executionProgress = 0.0;
        emit executionChanged();
        return;
    }

    const QString eventIso = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject entry = _archiveEntries[entryIndex].toObject();
    const bool started = !entry.value(QStringLiteral("start")).toString().isEmpty();
    if (!started) {
        const QJsonArray executions = entry.value(QStringLiteral("executions")).toArray();
        QJsonArray actualExecutions;
        for (const QJsonValue& executionValue : executions) {
            const QJsonObject execution = executionValue.toObject();
            if (execution.value(QStringLiteral("id")).toString() != _executionRunId &&
                    !execution.value(QStringLiteral("start")).toString().isEmpty()) {
                actualExecutions.append(execution);
            }
        }

        entry[QStringLiteral("executions")] = actualExecutions;
        entry[QStringLiteral("status")] = QStringLiteral("saved");
        entry[QStringLiteral("progress")] = 0.0;
        entry[QStringLiteral("currentItem")] = 0;
        entry[QStringLiteral("start")] = QString();
        entry[QStringLiteral("end")] = QString();
        entry.remove(QStringLiteral("executionName"));
        entry.remove(QStringLiteral("lastUpdate"));
        entry.remove(QStringLiteral("totalItems"));
        entry.remove(QStringLiteral("firstNavigationIndex"));
        entry.remove(QStringLiteral("lastNavigationIndex"));
        _archiveEntries[entryIndex] = entry;

        _executionEntryId.clear();
        _executionRunId.clear();
        _executionProgress = 0.0;
        _sortEntries();
        _writeIndex();
        _notifyAll();
        return;
    }

    const QString status = completed ? QStringLiteral("completed")
                                     : QStringLiteral("interrupted");
    const QString endIso = completed ? eventIso : QString();
    if (completed) {
        _executionProgress = 100.0;
    }

    QJsonArray executions = entry.value(QStringLiteral("executions")).toArray();
    for (int i = 0; i < executions.size(); ++i) {
        QJsonObject execution = executions[i].toObject();
        if (execution.value(QStringLiteral("id")).toString() == _executionRunId) {
            execution[QStringLiteral("lastUpdate")] = eventIso;
            execution[QStringLiteral("status")] = status;
            execution[QStringLiteral("progress")] = _executionProgress;
            if (completed) {
                execution[QStringLiteral("end")] = eventIso;
            }
            execution[QStringLiteral("executionName")] = executionName(
                        execution.value(QStringLiteral("prepared")).toString(),
                        execution.value(QStringLiteral("start")).toString(),
                        endIso,
                        status);
            executions[i] = execution;
            break;
        }
    }

    entry[QStringLiteral("executions")] = executions;
    entry[QStringLiteral("lastUpdate")] = eventIso;
    entry[QStringLiteral("status")] = status;
    entry[QStringLiteral("progress")] = _executionProgress;
    entry[QStringLiteral("modified")] = eventIso;
    if (completed) {
        entry[QStringLiteral("end")] = eventIso;
    } else {
        entry[QStringLiteral("end")] = QString();
    }
    entry[QStringLiteral("executionName")] = executionName(
                entry.value(QStringLiteral("lastUpdate")).toString(eventIso),
                entry.value(QStringLiteral("start")).toString(),
                endIso,
                status);
    _archiveEntries[entryIndex] = entry;

    _executionEntryId.clear();
    _executionRunId.clear();
    _sortEntries();
    _writeIndex();
    _notifyAll();
}
void MissionArchiveManager::_scheduleAutosave()
{
    if (!_loadingArchive && _planController && _planController->containsItems()) {
        _autosaveTimer.start();
    }
}

void MissionArchiveManager::_autosaveTimeout()
{
    _snapshotNow();
}

void MissionArchiveManager::_planContainsItemsChanged(bool containsItems)
{
    if (containsItems) {
        _scheduleAutosave();
    } else if (!executionActive()) {
        _workingEntryId.clear();
        emit currentEntryChanged();
    }
}

void MissionArchiveManager::_currentPlanFileChanged()
{
    if (_loadingArchive || !_planController) {
        return;
    }

    const QString current = QFileInfo(_planController->currentPlanFile()).absoluteFilePath();
    const QString archive = QFileInfo(_archiveRoot()).absoluteFilePath();
    if (!current.isEmpty() && !current.startsWith(archive)) {
        _workingEntryId.clear();
        emit currentEntryChanged();
        _scheduleAutosave();
    }
}

QString MissionArchiveManager::_archiveRoot() const
{
    // Android uses app-private storage; desktop uses Documents/MagLab/missions.
    // MagLabPaths creates the directory on first use.
    return MagLabPaths::instance()->missionsDir();
}

QString MissionArchiveManager::_indexPath() const
{
    const QString root = _archiveRoot();
    return root.isEmpty() ? QString() : QDir(root).filePath(QStringLiteral("archive.json"));
}

QString MissionArchiveManager::_planPath(const QString& id) const
{
    const QString root = _archiveRoot();
    // Index contents are data, not trusted filesystem paths. Preserve old IDs
    // containing safe ASCII characters without permitting traversal or separators.
    if (root.isEmpty() || id.isEmpty() || id.size() > 128) {
        return {};
    }
    for (const QChar c : id) {
        if (!((c >= QLatin1Char('a') && c <= QLatin1Char('z')) ||
              (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) ||
              (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
              c == QLatin1Char('-') || c == QLatin1Char('_'))) {
            return {};
        }
    }
    return QDir(root).filePath(id + QStringLiteral(".plan"));
}

void MissionArchiveManager::_loadIndex()
{
    QFile file(_indexPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = document.object();
    if (root.value(QString::fromLatin1(kArchiveVersionKey)).toInt() != kArchiveVersion) {
        return;
    }

    _archiveEntries = root.value(QString::fromLatin1(kArchiveEntriesKey)).toArray();
    bool changed = false;
    for (int i = 0; i < _archiveEntries.size(); ++i) {
        QJsonObject entry = _archiveEntries[i].toObject();
        bool entryChanged = false;

        const QJsonArray storedExecutions = entry.value(QStringLiteral("executions")).toArray();
        QJsonArray actualExecutions;
        for (const QJsonValue& executionValue : storedExecutions) {
            QJsonObject execution = executionValue.toObject();
            const QString start = execution.value(QStringLiteral("start")).toString();
            if (start.isEmpty()) {
                entryChanged = true;
                continue;
            }
            if (execution.value(QStringLiteral("status")).toString() == QStringLiteral("running")) {
                execution[QStringLiteral("status")] = QStringLiteral("interrupted");
                execution[QStringLiteral("end")] = QString();
                execution[QStringLiteral("executionName")] = executionName(
                            execution.value(QStringLiteral("prepared")).toString(),
                            start,
                            QString(),
                            QStringLiteral("interrupted"));
                entryChanged = true;
            }
            actualExecutions.append(execution);
        }
        if (actualExecutions.size() != storedExecutions.size() || entryChanged) {
            entry[QStringLiteral("executions")] = actualExecutions;
        }

        const QString entryStatus = entry.value(QStringLiteral("status")).toString();
        const QString entryStart = entry.value(QStringLiteral("start")).toString();
        if (entryStart.isEmpty() &&
                (entryStatus == QStringLiteral("waiting") ||
                 entryStatus == QStringLiteral("cancelled"))) {
            entry[QStringLiteral("status")] = QStringLiteral("saved");
            entry[QStringLiteral("progress")] = 0.0;
            entry[QStringLiteral("currentItem")] = 0;
            entry[QStringLiteral("start")] = QString();
            entry[QStringLiteral("end")] = QString();
            entry.remove(QStringLiteral("executionName"));
            entry.remove(QStringLiteral("lastUpdate"));
            entry.remove(QStringLiteral("totalItems"));
            entry.remove(QStringLiteral("firstNavigationIndex"));
            entry.remove(QStringLiteral("lastNavigationIndex"));
            entryChanged = true;
        } else if (entryStatus == QStringLiteral("running") && !entryStart.isEmpty()) {
            const QString prepared = entry.value(QStringLiteral("lastUpdate")).toString(entryStart);
            entry[QStringLiteral("status")] = QStringLiteral("interrupted");
            entry[QStringLiteral("end")] = QString();
            entry[QStringLiteral("executionName")] = executionName(
                        prepared,
                        entryStart,
                        QString(),
                        QStringLiteral("interrupted"));
            entryChanged = true;
        }

        if (entryChanged) {
            _archiveEntries[i] = entry;
            changed = true;
        }
    }
    _sortEntries();
    if (changed) {
        _writeIndex();
    }
}

bool MissionArchiveManager::_writeIndex()
{
    QJsonObject root;
    root[QString::fromLatin1(kArchiveVersionKey)] = kArchiveVersion;
    root[QString::fromLatin1(kArchiveEntriesKey)] = _archiveEntries;

    const QString path = _indexPath();
    if (path.isEmpty()) {
        emit archiveError(tr("Mission archive storage is unavailable."));
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        emit archiveError(tr("Unable to write mission archive index: %1").arg(file.errorString()));
        return false;
    }
    const QByteArray bytes = QJsonDocument(root).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        emit archiveError(tr("Unable to commit mission archive index: %1").arg(file.errorString()));
        return false;
    }
    return true;
}

bool MissionArchiveManager::_writePlan(const QString& filePath, const QByteArray& bytes)
{
    if (filePath.isEmpty()) {
        emit archiveError(tr("Mission archive path is invalid or storage is unavailable."));
        return false;
    }
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit archiveError(tr("Unable to cache mission plan: %1").arg(file.errorString()));
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        emit archiveError(tr("Unable to commit mission plan: %1").arg(file.errorString()));
        return false;
    }
    return true;
}

void MissionArchiveManager::_snapshotNow()
{
    if (!_planController || !_planController->containsItems()) {
        return;
    }

    const QJsonDocument plan = _planController->saveToJson();
    if (plan.isNull()) {
        emit archiveError(tr("Invalid mission output cannot be archived."));
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    int entryIndex = _findEntryById(_workingEntryId);
    if (entryIndex < 0) {
        QJsonObject entry;
        _workingEntryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        entry[QStringLiteral("id")] = _workingEntryId;
        entry[QStringLiteral("name")] = makeTimestampName(now);
        entry[QStringLiteral("created")] = now.toString(Qt::ISODate);
        entry[QStringLiteral("executions")] = QJsonArray();
        _archiveEntries.prepend(entry);
        entryIndex = 0;
    }

    if (!_writePlan(_planPath(_workingEntryId), plan.toJson())) {
        return;
    }

    QJsonObject entry = _archiveEntries[entryIndex].toObject();
    QString type;
    QString icon;
    _detectPlanPresentation(plan, type, icon);
    entry[QStringLiteral("type")] = type;
    if (!entry.value(QStringLiteral("customIcon")).toBool()) {
        entry[QStringLiteral("icon")] = icon;
    }
    entry[QStringLiteral("modified")] = now.toString(Qt::ISODate);
    entry[QStringLiteral("missionItems")] = (_planController->missionController()->visualItems()
        ? qMax(0, _planController->missionController()->visualItems()->count() - 1) : 0);
    _archiveEntries[entryIndex] = entry;
    _sortEntries();
    _writeIndex();
    _notifyAll();
}

int MissionArchiveManager::_findEntryById(const QString& id) const
{
    if (id.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < _archiveEntries.size(); ++i) {
        if (_archiveEntries[i].toObject().value(QStringLiteral("id")).toString() == id) {
            return i;
        }
    }
    return -1;
}

int MissionArchiveManager::_latestRecordIndex() const
{
    const int activeIndex = _findEntryById(_executionEntryId);
    if (activeIndex >= 0) {
        return activeIndex;
    }
    for (int i = 0; i < _archiveEntries.size(); ++i) {
        const QJsonObject entry = _archiveEntries[i].toObject();
        if (!entry.value(QStringLiteral("start")).toString().isEmpty()) {
            return i;
        }
    }
    return _archiveEntries.isEmpty() ? -1 : 0;
}

void MissionArchiveManager::_sortEntries()
{
    QList<QJsonObject> objects;
    objects.reserve(_archiveEntries.size());
    for (const QJsonValue& value : _archiveEntries) {
        objects.append(value.toObject());
    }
    std::stable_sort(objects.begin(), objects.end(), [](const QJsonObject& left, const QJsonObject& right) {
        return left.value(QStringLiteral("modified")).toString()
                > right.value(QStringLiteral("modified")).toString();
    });
    _archiveEntries = QJsonArray();
    for (const QJsonObject& object : objects) {
        _archiveEntries.append(object);
    }
}

QVariantMap MissionArchiveManager::_entryVariant(const QJsonObject& entry) const
{
    QVariantMap result = entry.toVariantMap();
    const QString status = entry.value(QStringLiteral("status")).toString(QStringLiteral("saved"));
    const QString modified = entry.value(QStringLiteral("modified")).toString();
    const QString start = entry.value(QStringLiteral("start")).toString();
    const QString end = entry.value(QStringLiteral("end")).toString();
    result[QStringLiteral("name")] = entry.value(QStringLiteral("name")).toString();
    result[QStringLiteral("executionName")] = entry.value(QStringLiteral("executionName")).toString(
                executionName(entry.value(QStringLiteral("lastUpdate")).toString(modified),
                              start,
                              end,
                              status));
    result[QStringLiteral("type")] = entry.value(QStringLiteral("type")).toString(tr("Mission"));
    result[QStringLiteral("icon")] = entry.value(QStringLiteral("icon"))
            .toString(QStringLiteral("/qmlimages/PaperPlane.svg"));
    result[QStringLiteral("status")] = status;
    result[QStringLiteral("progress")] = entry.value(QStringLiteral("progress")).toDouble(0.0);
    result[QStringLiteral("currentItem")] = entry.value(QStringLiteral("currentItem")).toInt(0);
    result[QStringLiteral("totalItems")] = entry.value(QStringLiteral("totalItems")).toInt(0);
    result[QStringLiteral("savedDate")] = displayDate(modified);
    result[QStringLiteral("savedTime")] = displayTime(modified);
    result[QStringLiteral("flightDate")] = displayDate(start);
    result[QStringLiteral("startTime")] = displayTime(start);
    result[QStringLiteral("endTime")] = displayTime(end);
    result[QStringLiteral("isRunning")] = status == QStringLiteral("running");
    result[QStringLiteral("isWaiting")] = status == QStringLiteral("waiting");
    result[QStringLiteral("hasExecution")] = !start.isEmpty();
    return result;
}

void MissionArchiveManager::_notifyAll()
{
    emit entriesChanged();
    emit latestRecordChanged();
    emit currentEntryChanged();
    emit executionChanged();
}

QJsonObject MissionArchiveManager::_findComplexItem(const QJsonValue& value)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (object.contains(QStringLiteral("complexItemType"))) {
            return object;
        }
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            const QJsonObject found = _findComplexItem(it.value());
            if (!found.isEmpty()) {
                return found;
            }
        }
    } else if (value.isArray()) {
        for (const QJsonValue& child : value.toArray()) {
            const QJsonObject found = _findComplexItem(child);
            if (!found.isEmpty()) {
                return found;
            }
        }
    }
    return QJsonObject();
}

void MissionArchiveManager::_detectPlanPresentation(const QJsonDocument& plan, QString& type, QString& icon)
{
    type = tr("Waypoint route");
    icon = QStringLiteral("/qmlimages/MapAddMission.svg");

    const QJsonObject complexItem = _findComplexItem(plan.object());
    const QString complexType = complexItem.value(QStringLiteral("complexItemType")).toString();
    if (complexType.compare(QStringLiteral("LLM"), Qt::CaseInsensitive) == 0) {
        const int mode = complexItem.value(QStringLiteral("planMode")).toInt(0);
        switch (mode) {
        case 1: type = tr("Spiral route"); break;
        case 2: type = tr("FOM route"); break;
        case 3: type = tr("Custom route"); break;
        case 4: type = tr("Square route"); break;
        default: type = tr("Language plan"); break;
        }
        icon = QStringLiteral("/qmlimages/PlanCreator/LLMPlanCreator.png");
    } else if (complexType.compare(QStringLiteral("Sketch"), Qt::CaseInsensitive) == 0) {
        type = tr("Sketch route");
        icon = QStringLiteral("/qmlimages/PlanCreator/SketchPlanCreator.png");
    } else if (complexType.compare(QStringLiteral("survey"), Qt::CaseInsensitive) == 0) {
        type = tr("Survey route");
        icon = QStringLiteral("/qmlimages/PlanCreator/SurveyPlanCreator.png");
    } else if (complexType.compare(QStringLiteral("CorridorScan"), Qt::CaseInsensitive) == 0) {
        type = tr("Corridor scan");
        icon = QStringLiteral("/qmlimages/MapDrawShape.svg");
    } else if (complexType.compare(QStringLiteral("StructureScan"), Qt::CaseInsensitive) == 0) {
        type = tr("Structure scan");
        icon = QStringLiteral("/qmlimages/MapDrawShape.svg");
    }
}
