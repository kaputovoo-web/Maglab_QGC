/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class PlanMasterController;
class Vehicle;

/// Persistent archive for locally planned missions and their execution records.
///
/// The archive deliberately writes plan snapshots without changing the regular
/// PlanMasterController current file or dirty state. This makes it an autosave
/// safety net instead of a replacement for the user's explicit Save command.
class MissionArchiveManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_MOC_INCLUDE("PlanMasterController.h")

    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList executionRecords READ executionRecords NOTIFY entriesChanged)
    Q_PROPERTY(QVariantMap latestRecord READ latestRecord NOTIFY latestRecordChanged)
    Q_PROPERTY(QVariantMap currentEntry READ currentEntry NOTIFY currentEntryChanged)
    Q_PROPERTY(bool executionActive READ executionActive NOTIFY executionChanged)
    Q_PROPERTY(double executionProgress READ executionProgress NOTIFY executionChanged)

public:
    explicit MissionArchiveManager(QObject* parent = nullptr);

    QVariantList entries() const;
    QVariantList executionRecords() const;
    QVariantMap latestRecord() const;
    QVariantMap currentEntry() const;
    bool executionActive() const { return !_executionEntryId.isEmpty(); }
    double executionProgress() const { return _executionProgress; }

    Q_INVOKABLE void attachPlanController(PlanMasterController* controller);
    Q_INVOKABLE void saveNow();
    Q_INVOKABLE bool loadEntry(int index, PlanMasterController* controller);
    Q_INVOKABLE void updateEntry(int index, const QString& name, const QString& iconSource);
    Q_INVOKABLE void renameEntry(int index, const QString& name);
    Q_INVOKABLE void setEntryIcon(int index, const QString& iconSource);
    Q_INVOKABLE void removeEntry(int index);

    Q_INVOKABLE void startExecution(int totalItems, int firstNavigationIndex, int lastNavigationIndex);
    void observeVehicle(Vehicle* vehicle);
    Q_INVOKABLE void updateExecution(int currentItem, int totalItems);
    Q_INVOKABLE void missionItemReached(int sequenceNumber);
    Q_INVOKABLE void finishExecution(bool completed);

signals:
    void entriesChanged();
    void latestRecordChanged();
    void currentEntryChanged();
    void executionChanged();
    void archiveError(const QString& message);

private slots:
    void _scheduleAutosave();
    void _autosaveTimeout();
    void _planContainsItemsChanged(bool containsItems);
    void _currentPlanFileChanged();

private:
    void _observeMissionIndex(int index);
    void _endObservedExecution();
    QPointer<Vehicle> _observedVehicle;
    QList<QMetaObject::Connection> _vehicleConnections;
    bool _startingObservedExecution = false;
    bool _observedExecution = false;
    QString _archiveRoot() const;
    QString _indexPath() const;
    QString _planPath(const QString& id) const;
    void _loadIndex();
    bool _writeIndex();
    bool _writePlan(const QString& filePath, const QByteArray& bytes);
    void _snapshotNow();
    int _findEntryById(const QString& id) const;
    int _latestRecordIndex() const;
    void _sortEntries();
    QVariantMap _entryVariant(const QJsonObject& entry) const;
    void _notifyAll();
    static QJsonObject _findComplexItem(const QJsonValue& value);
    static void _detectPlanPresentation(const QJsonDocument& plan, QString& type, QString& icon);

    QJsonArray _archiveEntries;
    QPointer<PlanMasterController> _planController;
    QTimer _autosaveTimer;
    QString _workingEntryId;
    QString _executionEntryId;
    QString _executionRunId;
    double _executionProgress = 0.0;
    bool _loadingArchive = false;
};
