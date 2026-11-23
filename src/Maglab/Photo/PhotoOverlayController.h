#pragma once

#include <QObject>
#include <QImage>
#include <QFutureWatcher>
#include <QPointer>
#include <QSettings>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <limits>
#include <memory>

struct PhotoOverlayResult
{
    bool ok = false;
    bool cancelled = false;
    QString error;
    double geoMinLon = 0.0, geoMaxLon = 0.0, geoMinLat = 0.0, geoMaxLat = 0.0;
    double mapMinValue = 0.0, mapMaxValue = 0.0;
    double metersPerPixel = 0.0;
    QString outputPath;
    QImage image;
    int outputWidth = 0;
    int outputHeight = 0;
};

/// Refactored photo/magnetic-map overlay controller.
///
/// The original magphoto prototype read a photo sidecar and a magnetic-map
/// corner sidecar, projected the four map corners into the camera image and
/// warped the map with a perspective transform. This class keeps that core
/// geometry but uses MagLab-owned storage and the current QGC video pipeline.
class PhotoOverlayController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString previewPath READ previewPath NOTIFY previewPathChanged)
    Q_PROPERTY(bool hasPreview READ hasPreview NOTIFY previewPathChanged)
    Q_PROPERTY(double geoMinLon READ geoMinLon NOTIFY previewPathChanged)
    Q_PROPERTY(double geoMaxLon READ geoMaxLon NOTIFY previewPathChanged)
    Q_PROPERTY(double geoMinLat READ geoMinLat NOTIFY previewPathChanged)
    Q_PROPERTY(double geoMaxLat READ geoMaxLat NOTIFY previewPathChanged)
    Q_PROPERTY(double mapMinValue READ mapMinValue NOTIFY previewPathChanged)
    Q_PROPERTY(double mapMaxValue READ mapMaxValue NOTIFY previewPathChanged)
    Q_PROPERTY(double metersPerPixel READ metersPerPixel NOTIFY previewPathChanged)
    Q_PROPERTY(int outputWidth READ outputWidth NOTIFY outputSizeChanged)
    Q_PROPERTY(int outputHeight READ outputHeight NOTIFY outputSizeChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(double hfov READ hfov WRITE setHfov NOTIFY settingsChanged)
    Q_PROPERTY(double vfov READ vfov WRITE setVfov NOTIFY settingsChanged)
    Q_PROPERTY(double heightAGL READ heightAGL WRITE setHeightAGL NOTIFY settingsChanged)
    Q_PROPERTY(double yawOffset READ yawOffset WRITE setYawOffset NOTIFY settingsChanged)
    Q_PROPERTY(double transparency READ transparency WRITE setTransparency NOTIFY settingsChanged)

public:
    explicit PhotoOverlayController(QObject* parent = nullptr);
    ~PhotoOverlayController() override;

    bool busy() const { return _busy; }
    QString outputPath() const { return _outputPath; }
    QString previewPath() const { return _previewPath; }
    bool hasPreview() const { return !_previewImage.isNull(); }
    double geoMinLon() const { return _geoMinLon; }
    double geoMaxLon() const { return _geoMaxLon; }
    double geoMinLat() const { return _geoMinLat; }
    double geoMaxLat() const { return _geoMaxLat; }
    double mapMinValue() const { return _mapMinValue; }
    double mapMaxValue() const { return _mapMaxValue; }
    double metersPerPixel() const { return _metersPerPixel; }
    int outputWidth() const { return _outputWidth; }
    int outputHeight() const { return _outputHeight; }
    QString errorString() const { return _errorString; }
    QString statusText() const { return _statusText; }
    double hfov() const { return _hfov; }
    double vfov() const { return _vfov; }
    double heightAGL() const { return _heightAGL; }
    double yawOffset() const { return _yawOffset; }
    double transparency() const { return _transparency; }

    void setHfov(double value);
    void setVfov(double value);
    void setHeightAGL(double value);
    void setYawOffset(double value);
    void setTransparency(double value);

    /// Writes the camera pose beside a captured photo. The legacy two-line
    /// txtvis format remains readable; an optional third line stores altitude.
    Q_INVOKABLE QString writeSidecar(const QString& photoPath, double lon, double lat,
                                     double altitude, double roll, double pitch, double yaw);
    Q_INVOKABLE QString sidecarPath(const QString& photoPath) const;
    Q_INVOKABLE bool importPoseSidecar(const QString& photoPath, const QString& sidecarPath);
    Q_INVOKABLE bool hasPhotoSidecar(const QString& photoPath) const;
    Q_INVOKABLE bool hasMagSidecar(const QString& magImagePath) const;
    Q_INVOKABLE QVariantMap loadBundle(const QString& manifestPath);
    Q_INVOKABLE QString findAssociatedMap(const QString& photoPath) const;
    Q_INVOKABLE QVariantMap pixelToGeo(const QString& photoPath, double x, double y) const;
    /// Project a ground coordinate back to the raw photo pixel used by the
    /// interactive overlay preview.
    Q_INVOKABLE QVariantMap geoToPixel(const QString& photoPath, double lon, double lat) const;
    Q_INVOKABLE QVariantMap mapPixelToGeo(const QString& magImagePath, double x, double y) const;
    Q_INVOKABLE QVariantMap mapGeoInfo(const QString& magImagePath) const;
    Q_INVOKABLE QString saveOutput();
    Q_INVOKABLE double readSurveyAgl(const QString& surveyPath) const;
    Q_INVOKABLE void generate(const QString& photoPath, const QString& magImagePath,
                              double minValue = std::numeric_limits<double>::quiet_NaN(),
                              double maxValue = std::numeric_limits<double>::quiet_NaN(),
                              const QString& trajectoryPath = QString());
    Q_INVOKABLE void cancel();

signals:
    void busyChanged();
    void outputPathChanged();
    void previewPathChanged();
    void outputSizeChanged();
    void errorStringChanged();
    void statusTextChanged();
    void settingsChanged();
    void finished(bool success);

private slots:
    void _onFinished();

private:
    void _setBusy(bool busy);
    void _setError(const QString& error);
    void _setStatus(const QString& status);
    void _loadSettings();
    void _saveSettings(const QString& key, const QVariant& value);

    bool _busy = false;
    QString _outputPath;
    QString _previewPath;
    QImage _previewImage;
    int _outputWidth = 0;
    double _geoMinLon = 0.0, _geoMaxLon = 0.0, _geoMinLat = 0.0, _geoMaxLat = 0.0;
    double _mapMinValue = 0.0, _mapMaxValue = 0.0, _metersPerPixel = 0.0;
    QString _lastPhotoPath, _lastMagPath, _lastTrajectoryPath;
    double _lastMinValue = std::numeric_limits<double>::quiet_NaN();
    double _lastMaxValue = std::numeric_limits<double>::quiet_NaN();
    int _outputHeight = 0;
    QString _errorString;
    QString _statusText;
    double _hfov = 71.9;
    double _vfov = 49.3;
    double _heightAGL = 15.0;
    double _yawOffset = -13.0;
    double _transparency = 0.55;

    QSettings _settings;
    QFutureWatcher<PhotoOverlayResult>* _watcher = nullptr;
    std::shared_ptr<std::atomic_bool> _cancel;
};
