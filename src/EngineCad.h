#ifndef ENGINE_CAD_H
#define ENGINE_CAD_H

#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <functional>
#include <QDateTime>

#include "Geometry.h"
#include "DXFWrapper.h"

/**
 * 核心CAD计算引擎 - 完整重构版
 * 
 * 复刻Python engine_cad_v3.py的全部六大任务：
 * 1. runAutoline - 断面合并（包络线）
 * 2. runAutopaste - 批量粘贴（桩号匹配v2）
 * 3. runAutohatch - 快速填充
 * 4. runAutosection - 分层算量
 * 5. runBackfill - 回淤计算
 * 6. runAutosectionBackfill - 分层+回淤合并
 */
class EngineCad
{
public:
    typedef std::function<void(const QString&, const QString&)> LogCallback;

    EngineCad();

    // ==================== 六大核心任务 ====================
    bool runAutoline(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runAutopaste(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runAutohatch(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runAutosection(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runBackfill(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);
    bool runAutosectionBackfill(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result);

private:
    // ==================== 辅助函数 ====================
    QString getOutputPath(const QString &inputPath, const QString &suffix, const QString &outputDir);
    double getYAtX(const Line2D &line, double x);
    Line2D generateEnvelope(const Line2D &baseLine, const QVector<Line2D> &sectionLines, const QString &envelopeType);
    QVector<Line2D> extractLinesFromDXF(const QString &filePath, const QString &layer);
    bool writeDXFWithLines(const QString &filePath, const QString &layer, const QVector<Line2D> &lines);
    bool appendDXFWithHatch(const QString &inputPath, const QString &outputPath, const QString &layer, const QVector<Polygon2D> &polygons);

    /**
     * 用Python脚本处理DXF输出（确保兼容性）
     * 带新增LINE实体版本
     */
    bool saveDXFWithPython(const QString &inputPath, const QString &outputPath,
                           DXFWrapper &dxf, const QVector<Polygon2D> &newHatches,
                           const QVector<Line2D> &newLines,
                           LogCallback log);

    /**
     * 用Python脚本处理DXF输出（确保兼容性）
     * 只处理HATCH版本（兼容旧调用）
     */
    bool saveDXFWithPython(const QString &inputPath, const QString &outputPath,
                           DXFWrapper &dxf, const QVector<Polygon2D> &newHatches,
                           LogCallback log);

    /**
     * 调用Python脚本进行精确多边形计算 + DXF/Excel输出
     * result输出参数：totalArea/backfillArea由Python返回
     */
    bool runPythonComputation(const QString &inputPath, const QString &outputDxfPath,
                              const QString &outputXlsxPath,
                              const QJsonObject &jsonData,
                              LogCallback log,
                              QJsonObject &result);

    // ==================== 内部辅助结构 ====================
    
    /**
     * 实体列表数据（用于DMX等）
     */
    struct EntityListData {
        Line2D line;
        double xMin, xMax, yMin, yMax;
        double xCenter, yCenter;
        QVector<Point2D> pts;
    };
    
    /**
     * 小矩形信息（批量粘贴）
     */
    struct SmallRectInfo {
        Box2D bbox;
        Point2D basepoint;
        double centerY;
    };
    
    /**
     * 断面曲线信息（批量粘贴）
     */
    struct CurveInfo {
        Line2D line;
        double centerX;
        double centerY;
        int vertexCount;
    };
    
    /**
     * 源套组信息（批量粘贴）
     */
    struct SourceSetInfo {
        int index;
        Point2D basepoint;
        double centerY;
        int station;
        QString stationText;
        Line2D curve;
        bool hasCurve = false;
    };
    
    /**
     * 基点信息（批量粘贴）
     */
    struct BasepointInfo {
        double x;
        double y;
    };
    
    /**
     * 桩号匹配信息（批量粘贴）
     */
    struct StationMatchInfo {
        QString text;
        int value;
        double x;
        double y;
    };
    
    /**
     * 目标套组信息（批量粘贴）
     */
    struct TargetSetInfo {
        Point2D basepoint;
        int station;
        QString stationText;
    };
    
    /**
     * 匹配对信息（批量粘贴）
     */
    struct MatchedPairInfo {
        SourceSetInfo source;
        TargetSetInfo target;
        int station;
    };
    
    // ==================== 内部辅助函数 ====================
    
    /**
     * 获取图层实体列表
     */
    QVector<EntityListData> getEntityList(DXFWrapper &dxf, const QString &layer);
    
    /**
     * 构建设计区多边形
     */
    Polygon2D buildDesignPolygon(const QVector<Line2D> &excavLines, double sectXMin, double sectXMax);
    
    // ==================== 批量粘贴辅助函数 ====================
    QVector<SmallRectInfo> detectSmallRects(DXFWrapper &dxf);
    QVector<CurveInfo> detectCurves(DXFWrapper &dxf);
    QVector<int> extractStationValues(DXFWrapper &dxf);
    QVector<BasepointInfo> detectL1Basepoints(DXFWrapper &dxf);
    QVector<StationMatchInfo> extractTargetStations(DXFWrapper &dxf);
    QVector<TargetSetInfo> matchBasepointsToStations(
        const QVector<BasepointInfo> &basepoints,
        const QVector<StationMatchInfo> &stations);
};

#endif // ENGINE_CAD_H