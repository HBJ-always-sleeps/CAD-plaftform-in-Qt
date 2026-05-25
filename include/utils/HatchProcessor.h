#ifndef HATCH_PROCESSOR_H
#define HATCH_PROCESSOR_H

#include "Geometry.h"
#include <QString>
#include <QColor>
#include <QVector>

/**
 * HatchProcessor类 - 复刻Python engine_cad_v3.py HatchProcessor类
 * 
 * 填充处理器：
 * - to_polygon: 填充转多边形
 * - add_with_label: 添加填充和标注
 * - add_simple: 添加简单填充（无标注）
 */
class HatchProcessor
{
public:
    /**
     * 填充转多边形
     * 
     * 对应Python: engine_cad_v3.py 第244-264行
     * 
     * 从DXF HATCH实体提取边界点，转换为Polygon2D
     * 
     * @param hatchEntity DXF填充实体指针
     * @return 多边形，如果失败返回空
     */
    static Polygon2D toPolygon(const void *hatchEntity) {
        Polygon2D result;
        
        // 注意：实际实现需要dxflib库支持
        // 
        // try {
        //     for (path in hatch.paths) {
        //         pts = [];
        //         
        //         // 边界路径类型检测
        //         if (path.has_vertices && path.vertices.size() > 0) {
        //             // Polyline路径
        //             for (v in path.vertices) {
        //                 pts.append(Point2D(v[0], v[1]));
        //             }
        //         } else if (path.has_edges) {
        //             // 边界边路径
        //             for (edge in path.edges) {
        //                 if (edge is LineEdge) {
        //                     pts.append(Point2D(edge.start[0], edge.start[1]));
        //                     pts.append(Point2D(edge.end[0], edge.end[1]));
        //                 } else if (edge is ArcEdge || edge is EllipseEdge) {
        //                     // 离散化曲线
        //                     flattened_pts = edge.flattening(distance=0.01);
        //                     for (p in flattened_pts) {
        //                         pts.append(Point2D(p.x, p.y));
        //                     }
        //                 }
        //             }
        //         }
        //         
        //         if (pts.size() >= 3) {
        //             poly = Polygon2D(pts);
        //             if (!poly.isValid()) poly = poly.buffer(0);
        //             if (!poly.isEmpty()) {
        //                 polygons.append(poly);
        //             }
        //         }
        //     }
        //     
        //     // 合并多个多边形
        //     result = unary_union(polygons);
        // } catch (...) {}
        
        return result;
    }
    
    /**
     * 添加填充和标注
     * 
     * 对应Python: engine_cad_v3.py 第267-309行
     * 
     * @param poly 多边形
     * @param rgbColor RGB颜色
     * @param pattern 填充图案名称
     * @param scale 填充比例
     * @param textHeight 文字高度
     * @param strataName 地层名称
     * @param isDesign 是否为设计区
     * @param msp 模型空间指针
     * @param doc 文档指针
     * @return 总面积
     */
    static double addWithLabel(Polygon2D &poly,
                               const QColor &rgbColor,
                               const QString &pattern,
                               double scale,
                               double textHeight,
                               const QString &strataName,
                               bool isDesign,
                               void *msp,
                               void *doc) {
        
        if (poly.isEmpty() || !poly.isValid()) {
            return 0.0;
        }
        
        double totalArea = 0.0;
        
        // 确定图层名称
        QString labelType = isDesign ? QStringLiteral("设计") : QStringLiteral("超挖");
        QString layerHatch = strataName + labelType;
        QString layerLabel = strataName + labelType + QStringLiteral("_标注");
        QString fullLabel = strataName + labelType;
        
        // 处理单多边形或多多边形
        QVector<Polygon2D> geoms;
        // if (poly is Polygon) geoms = {poly};
        // else if (poly is MultiPolygon) geoms = poly.geoms;
        geoms.append(poly);
        
        for (Polygon2D &p : geoms) {
            double areaVal = p.area();
            if (areaVal < 0.01) continue;
            
            totalArea += areaVal;
            
            // 创建HATCH实体
            // hatch = msp.add_hatch(dxfattribs={'layer': layerHatch});
            // hatch.rgb = rgbColor;
            // hatch.set_pattern_fill(pattern, scale=scale);
            // hatch.paths.add_polyline_path(p.exterior.coords, is_closed=True);
            // for interior in p.interiors:
            //     hatch.paths.add_polyline_path(interior.coords, is_closed=True);
            
            // 添加标注（面积大于0.1时）
            if (areaVal > 0.1) {
                Point2D inPoint = p.representativePoint();
                
                // 创建MTEXT标注
                // QString labelContent = QString("{\\fArial|b1;%1\\P%2}")
                //     .arg(fullLabel).arg(areaVal, 0, 'f', 3);
                // mtext = msp.add_mtext(labelContent, dxfattribs={
                //     'layer': layerLabel,
                //     'insert': (inPoint.x, inPoint.y),
                //     'char_height': textHeight,
                //     'attachment_point': 5,  // 中心
                // });
                // mtext.rgb = rgbColor;
                // mtext.dxf.bg_fill_setting = 1;  // 背景
                // mtext.dxf.bg_fill_scale_factor = 1.3;
            }
        }
        
        return totalArea;
    }
    
    /**
     * 添加简单填充（无标注）
     * 
     * 对应Python: engine_cad_v3.py 第311-333行
     * 
     * @param poly 多边形
     * @param layerName 图层名称
     * @param colorIndex DXF颜色索引
     * @param rgbColor RGB颜色（可选）
     * @param msp 模型空间指针
     */
    static void addSimple(Polygon2D &poly,
                          const QString &layerName,
                          int colorIndex = 7,
                          const QColor &rgbColor = QColor(),
                          void *msp = nullptr) {
        
        if (poly.isEmpty()) return;
        
        // 获取边界
        QVector<QVector<Point2D>> boundaries;
        
        // 外环
        if (!poly.exterior.isEmpty()) {
            boundaries.append(poly.exterior);
        }
        
        // 内环（孔洞）
        for (const QVector<Point2D> &interior : poly.interiors) {
            boundaries.append(interior);
        }
        
        // 创建填充
        for (const QVector<Point2D> &boundary : boundaries) {
            if (boundary.size() >= 3) {
                // hatch = msp.add_hatch(dxfattribs={'layer': layerName, 'color': colorIndex});
                // hatch.set_pattern_fill('SOLID', scale=1.0);
                // if (rgbColor.isValid()) hatch.rgb = rgbColor;
                // 
                // QVector<Point2D> pts;
                // for (pt in boundary) pts.append(pt);
                // hatch.paths.add_polyline_path(pts, is_closed=True);
            }
        }
    }
    
    /**
     * 合并多个多边形
     */
    static Polygon2D unionPolygons(const QVector<Polygon2D> &polygons) {
        if (polygons.isEmpty()) return Polygon2D();
        if (polygons.size() == 1) return polygons[0];
        
        // 简化实现：返回第一个多边形
        // 完整实现需要复杂的几何算法（unary_union）
        
        Polygon2D result;
        for (const Polygon2D &poly : polygons) {
            // 合并外环（简化）
            for (const Point2D &pt : poly.exterior) {
                result.exterior.append(pt);
            }
        }
        
        return result;
    }
    
    /**
     * 计算多边形交集
     */
    static Polygon2D intersection(const Polygon2D &poly1, const Polygon2D &poly2) {
        // 简化实现：需要完整几何算法
        return Polygon2D();
    }
    
    /**
     * 计算多边形差集
     */
    static Polygon2D difference(const Polygon2D &poly1, const Polygon2D &poly2) {
        // 简化实现：需要完整几何算法
        return Polygon2D();
    }
};

#endif // HATCH_PROCESSOR_H