/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MagneticMapController.h"

#include "Common/MagLabPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QtMath>

#include <QVector>
#include <algorithm>
#include <cmath>

namespace {

constexpr double kEarthRadiusMeters = 6378137.0;
constexpr double kGeometryEpsilon = 1e-9;
constexpr double kMercatorLatitudeLimit = 85.05112878;

double mercatorNorthing(double latitude)
{
    return kEarthRadiusMeters * std::log(std::tan(qDegreesToRadians(45.0 + latitude / 2.0)));
}

struct Sample {
    double x = 0.0;
    double y = 0.0;
    double value = 0.0;
    double latitude = 0.0;
    double longitude = 0.0;
};

struct Edge { int a = -1; int b = -1; };
struct Triangle { int a = -1; int b = -1; int c = -1; };

bool sameEdge(const Edge& lhs, const Edge& rhs)
{
    return (lhs.a == rhs.a && lhs.b == rhs.b) ||
           (lhs.a == rhs.b && lhs.b == rhs.a);
}

double median(QVector<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const int middle = values.size() / 2;
    return values.size() % 2 ? values[middle]
                             : (values[middle - 1] + values[middle]) * 0.5;
}

void hampelFilter(QVector<Sample>& samples)
{
    if (samples.size() < 5) {
        return;
    }

    QVector<double> filtered;
    filtered.reserve(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        QVector<double> window;
        for (int j = qMax(0, i - 3); j <= qMin(static_cast<int>(samples.size()) - 1, i + 3); ++j) {
            window.append(samples[j].value);
        }
        const double localMedian = median(window);
        QVector<double> deviations;
        deviations.reserve(window.size());
        for (double value : window) {
            deviations.append(qAbs(value - localMedian));
        }
        const double mad = median(deviations);
        const double threshold = qMax(100.0, 3.0 * 1.4826 * mad);
        filtered.append(qAbs(samples[i].value - localMedian) > threshold
                        ? localMedian : samples[i].value);
    }
    for (int i = 0; i < samples.size(); ++i) {
        samples[i].value = filtered[i];
    }
}

bool circumcircleContains(const QVector<Sample>& points, const Triangle& triangle, const Sample& point)
{
    const Sample& a = points[triangle.a];
    const Sample& b = points[triangle.b];
    const Sample& c = points[triangle.c];
    const double d = 2.0 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (qAbs(d) < kGeometryEpsilon) {
        return false;
    }

    const double aa = a.x * a.x + a.y * a.y;
    const double bb = b.x * b.x + b.y * b.y;
    const double cc = c.x * c.x + c.y * c.y;
    const double ux = (aa * (b.y - c.y) + bb * (c.y - a.y) + cc * (a.y - b.y)) / d;
    const double uy = (aa * (c.x - b.x) + bb * (a.x - c.x) + cc * (b.x - a.x)) / d;
    const double radiusSquared = (ux - a.x) * (ux - a.x) + (uy - a.y) * (uy - a.y);
    const double distanceSquared = (ux - point.x) * (ux - point.x) + (uy - point.y) * (uy - point.y);
    return distanceSquared <= radiusSquared + qMax(1.0, radiusSquared) * 1e-10;
}

QVector<Triangle> delaunayTriangulation(const QVector<Sample>& input)
{
    QVector<Sample> points = input;
    double minX = input.first().x;
    double maxX = minX;
    double minY = input.first().y;
    double maxY = minY;
    for (const Sample& sample : input) {
        minX = qMin(minX, sample.x);
        maxX = qMax(maxX, sample.x);
        minY = qMin(minY, sample.y);
        maxY = qMax(maxY, sample.y);
    }

    const double span = qMax(maxX - minX, maxY - minY);
    const double centerX = (minX + maxX) * 0.5;
    const double centerY = (minY + maxY) * 0.5;
    const double scale = qMax(1.0, span) * 32.0;
    const int superA = points.size();
    points.append({centerX - 2.0 * scale, centerY - scale, 0.0, 0.0, 0.0});
    const int superB = points.size();
    points.append({centerX, centerY + 2.0 * scale, 0.0, 0.0, 0.0});
    const int superC = points.size();
    points.append({centerX + 2.0 * scale, centerY - scale, 0.0, 0.0, 0.0});

    QVector<Triangle> triangles;
    triangles.append({superA, superB, superC});

    for (int pointIndex = 0; pointIndex < input.size(); ++pointIndex) {
        QVector<Edge> polygon;
        QVector<Triangle> retained;
        for (const Triangle& triangle : triangles) {
            if (circumcircleContains(points, triangle, points[pointIndex])) {
                polygon.append({triangle.a, triangle.b});
                polygon.append({triangle.b, triangle.c});
                polygon.append({triangle.c, triangle.a});
            } else {
                retained.append(triangle);
            }
        }

        QVector<Edge> boundary;
        for (int i = 0; i < polygon.size(); ++i) {
            bool shared = false;
            for (int j = 0; j < polygon.size(); ++j) {
                if (i != j && sameEdge(polygon[i], polygon[j])) {
                    shared = true;
                    break;
                }
            }
            if (!shared) {
                boundary.append(polygon[i]);
            }
        }

        triangles = retained;
        for (const Edge& edge : boundary) {
            triangles.append({edge.a, edge.b, pointIndex});
        }
    }

    QVector<Triangle> result;
    for (const Triangle& triangle : triangles) {
        if (triangle.a < input.size() && triangle.b < input.size() && triangle.c < input.size()) {
            result.append(triangle);
        }
    }
    return result;
}

bool interpolate(const QVector<Sample>& samples, const Triangle& triangle,
                 double x, double y, double& value)
{
    const Sample& a = samples[triangle.a];
    const Sample& b = samples[triangle.b];
    const Sample& c = samples[triangle.c];
    const double denominator = (b.y - c.y) * (a.x - c.x) +
                               (c.x - b.x) * (a.y - c.y);
    if (qAbs(denominator) < kGeometryEpsilon) {
        return false;
    }
    const double wA = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / denominator;
    const double wB = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / denominator;
    const double wC = 1.0 - wA - wB;
    if (wA < -1e-7 || wB < -1e-7 || wC < -1e-7) {
        return false;
    }
    value = wA * a.value + wB * b.value + wC * c.value;
    return true;
}

QRgb jetColor(double normalized)
{
    const double t = qBound(0.0, normalized, 1.0);
    const auto channel = [t](double center) {
        return qBound(0, qRound(255.0 * qBound(0.0, 1.5 - qAbs(4.0 * t - center), 1.0)), 255);
    };
    return qRgba(channel(3.0), channel(2.0), channel(1.0), 230);
}

} // namespace

MagneticMapController::MagneticMapController(QObject* parent)
    : QObject(parent)
{
    QDir().mkpath(defaultFolder());
}

QString MagneticMapController::defaultFolder() const
{
    // Survey CSV data is written by MagSurveyController into
    // Documents/MagLab/survey; default the file picker there.
    return MagLabPaths::instance()->surveyDir();
}

void MagneticMapController::_setError(const QString& error)
{
    if (_errorString == error) {
        return;
    }
    _errorString = error;
    emit errorStringChanged();
}

void MagneticMapController::_setBusy(bool busy)
{
    if (_busy == busy) {
        return;
    }
    _busy = busy;
    emit busyChanged();
}

void MagneticMapController::clear()
{
    _imageUrl = QUrl();
    _northWest = QGeoCoordinate();
    _southEast = QGeoCoordinate();
    _sourceFile.clear();
    _sampleCount = 0;
    _minimumValue = 0.0;
    _maximumValue = 0.0;
    _setError(QString());
    emit mapChanged();
}

bool MagneticMapController::loadFile(const QString& filePath, int resolution)
{
    if (_busy) {
        return false;
    }
    _setBusy(true);
    _setError(QString());

    const QString localPath = filePath.startsWith(QStringLiteral("file:"))
            ? QUrl(filePath).toLocalFile() : filePath;
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        _setError(tr("Unable to open %1").arg(QFileInfo(localPath).fileName()));
        _setBusy(false);
        return false;
    }

    QVector<Sample> samples;
    QTextStream stream(&file);
    const QRegularExpression whitespaceSeparator(QStringLiteral("\\s+"));
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        // Preserve empty delimited columns: dropping one shifts sensor/coordinate indices.
        // Whitespace-only TXT files use a separate splitting rule.
        const QChar delimiter = line.contains(QLatin1Char(',')) ? QLatin1Char(',')
                              : line.contains(QLatin1Char(';')) ? QLatin1Char(';')
                              : QLatin1Char('\t');
        const QStringList fields = line.contains(delimiter)
                ? line.split(delimiter, Qt::KeepEmptyParts)
                : line.split(whitespaceSeparator, Qt::SkipEmptyParts);

        bool lonOk = false;
        bool latOk = false;
        bool valueOk = false;
        double longitude = 0.0;
        double latitude = 0.0;
        double value = 0.0;

        if (fields.size() >= 8) {
            longitude = fields[1].toDouble(&lonOk);
            latitude = fields[2].toDouble(&latOk);
            bool firstOk = false;
            bool secondOk = false;
            const double first = fields[6].toDouble(&firstOk);
            const double second = fields[7].toDouble(&secondOk);
            valueOk = firstOk && secondOk;
            value = first - second;
        } else if (fields.size() >= 7) {
            longitude = fields[0].toDouble(&lonOk);
            latitude = fields[1].toDouble(&latOk);
            bool firstOk = false;
            bool secondOk = false;
            const double first = fields[5].toDouble(&firstOk);
            const double second = fields[6].toDouble(&secondOk);
            valueOk = firstOk && secondOk;
            value = first - second;
        } else if (fields.size() >= 3) {
            longitude = fields[0].toDouble(&lonOk);
            latitude = fields[1].toDouble(&latOk);
            value = fields[2].toDouble(&valueOk);
        }

        if (!lonOk || !latOk || !valueOk || !qIsFinite(value)
                || !qIsFinite(latitude) || !qIsFinite(longitude)
                || qAbs(latitude) > kMercatorLatitudeLimit || qAbs(longitude) > 180.0) {
            continue;
        }
        samples.append({0.0, 0.0, value, latitude, longitude});
    }

    if (samples.size() < 3) {
        _setError(tr("At least three valid magnetic samples are required."));
        _setBusy(false);
        return false;
    }

    hampelFilter(samples);

    double minLatitude = samples.first().latitude;
    double maxLatitude = minLatitude;
    double minLongitude = samples.first().longitude;
    double maxLongitude = minLongitude;
    double minValue = samples.first().value;
    double maxValue = minValue;
    double meanLatitude = 0.0;
    double meanLongitude = 0.0;
    for (const Sample& sample : samples) {
        minLatitude = qMin(minLatitude, sample.latitude);
        maxLatitude = qMax(maxLatitude, sample.latitude);
        minLongitude = qMin(minLongitude, sample.longitude);
        maxLongitude = qMax(maxLongitude, sample.longitude);
        minValue = qMin(minValue, sample.value);
        maxValue = qMax(maxValue, sample.value);
        meanLatitude += sample.latitude;
        meanLongitude += sample.longitude;
    }
    meanLatitude /= samples.size();
    meanLongitude /= samples.size();

    if (qFuzzyCompare(minLatitude, maxLatitude) || qFuzzyCompare(minLongitude, maxLongitude)) {
        _setError(tr("Magnetic samples must cover a two-dimensional geographic area."));
        _setBusy(false);
        return false;
    }

    if (maxLongitude - minLongitude >= 180.0) {
        _setError(tr("Magnetic maps crossing the antimeridian are not supported."));
        _setBusy(false);
        return false;
    }

    // Raster rows must use the same Web Mercator projection as MapQuickItem.
    // Subtract a local origin to retain precision in the triangulation.
    const double referenceNorthing = mercatorNorthing(meanLatitude);
    for (Sample& sample : samples) {
        sample.x = kEarthRadiusMeters * qDegreesToRadians(sample.longitude - meanLongitude);
        sample.y = mercatorNorthing(sample.latitude) - referenceNorthing;
    }

    const QVector<Triangle> triangles = delaunayTriangulation(samples);
    if (triangles.isEmpty()) {
        _setError(tr("The sample geometry cannot be triangulated."));
        _setBusy(false);
        return false;
    }

    double minX = samples.first().x;
    double maxX = minX;
    double minY = samples.first().y;
    double maxY = minY;
    for (const Sample& sample : samples) {
        minX = qMin(minX, sample.x);
        maxX = qMax(maxX, sample.x);
        minY = qMin(minY, sample.y);
        maxY = qMax(maxY, sample.y);
    }

    resolution = qBound(96, resolution, 768);
    QImage image(resolution, resolution, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    const double valueSpan = maxValue - minValue;
    for (int row = 0; row < resolution; ++row) {
        const double y = maxY - (maxY - minY) * row / qMax(1, resolution - 1);
        QRgb* scanLine = reinterpret_cast<QRgb*>(image.scanLine(row));
        for (int column = 0; column < resolution; ++column) {
            const double x = minX + (maxX - minX) * column / qMax(1, resolution - 1);
            double interpolated = 0.0;
            bool found = false;
            for (const Triangle& triangle : triangles) {
                if (interpolate(samples, triangle, x, y, interpolated)) {
                    found = true;
                    break;
                }
            }
            if (found) {
                const double normalized = qAbs(valueSpan) < kGeometryEpsilon
                        ? 0.5 : (interpolated - minValue) / valueSpan;
                scanLine[column] = jetColor(normalized);
            }
        }
    }

    const QString outputDirectory = MagLabPaths::instance()->magMapDir();
    if (outputDirectory.isEmpty()) {
        _setError(tr("Unable to create the magnetic map cache directory."));
        _setBusy(false);
        return false;
    }
    const QString outputPath = QDir(outputDirectory).filePath(QStringLiteral("fly-view-overlay.png"));
    if (!image.save(outputPath, "PNG")) {
        _setError(tr("Unable to save the generated magnetic map."));
        _setBusy(false);
        return false;
    }

    _sourceFile = localPath;
    _northWest = QGeoCoordinate(maxLatitude, minLongitude);
    _southEast = QGeoCoordinate(minLatitude, maxLongitude);
    _sampleCount = samples.size();
    _minimumValue = minValue;
    _maximumValue = maxValue;
    QUrl imageUrl = QUrl::fromLocalFile(outputPath);
    imageUrl.setQuery(QStringLiteral("revision=%1").arg(++_revision));
    _imageUrl = imageUrl;
    _setBusy(false);
    emit mapChanged();
    return true;
}
