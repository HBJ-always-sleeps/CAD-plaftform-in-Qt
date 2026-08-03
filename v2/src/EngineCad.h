#ifndef ENGINE_CAD_H
#define ENGINE_CAD_H

#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QFile>
#include <QSet>
#include <functional>

#include "Geometry.h"
#include "DXFWrapper.h"
#include "StationMatcher.h"

class EngineCad
{
public:
    typedef std::function<void(const QString&, const QString&)> LogCallback;

    EngineCad();

    bool runAutoline(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runAutopaste(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runAutohatch(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runAutosection(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runBackfill(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runAutosectionBackfill(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runExcelMigration(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runGeologyTopview(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);

private:
    struct EntityListData {
        Line2D line;
        double xMin, xMax, yMin, yMax;
        double xCenter, yCenter;
    };

    // ---- Shared helpers for autosection/backfill ----
    struct SectionData {
        QString filePath;
        DXFWrapper dxf;
        QStringList allLayers;
        QStringList strataLayers;
        QVector<Line2D> allLines;
        QVector<Line2D> excavLines;
        QVector<Line2D> overexcLines;
        QVector<Line2D> designLines;  // for backfill task
        QVector<EntityListData> dmxList;
        QVector<StationMatcher::StationInfo> stations;
        QVector<StationMatcher::StationInfo> sortedStations;
    };

    enum class SectionTaskMode { Autosection, Backfill, Combined };

    bool loadSectionData(const QString &filePath, const QString &dmxLayer,
                         const QString &pileLayer, double targetElevation,
                         LogCallback log, SectionData &data);

    bool runSectionTask(SectionTaskMode mode, const QMap<QString, QString> &params,
                        LogCallback log, QJsonObject &result);

    QJsonObject buildSectionJson(const SectionData &data, const QString &station,
                                 int idx, const Box2D &boundaryBox,
                                 const QVector<Line2D> *auxLines,
                                 const QVector<Line2D> *updateLines);

    QJsonArray buildExcavJson(const QVector<Line2D> &excavLines);

    // ---- Layer matching ----
    static QVector<DXFWrapper::TextInfo> matchTextsByLayer(
        const QVector<DXFWrapper::TextInfo> &allTexts,
        const QString &exactLayer);

    // ---- Existing helpers ----
    QVector<EntityListData> getEntityList(DXFWrapper &dxf, const QString &layer);

    // ---- Autopaste helpers ----
    struct SmallRectInfo {
        Box2D bbox;
        Point2D basepoint;   // (center_x, top_y)
        double centerY;
    };

    struct CurveInfo {
        Box2D bbox;
        Point2D center;
        int vertexCount;
        int lineIndex;       // index into the source dxf lines vector
    };

    struct SourceSetInfo {
        int index;
        Box2D rectBbox;
        Point2D basepoint;
        double centerY;
        bool hasCurve;
        int curveLineIndex;
        QVector<Point2D> curvePoints;
        int station;
        QString stationText;
    };

    struct BasepointInfo {
        double x, y;
    };

    struct TargetStationInfo {
        QString text;
        int value;
        double x, y;
    };

    struct TargetSetInfo {
        Point2D basepoint;
        int station;
        QString stationText;
    };
};

#endif // ENGINE_CAD_H
