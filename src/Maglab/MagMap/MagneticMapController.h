/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QGeoCoordinate>
#include <QObject>
#include <QUrl>
#include <QtQmlIntegration/QtQmlIntegration>

/// Loads offline magnetic survey samples and generates a georeferenced raster
/// for display in Fly View. The source format is compatible with the original
/// Handle implementation: longitude and latitude are columns 2 and 3, while
/// the mapped value is column 7 minus column 8. A simple lon/lat/value file is
/// accepted as well.
class MagneticMapController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl imageUrl READ imageUrl NOTIFY mapChanged)
    Q_PROPERTY(QGeoCoordinate northWest READ northWest NOTIFY mapChanged)
    Q_PROPERTY(QGeoCoordinate southEast READ southEast NOTIFY mapChanged)
    Q_PROPERTY(QString sourceFile READ sourceFile NOTIFY mapChanged)
    Q_PROPERTY(QString defaultFolder READ defaultFolder CONSTANT)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(bool hasMap READ hasMap NOTIFY mapChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY mapChanged)
    Q_PROPERTY(double minimumValue READ minimumValue NOTIFY mapChanged)
    Q_PROPERTY(double maximumValue READ maximumValue NOTIFY mapChanged)

public:
    explicit MagneticMapController(QObject* parent = nullptr);

    QUrl imageUrl() const { return _imageUrl; }
    QGeoCoordinate northWest() const { return _northWest; }
    QGeoCoordinate southEast() const { return _southEast; }
    QString sourceFile() const { return _sourceFile; }
    QString defaultFolder() const;
    QString errorString() const { return _errorString; }
    bool hasMap() const { return !_imageUrl.isEmpty() && _northWest.isValid() && _southEast.isValid(); }
    bool busy() const { return _busy; }
    int sampleCount() const { return _sampleCount; }
    double minimumValue() const { return _minimumValue; }
    double maximumValue() const { return _maximumValue; }

    Q_INVOKABLE bool loadFile(const QString& filePath, int resolution = 256);
    Q_INVOKABLE void clear();

signals:
    void mapChanged();
    void errorStringChanged();
    void busyChanged();

private:
    void _setError(const QString& error);
    void _setBusy(bool busy);

    QUrl _imageUrl;
    QGeoCoordinate _northWest;
    QGeoCoordinate _southEast;
    QString _sourceFile;
    QString _errorString;
    bool _busy = false;
    int _sampleCount = 0;
    double _minimumValue = 0.0;
    double _maximumValue = 0.0;
    quint64 _revision = 0;
};
