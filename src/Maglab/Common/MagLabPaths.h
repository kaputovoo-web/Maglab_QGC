/****************************************************************************
 *
 * MagLabPaths — single entry point for all MagLab data storage locations and
 * the Android storage-permission flow.
 *
 * Every MagLab feature (survey capture, magnetic map, mission archive, RTSP
 * video) must obtain its directories through this class instead of hardcoding
 * a path. Android uses app-private data without broad storage permissions;
 * desktop preserves Documents/MagLab when writable.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include <QtQml/QQmlEngine>

class MagLabPaths : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool storageReady READ storageReady NOTIFY storageReadyChanged)

public:
    static MagLabPaths* instance();
    static MagLabPaths* create(QQmlEngine*, QJSEngine*)
    {
        MagLabPaths* paths = instance();
        QQmlEngine::setObjectOwnership(paths, QQmlEngine::CppOwnership);
        return paths;
    }

    Q_INVOKABLE QString rootDir();
    Q_INVOKABLE QString surveyDir();
    Q_INVOKABLE QString magMapDir();
    Q_INVOKABLE QString missionsDir();
    Q_INVOKABLE QString videoDir();

    Q_INVOKABLE QString newSurveyFile();
    Q_INVOKABLE QString newMagMapFile(const QString& suffix = QStringLiteral("png"));
    /// Newest-first list of generated magnetic map images in the private map directory.
    Q_INVOKABLE QStringList magMapFiles();

    bool storageReady() const { return _storageReady; }

    Q_INVOKABLE void requestStoragePermission();
    Q_INVOKABLE QString diagnostics();

signals:
    void storageReadyChanged();

private:
    explicit MagLabPaths(QObject* parent = nullptr);

    QString _subDir(const QString& name);
    QString _root;              // Documents/MagLab (resolved on first use)
    bool    _rootResolved = false;
    bool    _storageReady = false;
};
