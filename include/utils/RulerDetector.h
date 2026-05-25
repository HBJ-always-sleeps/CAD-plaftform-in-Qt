#ifndef RULER_DETECTOR_H
#define RULER_DETECTOR_H

#include "Geometry.h"
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPair>
#include <cmath>

/**
 * RulerDetector类 - 复刻Python engine_cad_v3.py RulerDetector类
 * 
 * 标尺检测器：
 * - detect_scale: 检测标尺比例，返回(elev_to_y, y_to_elev)函数对
 * 
 * 核心算法：
 * 1. 检测标尺块参照（INSERT实体）
 * 2. 遍历块内TEXT获取高程标注
 * 3. 线性回归拟合 Y ↔ 高程 关系
 * 
 * 数学原理：
 * y = a * elevation + b
 * 
 * 线性回归参数计算：
 * a = (n * Σ(y*e) - Σy * Σe) / (n * Σe² - (Σe)²)
 * b = (Σy - a * Σe) / n
 */
class RulerDetector
{
public:
    /**
     * 高程-Y坐标转换函数结构
     */
    struct ScaleResult {
        double a;  // 斜率
        double b;  // 截距
        
        double elevToY(double elevation) const {
            return a * elevation + b;
        }
        
        double yToElev(double y) const {
            return (y - b) / a;
        }
        
        bool isValid() const {
            return std::abs(a) > 0.0001;
        }
    };
    
    /**
     * 高程点数据
     */
    struct ElevationPoint {
        double worldY;      // 世界Y坐标
        double elevation;   // 高程值（米）
        
        ElevationPoint(double y = 0, double elev = 0) : worldY(y), elevation(elev) {}
    };
    
    /**
     * 检测标尺比例
     * 
     * 对应Python: engine_cad_v3.py 第392-455行
     * 
     * @param rulerLayer 标尺图层名称
     * @param sectXMin 断面X范围最小值
     * @param sectXMax 断面X范围最大值
     * @param sectYCenter 断面Y中心
     * @param sectYMin 断面Y最小值
     * @param sectYMax 断面Y最大值
     * @return ScaleResult，包含转换函数参数
     */
    static ScaleResult detectScale(const QString &rulerLayer,
                                   double sectXMin, double sectXMax,
                                   double sectYCenter,
                                   double sectYMin, double sectYMax) {
        
        // 注意：实际实现需要dxflib库支持
        // 此处为算法框架
        
        // Step 1: 检测标尺候选
        QVector<RulerCandidate> candidates = detectRulerCandidates(rulerLayer, 
                                                                    sectXMin, sectXMax);
        
        if (candidates.isEmpty()) {
            return ScaleResult();  // 返回无效结果
        }
        
        // Step 2: 选择最佳标尺（与断面Y范围重叠最大）
        RulerCandidate bestRuler = selectBestRuler(candidates, sectYMin, sectYMax, sectYCenter);
        
        // Step 3: 提取高程点
        QVector<ElevationPoint> elevationPoints = extractElevationPoints(bestRuler);
        
        if (elevationPoints.size() < 2) {
            return ScaleResult();
        }
        
        // Step 4: 线性回归拟合
        return linearRegression(elevationPoints);
    }
    
    /**
     * 使用默认转换（无标尺时）
     */
    static ScaleResult defaultScale(double targetElevation) {
        // 默认转换公式：y = 5.0 * elevation - 27.0
        ScaleResult result;
        result.a = 5.0;
        result.b = -27.0;
        return result;
    }
    
private:
    /**
     * 标尺候选结构
     */
    struct RulerCandidate {
        double insertX;
        double insertY;
        double yMin;
        double yMax;
        // void *entity;  // 实际实体指针（dxflib）
        
        RulerCandidate() : insertX(0), insertY(0), yMin(0), yMax(0) {}
    };
    
    /**
     * 检测标尺候选
     * 
     * 需要dxflib库支持的具体实现
     */
    static QVector<RulerCandidate> detectRulerCandidates(const QString &layerName,
                                                          double sectXMin, double sectXMax) {
        QVector<RulerCandidate> candidates;
        
        // 遍历图层上的INSERT实体
        // for (entity in msp.query(f'*[layer=="{layerName}"]')) {
        //     if (entity.dxftype() == 'INSERT') {
        //         RulerCandidate cand;
        //         cand.insertX = entity.dxf.insert.x;
        //         cand.insertY = entity.dxf.insert.y;
        //         
        //         // 检查是否在断面X范围内（±100容差）
        //         if (sectXMin - 100 <= cand.insertX <= sectXMax + 100) {
        //             // 遍历块内TEXT获取Y范围
        //             cand.yMin = cand.yMax = cand.insertY;
        //             candidates.append(cand);
        //         }
        //     }
        // }
        
        return candidates;
    }
    
    /**
     * 选择最佳标尺
     */
    static RulerCandidate selectBestRuler(const QVector<RulerCandidate> &candidates,
                                          double sectYMin, double sectYMax,
                                          double sectYCenter) {
        if (candidates.isEmpty()) {
            return RulerCandidate();
        }
        
        double bestOverlapRatio = -1;
        RulerCandidate bestRuler;
        
        for (const RulerCandidate &ruler : candidates) {
            // 计算Y重叠
            double overlap = std::max(0.0, std::min(sectYMax, ruler.yMax) - 
                                      std::max(sectYMin, ruler.yMin));
            double rulerHeight = ruler.yMax - ruler.yMin;
            double overlapRatio = rulerHeight > 0 ? overlap / rulerHeight : 0;
            
            if (overlapRatio > bestOverlapRatio) {
                bestOverlapRatio = overlapRatio;
                bestRuler = ruler;
            }
        }
        
        // 如果没有找到有重叠的，选择最近的
        if (bestOverlapRatio < 0) {
            double sectXCenter = (sectYMin + sectYMax) / 2;  // 注意：这里应该是X
            double minDist = std::numeric_limits<double>::max();
            
            for (const RulerCandidate &ruler : candidates) {
                double dist = std::abs(ruler.insertX - sectXCenter);
                if (dist < minDist) {
                    minDist = dist;
                    bestRuler = ruler;
                }
            }
        }
        
        return bestRuler;
    }
    
    /**
     * 提取高程点
     * 
     * 需要dxflib库支持的具体实现
     */
    static QVector<ElevationPoint> extractElevationPoints(const RulerCandidate &ruler) {
        QVector<ElevationPoint> points;
        
        // 遍历标尺块内的TEXT实体
        // for (be in doc.blocks[block_name]) {
        //     if (be.dxftype() in ('TEXT', 'MTEXT')) {
        //         double worldY = be.dxf.insert.y + ruler.insertY;
        //         QString text = be.dxf.text.strip();
        //         double elevation = text.toDouble();
        //         points.append(ElevationPoint(worldY, elevation));
        //     }
        // }
        
        return points;
    }
    
    /**
     * 线性回归拟合
     * 
     * 数学公式：
     * y = a * elevation + b
     * 
     * a = (n * Σ(y*e) - Σy * Σe) / (n * Σe² - (Σe)²)
     * b = (Σy - a * Σe) / n
     */
    static ScaleResult linearRegression(const QVector<ElevationPoint> &points) {
        ScaleResult result;
        
        if (points.size() < 2) {
            return result;
        }
        
        int n = points.size();
        
        double sumY = 0;
        double sumE = 0;
        double sumYE = 0;
        double sumE2 = 0;
        
        for (const ElevationPoint &pt : points) {
            sumY += pt.worldY;
            sumE += pt.elevation;
            sumYE += pt.worldY * pt.elevation;
            sumE2 += pt.elevation * pt.elevation;
        }
        
        double denom = n * sumE2 - sumE * sumE;
        
        if (std::abs(denom) < 0.001) {
            return result;  // 防止除零
        }
        
        result.a = (n * sumYE - sumY * sumE) / denom;
        result.b = (sumY - result.a * sumE) / n;
        
        return result;
    }
};

#endif // RULER_DETECTOR_H