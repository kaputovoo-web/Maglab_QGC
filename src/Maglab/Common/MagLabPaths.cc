#include "MagLabPaths.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryFile>
#include <QtCore/QUuid>

namespace {
bool ensureWritable(const QString &path)
{
    if (path.isEmpty() || !QDir().mkpath(path)) {
        return false;
    }
    QTemporaryFile probe(QDir(path).filePath(QStringLiteral(".maglab-write-XXXXXX")));
    return probe.open();
}
}

MagLabPaths::MagLabPaths(QObject *parent)
    : QObject(parent)
{
}

MagLabPaths *MagLabPaths::instance()
{
    static MagLabPaths paths;
    return &paths;
}

QString MagLabPaths::rootDir()
{
    if (!_rootResolved) {
#ifndef Q_OS_ANDROID
        // Preserve the desktop location, without requesting broad Android storage access.
        const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (!documents.isEmpty()) {
            const QString candidate = QDir(documents).filePath(QStringLiteral("MagLab"));
            if (ensureWritable(candidate)) {
                _root = candidate;
            }
        }
#endif
        if (_root.isEmpty()) {
            const QString data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            if (!data.isEmpty()) {
                const QString candidate = QDir(data).filePath(QStringLiteral("MagLab"));
                if (ensureWritable(candidate)) {
                    _root = candidate;
                }
            }
        }
        // Retry failures instead of permanently caching an inaccessible location.
        _rootResolved = !_root.isEmpty();
        const bool ready = _rootResolved;
        if (_storageReady != ready) {
            _storageReady = ready;
            emit storageReadyChanged();
        }
    }
    return _root;
}

QString MagLabPaths::_subDir(const QString &name)
{
    const QString root = rootDir();
    if (root.isEmpty()) {
        return {};
    }
    const QString dir = QDir(root).filePath(name);
    return ensureWritable(dir) ? dir : QString();
}

QString MagLabPaths::surveyDir() { return _subDir(QStringLiteral("survey")); }
QString MagLabPaths::magMapDir() { return _subDir(QStringLiteral("magmap")); }
QString MagLabPaths::missionsDir() { return _subDir(QStringLiteral("missions")); }
QString MagLabPaths::videoDir() { return _subDir(QStringLiteral("video")); }

QString MagLabPaths::newSurveyFile()
{
    const QString dir = surveyDir();
    if (dir.isEmpty()) {
        return {};
    }
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    return QDir(dir).filePath(QStringLiteral("mag_%1_%2.csv").arg(stamp, QUuid::createUuid().toString(QUuid::Id128)));
}

QString MagLabPaths::newMagMapFile(const QString &suffix)
{
    // A suffix must not introduce path separators or escape the managed directory.
    if (suffix.isEmpty()) {
        return {};
    }
    for (const QChar ch : suffix) {
        if (!ch.isLetterOrNumber()) {
            return {};
        }
    }
    const QString dir = magMapDir();
    if (dir.isEmpty()) {
        return {};
    }
    return QDir(dir).filePath(QStringLiteral("magmap_%1.%2")
                                 .arg(QUuid::createUuid().toString(QUuid::Id128), suffix));
}

QStringList MagLabPaths::magMapFiles()
{
    const QString dirPath = magMapDir();
    if (dirPath.isEmpty()) {
        return {};
    }
    QDir dir(dirPath);
    QFileInfoList files = dir.entryInfoList({"*.jpg", "*.jpeg", "*.png"}, QDir::Files | QDir::Readable, QDir::Time);
    QStringList result;
    result.reserve(files.size());
    for (const QFileInfo& info : files) {
        result.append(info.absoluteFilePath());
    }
    return result;
}

void MagLabPaths::requestStoragePermission()
{
    // Retained for old callers: managed storage needs no special Android permission.
    const bool ready = ensureWritable(rootDir());
    if (_storageReady != ready) {
        _storageReady = ready;
        emit storageReadyChanged();
    }
}

QString MagLabPaths::diagnostics()
{
    return QStringLiteral("root=%1\nstorage_ready=%2").arg(rootDir(), _storageReady ? QStringLiteral("true")
                                                                                  : QStringLiteral("false"));
}
