#ifndef LAYER_EXTRACTOR_H
#define LAYER_EXTRACTOR_H

#include "Geometry.h"
#include "EntityHelper.h"
#include <QString>
#include <QVector>
#include <QRegularExpression>

/**
 * LayerExtractor类 - 复刻Python engine_cad_v3.py LayerExtractor类
 * 
 * 图层提取工具集：
 * - get_lines: 从指定图层提取所有线段
 * - get_texts: 提取文本实体
 * - get_polylines_by_color: 按颜色获取多段线
 */
class LayerExtractor
{
public:
    /**
     * 文本信息结构
     */
    struct TextInfo {
        QString text;
        double x;
        double y;
        // void *entity;  // 实际实体指针（dxflib）
        
        TextInfo() : text(""), x(0), y(0) {}
        TextInfo(const QString &t, double px, double py) : text(t), x(px), y(py) {}
    };
    
    /**
     * 多段线信息结构
     */
    struct PolylineInfo {
        Line2D line;
        double x;     // 中心X
        double y;     // 中心Y
        // void *entity;  // 实际实体指针
        
        PolylineInfo() : x(0), y(0) {}
    };
    
    /**
     * 从指定图层提取所有线段
     * 
     * 对应Python: engine_cad_v3.py 第135-143行
     */
    static QVector<Line2D> getLines(const QString &layer) {
        QVector<Line2D> lines;
        
        // 注意：实际实现需要dxflib库支持
        // for (e in msp.query(f'*[layer=="{layer}"]')) {
        //     ls = EntityHelper::to_linestring(e);
        //     if (ls.isValid()) {
        //         lines.append(ls);
        //     }
        // }
        
        return lines;
    }
    
    /**
     * 提取文本实体
     * 
     * 对应Python: engine_cad_v3.py 第145-157行
     */
    static QVector<TextInfo> getTexts(const QString &layerPattern = QString()) {
        QVector<TextInfo> texts;
        
        // 注意：实际实现需要dxflib库支持
        // for (e in msp.query('TEXT MTEXT')) {
        //     try {
        //         if (!layerPattern.isEmpty() && !e.dxf.layer.contains(layerPattern)) {
        //             continue;
        //         }
        //         
        //         pt = EntityHelper::get_best_point(e);
        //         txt = EntityHelper::get_text(e);
        //         texts.append(TextInfo(txt, pt.x, pt.y, e));
        //     } catch (...) {}
        // }
        
        return texts;
    }
    
    /**
     * 按颜色获取多段线
     * 
     * 对应Python: engine_cad_v3.py 第159-169行
     */
    static QVector<PolylineInfo> getPolylinesByColor(int color) {
        QVector<PolylineInfo> results;
        
        // 注意：实际实现需要dxflib库支持
        // for (e in msp.query('LWPOLYLINE')) {
        //     try {
        //         if (e.dxf.color == color) {
        //             pts = e.get_points();
        //             PolylineInfo info;
        //             info.line = Line2D(pts);
        //             info.x = info.line.midX();
        //             info.y = info.line.midY();
        //             info.entity = e;
        //             results.append(info);
        //         }
        //     } catch (...) {}
        // }
        
        return results;
    }
    
    /**
     * 检测地层图层
     * 
     * 地层图层命名规则：以数字开头，如"1级淤泥"、"3级粘土"等
     */
    static QStringList detectStrataLayers(const QStringList &allLayers) {
        QStringList strataLayers;
        
        QRegularExpression re(QStringLiteral("^\\d+级"));
        for (const QString &layer : allLayers) {
            if (re.match(layer).hasMatch()) {
                strataLayers.append(layer);
            }
        }
        
        // 按级别排序
        std::sort(strataLayers.begin(), strataLayers.end(), 
                  [](const QString &a, const QString &b) {
                      // 提取数字
                      QRegularExpression numRe(QStringLiteral("^\\d+"));
                      int numA = numRe.match(a).hasMatch() ? numRe.match(a).captured().toInt() : 999;
                      int numB = numRe.match(b).hasMatch() ? numRe.match(b).captured().toInt() : 999;
                      return numA < numB;
                  });
        
        return strataLayers;
    }
    
    /**
     * 获取图层上的HATCH填充实体
     */
    static QVector<Polygon2D> getHatches(const QString &layer) {
        QVector<Polygon2D> hatches;
        
        // 注意：实际实现需要dxflib库支持
        // for (h in msp.query(f'HATCH[layer=="{layer}"]')) {
        //     poly = HatchProcessor::to_polygon(h);
        //     if (poly.isValid() && !poly.isEmpty()) {
        //         hatches.append(poly);
        //     }
        // }
        
        return hatches;
    }
};

#endif // LAYER_EXTRACTOR_H