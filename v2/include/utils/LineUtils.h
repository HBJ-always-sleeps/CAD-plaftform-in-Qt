#ifndef LINE_UTILS_H
#define LINE_UTILS_H

#include "Geometry.h"
#include <QVector>
#include <cmath>
#include <algorithm>

/**
 * LineUtils类 - 复刻Python engine_cad_v3.py LineUtils类
 * 
 * 线段处理工具集：
 * - get_y_at_x: 获取指定X处的Y值（线性插值）
 * - extend: 延长线两端
 * - find_intersections: 找出两条线的所有交点
 */
class LineUtils
{
public:
    /**
     * 获取指定 X 处的 Y 值 - 线性插值
     * 
     * 对应Python: engine_cad_v3.py 第88-99行
     * 
     * 数学公式：
     * y(x) = y1 + (x - x1) / (x2 - x1) * (y2 - y1)
     * 
     * @param line 输入线段
     * @param x 目标X坐标
     * @param found 输出参数，是否找到
     * @return Y值，如果未找到返回0
     */
    static double getYAtX(const Line2D &line, double x, bool *found = nullptr) {
        const QVector<Point2D> &coords = line.points;
        
        if (found) *found = false;
        
        for (int i = 0; i < coords.size() - 1; ++i) {
            double x1 = coords[i].x;
            double y1 = coords[i].y;
            double x2 = coords[i + 1].x;
            double y2 = coords[i + 1].y;
            
            // 检查x是否在当前线段范围内
            bool inRange = (x1 <= x && x <= x2) || (x2 <= x && x <= x1);
            
            if (inRange) {
                // 几乎垂直的线段（x2-x1接近0）
                if (std::abs(x2 - x1) < 0.001) {
                    if (found) *found = true;
                    return y1;
                }
                
                // 线性插值参数 t
                double t = (x - x1) / (x2 - x1);
                
                // 线性插值公式
                double y = y1 + t * (y2 - y1);
                
                if (found) *found = true;
                return y;
            }
        }
        
        return 0.0;  // 未找到
    }
    
    /**
     * 找出两条线的所有交点
     * 
     * 对应Python: engine_cad_v3.py 第116-128行
     * 
     * 使用参数方程法计算交点
     * 
     * @param line1 第一条线
     * @param line2 第二条线
     * @return 交点列表
     */
    static QVector<Point2D> findIntersections(const Line2D &line1, const Line2D &line2) {
        QVector<Point2D> intersections;
        
        const QVector<Point2D> &coords1 = line1.points;
        const QVector<Point2D> &coords2 = line2.points;
        
        // 对每对线段计算交点
        for (int i = 0; i < coords1.size() - 1; ++i) {
            double x1 = coords1[i].x, y1 = coords1[i].y;
            double x2 = coords1[i + 1].x, y2 = coords1[i + 1].y;
            
            for (int j = 0; j < coords2.size() - 1; ++j) {
                double x3 = coords2[j].x, y3 = coords2[j].y;
                double x4 = coords2[j + 1].x, y4 = coords2[j + 1].y;
                
                // 参数方程法求交点
                Point2D pt;
                if (segmentIntersection(x1, y1, x2, y2, x3, y3, x4, y4, pt)) {
                    intersections.append(pt);
                }
            }
        }
        
        return intersections;
    }
    
    /**
     * 两线段相交检测（参数方程法）
     * 
     * 数学原理：
     * 线段1: P = P1 + t * (P2 - P1), t ∈ [0, 1]
     * 线段2: P = P3 + u * (P4 - P3), u ∈ [0, 1]
     * 
     * 交点条件：t ∈ [0, 1] 且 u ∈ [0, 1]
     */
    static bool segmentIntersection(double x1, double y1, double x2, double y2,
                                    double x3, double y3, double x4, double y4,
                                    Point2D &intersection) {
        double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        
        // 平行或重合
        if (std::abs(denom) < 0.0001) {
            return false;
        }
        
        double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
        double u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;
        
        // 检查参数是否在有效范围内
        if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
            intersection.x = x1 + t * (x2 - x1);
            intersection.y = y1 + t * (y2 - y1);
            return true;
        }
        
        return false;
    }
    
    /**
     * 计算两条线的距离
     */
    static double distance(const Line2D &line1, const Line2D &line2) {
        // 简化实现：计算代表点之间的距离
        return line1.representativePoint().distance(line2.representativePoint());
    }
    
    /**
     * 判断两条线是否相交
     */
    static bool intersects(const Line2D &line1, const Line2D &line2) {
        return !findIntersections(line1, line2).isEmpty();
    }
    
};

#endif // LINE_UTILS_H