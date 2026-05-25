#ifndef DXF_WRAPPER_H
#define DXF_WRAPPER_H

#include "Geometry.h"
#include "utils/EntityHelper.h"
#include "utils/LayerExtractor.h"
#include "Config.h"
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QColor>
#include <functional>

class DXFWrapper
{
public:
    typedef std::function<void(const QString&, const QString&)> LogCallback;
    
    struct TextInfo {
        QString text;
        QString layer;
        double x;
        double y;
        double height = 3.0;
        QColor rgbColor;
        
        TextInfo() : x(0), y(0) {}
    };
    
    // 文件操作
    bool read(const QString &filePath);
    bool save(const QString &filePath = QString());
    DXFWrapper createCopy();
    
    // 图层管理
    QStringList getLayers();
    bool hasLayer(const QString &layerName);
    void createLayer(const QString &layerName, int color = 7);
    bool isLayerVisible(const QString &layerName);
    
    // 实体查询
    QVector<void*> queryEntities(const QString &layerName, const QString &entityType = "*");
    QVector<Line2D> getLines(const QString &layerName);
    QVector<TextInfo> getTexts(const QString &layerPattern = QString());
    QVector<Polygon2D> getHatches(const QString &layerName);
    
    // 实体创建
    void addLWPolyline(const QVector<Point2D> &points, const QString &layerName, int color = -1);
    void addLine(const Point2D &start, const Point2D &end, const QString &layerName, int color = -1);
    void addHatch(const Polygon2D &polygon, const QString &layerName, const QString &pattern = "ANSI31", double scale = 1.0, const QColor &rgbColor = QColor());
    void addMText(const QString &content, const Point2D &position, double height, const QString &layerName, const QColor &rgbColor = QColor());
    
    // 属性
    bool isLoaded() const { return m_loaded; }
    QString filePath() const { return m_filePath; }
    
// 图层属性结构
    struct LayerInfo {
        QString name;
        int color = 7;          // AutoCAD颜色索引 (1-255)
        QString linetype = "Continuous";
        int lineweight = -3;    // 线宽 (默认-3 = ByLayer)
        bool frozen = false;
        bool locked = false;
    };

private:
    QString m_filePath;
    bool m_loaded = false;
    QVector<Line2D> m_lines;
    QVector<TextInfo> m_texts;
    QVector<Polygon2D> m_hatches;
    QStringList m_layers;
    QMap<QString, LayerInfo> m_layerInfo;  // 图层属性映射
    QStringList m_originalHeader;          // 原始HEADER内容（从源文件复制）

public:
    // 获取图层属性
    LayerInfo getLayerInfo(const QString &layerName) const {
        return m_layerInfo.value(layerName, LayerInfo());
    }
    QMap<QString, LayerInfo> getAllLayerInfo() const { return m_layerInfo; }
};

#endif // DXF_WRAPPER_H