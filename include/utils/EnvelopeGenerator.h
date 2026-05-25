#ifndef ENVELOPE_GENERATOR_H
#define ENVELOPE_GENERATOR_H

#include "Geometry.h"
#include "LineUtils.h"
#include <QSet>
#include <QVector>
#include <cmath>
#include <algorithm>

/**
 * EnvelopeGenerator类 - 复刻Python engine_cad_v3.py EnvelopeGenerator类
 * 
 * 包络线生成器：
 * - generate: 生成上/下包络线
 * 
 * 核心算法：
 * 1. 收集所有断面线的X坐标点（包括交点）
 * 2. 对每个X位置，计算所有断面线的Y值
 * 3. 选择目标Y值（最小或最大）
 * 4. 构建包络线坐标序列
 * 
 * 数学定义：
 * - 下包络线：E_lower(x) = min_i {y_i(x)}
 * - 上包络线：E_upper(x) = max_i {y_i(x)}
 */
class EnvelopeGenerator
{
public:
    /**
     * 包络线类型
     */
    enum class EnvelopeType {
        Lower,  // 下包络线（取最小Y）
        Upper   // 上包络线（取最大Y）
    };
    
    /**
     * 生成包络线
     * 
     * 对应Python: engine_cad_v3.py 第340-385行
     * 
     * @param baseLine 基准断面线
     * @param sectionLines 其他断面线列表
     * @param envelopeType 包络线类型（lower/upper）
     * @return 包络线，如果失败返回空线
     */
    static Line2D generate(const Line2D &baseLine, 
                           const QVector<Line2D> &sectionLines,
                           EnvelopeType envelopeType = EnvelopeType::Lower) {
        
        // Step 1: 收集所有X坐标
        QSet<double> allXCoords;
        
        // 基准线的X坐标
        for (const Point2D &pt : baseLine.points) {
            allXCoords.insert(std::round(pt.x * 1000) / 1000);  // 精度0.001
        }
        
        // 其他断面线的X坐标
        for (const Line2D &sec : sectionLines) {
            for (const Point2D &pt : sec.points) {
                allXCoords.insert(std::round(pt.x * 1000) / 1000);
            }
        }
        
        // Step 2: 收集交点附近的X坐标（加密采样）
        QVector<Line2D> allLines;
        allLines.append(baseLine);
        for (const Line2D &sec : sectionLines) {
            allLines.append(sec);
        }
        
        for (int i = 0; i < allLines.size(); ++i) {
            for (int j = i + 1; j < allLines.size(); ++j) {
                QVector<Point2D> intersections = LineUtils::findIntersections(allLines[i], allLines[j]);
                for (const Point2D &pt : intersections) {
                    double ix = pt.x;
                    allXCoords.insert(std::round(ix * 1000) / 1000);
                    
                    // 在交点附近加密采样（±1.0, ±0.5）
                    for (double delta : {-1.0, -0.5, 0.5, 1.0}) {
                        allXCoords.insert(std::round((ix + delta) * 1000) / 1000);
                    }
                }
            }
        }
        
        if (allXCoords.isEmpty()) {
            return Line2D();
        }
        
        // Step 3: 过滤X范围（基准线的范围）
        double xMin = baseLine.minX();
        double xMax = baseLine.maxX();
        
        QVector<double> filteredX;
        for (double x : allXCoords) {
            if (x >= xMin && x <= xMax) {
                filteredX.append(x);
            }
        }
        
        // 排序并去重
        std::sort(filteredX.begin(), filteredX.end());
        filteredX.erase(std::unique(filteredX.begin(), filteredX.end()), filteredX.end());
        
        if (filteredX.isEmpty()) {
            return Line2D();
        }
        
        // Step 4: 对每个X位置计算Y值并选择目标Y
        QVector<Point2D> envelopeCoords;
        
        for (double x : filteredX) {
            QVector<double> allYs;
            
            // 基准线的Y值
            bool found = false;
            double baseY = LineUtils::getYAtX(baseLine, x, &found);
            if (found) {
                allYs.append(baseY);
            }
            
            // 其他断面线的Y值
            for (const Line2D &sec : sectionLines) {
                double secY = LineUtils::getYAtX(sec, x, &found);
                if (found) {
                    allYs.append(secY);
                }
            }
            
            if (!allYs.isEmpty()) {
                // 选择目标Y值
                double targetY;
                if (envelopeType == EnvelopeType::Lower) {
                    // 下包络线：取最小Y
                    targetY = *std::min_element(allYs.begin(), allYs.end());
                } else {
                    // 上包络线：取最大Y
                    targetY = *std::max_element(allYs.begin(), allYs.end());
                }
                
                envelopeCoords.append(Point2D(x, targetY));
            }
        }
        
        // Step 5: 构建包络线
        if (envelopeCoords.size() >= 2) {
            return Line2D(envelopeCoords);
        }
        
        return Line2D();
    }
    
    /**
     * 从字符串解析包络线类型
     */
    static EnvelopeType parseEnvelopeType(const QString &typeStr) {
        QString lower = typeStr.toLower();
        if (lower == "lower" || lower == "下包络" || lower == "下") {
            return EnvelopeType::Lower;
        }
        return EnvelopeType::Upper;
    }
    
    /**
     * 包络线类型转字符串
     */
    static QString envelopeTypeToString(EnvelopeType type) {
        return type == EnvelopeType::Lower ? QStringLiteral("下包络") : QStringLiteral("上包络");
    }
};

#endif // ENVELOPE_GENERATOR_H