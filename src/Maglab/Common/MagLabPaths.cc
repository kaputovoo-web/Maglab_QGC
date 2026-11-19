#include "MagLabPaths.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryFile>
#include <QtCore/QSet>
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

QString MagLabPaths::surveyDir() { return scalarSurveyDir(); }
QString MagLabPaths::scalarSurveyDir()
{
    const QString target = _subDir(QStringLiteral("survey/scalar"));
    if (target.isEmpty()) {
        return {};
    }
    // Preserve pre-Scalar/Vector history without deleting or moving the original
    // files. Copy once into the new scalar directory so the normal file picker
    // and map import can see old scalar recordings.
    const QString legacy = _subDir(QStringLiteral("survey"));
    if (legacy.isEmpty() || legacy == target) {
        return target;
    }
    QDir targetDir(target);
    QDir legacyDir(legacy);
    const QFileInfoList files = legacyDir.entryInfoList(
                {QStringLiteral("*.csv"), QStringLiteral("*.txt")},
                QDir::Files | QDir::Readable);
    for (const QFileInfo& file : files) {
        const QString destination = targetDir.filePath(file.fileName());
        if (!QFile::exists(destination)) {
            QFile::copy(file.absoluteFilePath(), destination);
        }
    }
    return target;
}
QString MagLabPaths::vectorSurveyDir() { return _subDir(QStringLiteral("survey/vector")); }
QString MagLabPaths::surveyDirForMode(int mode)
{
    return mode == 1 ? vectorSurveyDir() : scalarSurveyDir();
}
QString MagLabPaths::magMapDir() { return _subDir(QStringLiteral("magmap")); }
QString MagLabPaths::photoDir() { return _subDir(QStringLiteral("photos")); }
QString MagLabPaths::photoMetaDir() { return _subDir(QStringLiteral("photos/metadata")); }
QString MagLabPaths::photoOverlayDir() { return _subDir(QStringLiteral("photo-overlays")); }

QStringList MagLabPaths::photoFiles()
{
    const QString dirPath = photoDir();
    if (dirPath.isEmpty()) {
        return {};
    }
    const QFileInfoList files = QDir(dirPath).entryInfoList(
                {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png")},
                QDir::Files | QDir::Readable, QDir::Time);
    QStringList result;
    result.reserve(files.size());
    for (const QFileInfo& info : files) {
        result.append(info.absoluteFilePath());
    }
    // Keep legacy magphoto captures visible: old versions wrote vis*.jpg and
    // txtvis*.txt into the magnetic map directory.
    const QString legacyDirPath = magMapDir();
    if (!legacyDirPath.isEmpty() && legacyDirPath != dirPath) {
        const QFileInfoList legacyFiles = QDir(legacyDirPath).entryInfoList(
                    {QStringLiteral("vis*.jpg"), QStringLiteral("vis*.jpeg"), QStringLiteral("vis*.png")},
                    QDir::Files | QDir::Readable, QDir::Time);
        for (const QFileInfo& info : legacyFiles) {
            const QString sidecar = info.dir().filePath(
                        QStringLiteral("txt%1.txt").arg(info.completeBaseName()));
            if (QFileInfo::exists(sidecar)) {
                result.append(info.absoluteFilePath());
            }
        }
    }
    return result;
}
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

QVariantList MagLabPaths::surveyFiles()
{
    QVariantList result;
    QSet<QString> seen;
    const QStringList dirs = { scalarSurveyDir(), vectorSurveyDir(), surveyDir() };
    for (const QString& dirPath : dirs) {
        if (dirPath.isEmpty()) continue;
        const QFileInfoList files = QDir(dirPath).entryInfoList(
                    {QStringLiteral("*.txt"), QStringLiteral("*.csv")},
                    QDir::Files | QDir::Readable, QDir::Time);
        for (const QFileInfo& info : files) {
            const QString absolute = info.absoluteFilePath();
            if (seen.contains(absolute)) continue;
            seen.insert(absolute);
            const qint64 bytes = info.size();
            QVariantMap entry;
            entry[QStringLiteral("name")] = info.fileName();
            entry[QStringLiteral("path")] = absolute;
            entry[QStringLiteral("size")] = bytes >= 1024 * 1024
                    ? QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 2)
                    : QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
            entry[QStringLiteral("modified")] = info.lastModified().toLocalTime()
                    .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
            entry[QStringLiteral("directory")] = info.absolutePath();
            result.append(entry);
        }
    }
    return result;
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
    // Keep old magphoto captures visible: they were stored in the magnetic
    // map folder as vis*.jpg with a txtvis*.txt sidecar.
    const QString legacyDirPath = magMapDir();
    if (!legacyDirPath.isEmpty() && legacyDirPath != dirPath) {
        const QFileInfoList legacyFiles = QDir(legacyDirPath).entryInfoList(
                    {QStringLiteral("vis*.jpg"), QStringLiteral("vis*.jpeg"), QStringLiteral("vis*.png")},
                    QDir::Files | QDir::Readable, QDir::Time);
        for (const QFileInfo& info : legacyFiles) {
            const QString sidecar = info.dir().filePath(
                        QStringLiteral("txt%1.txt").arg(info.completeBaseName()));
            if (QFileInfo::exists(sidecar)) {
                result.append(info.absoluteFilePath());
            }
        }
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
