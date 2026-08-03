#ifndef PYTHON_BRIDGE_H
#define PYTHON_BRIDGE_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <functional>

#include "Geometry.h"

class PythonBridge
{
public:
    typedef std::function<void(const QString&, const QString&)> LogCallback;

    struct Result {
        bool success = false;
        double totalArea = 0.0;
        double backfillArea = 0.0;
    };

    static QJsonArray pointToJson(const Point2D &pt);
    static QJsonArray pointsToJsonArray(const QVector<Point2D> &pts);
    static QJsonObject lineToJson(const Line2D &line);

    static Result run(const QString &inputPath, const QString &outputDxfPath,
                      const QString &outputXlsxPath, const QJsonObject &jsonData,
                      LogCallback log);
};

#endif // PYTHON_BRIDGE_H
