#ifndef VIRTUAL_BOX_BUILDER_H
#define VIRTUAL_BOX_BUILDER_H

#include "Geometry.h"
#include "LineUtils.h"
#include <QVector>
#include <cmath>
#include <algorithm>

/**
 * VirtualBoxBuilder类 - 复刻Python engine_cad_v3.py VirtualBoxBuilder类
 * 
 * 虚拟断面框构建器：
 * - build_from_overexcav: 从超挖线构建虚拟断面框
 * 
 * 用途：
 * 当断面图中没有明确的断面框时，从超挖线的位置推断虚拟断面框范围
 */
class VirtualBoxBuilder
{
public:
    /**
     * 线信息结构
     */
    struct LineInfo {
        Line2D line;
        double midX;
        double midY;
        double minX, maxX, minY, maxY;  // bounds
        
        LineInfo() : midX(0), midY(0), minX(0), maxX(0), minY(0), maxY(0) {}
        
        LineInfo(const Line2D &l) : line(l) {
            midX = l.midX();
            midY = l.midY();
            minX = l.minX();
            maxX = l.maxX();
            minY = l.minY();
            maxY = l.maxY();
        }
    };
    
    /**
     * 从超挖线构建虚拟断面框
     * 
     * 对应Python: engine_cad_v3.py 第461-507行
     * 
     * 算法步骤：
     * 1. 对超挖线按Y坐标聚类
     * 2. 每个聚类形成一个虚拟断面框
     * 3. 返回Box2D列表
     * 
     * @param overexcLines 超挖线列表
     * @return 虚拟断面框列表
     */
    static QVector<Box2D> buildFromOverexcav(const QVector<Line2D> &overexcLines) {
        if (overexcLines.isEmpty()) {
            return QVector<Box2D>();
        }
        
        // Step 1: 构建线信息列表
        QVector<LineInfo> lineInfo;
        for (const Line2D &line : overexcLines) {
            lineInfo.append(LineInfo(line));
        }
        
        int n = lineInfo.size();
        if (n == 0) return QVector<Box2D>();
        
        // 单条线：直接返回其包围盒
        if (n == 1) {
            const LineInfo &info = lineInfo[0];
            return QVector<Box2D>() << Box2D(info.minX, info.minY, info.maxX, info.maxY);
        }
        
        // Step 2: 按Y坐标聚类
        // 计算高度统计，用于确定聚类阈值
        QVector<double> heights;
        for (const LineInfo &info : lineInfo) {
            heights.append(info.maxY - info.minY);
        }
        std::sort(heights.begin(), heights.end());
        double medianHeight = heights[heights.size() / 2];
        double clusterThreshold = medianHeight * 1.5;
        
        // 按Y从大到小排序（Y越大越靠上）
        QVector<LineInfo> sortedByY = lineInfo;
        std::sort(sortedByY.begin(), sortedByY.end(), 
                  [](const LineInfo &a, const LineInfo &b) {
                      return a.midY > b.midY;  // 从大到小
                  });
        
        // Step 3: 聚类
        QVector<QVector<LineInfo>> clusters;
        clusters.append(QVector<LineInfo>() << sortedByY[0]);
        
        for (int i = 1; i < sortedByY.size(); ++i) {
            double yGap = std::abs(sortedByY[i].midY - sortedByY[i - 1].midY);
            
            if (yGap < clusterThreshold) {
                // 属于同一聚类
                clusters.last().append(sortedByY[i]);
            } else {
                // 新聚类
                clusters.append(QVector<LineInfo>() << sortedByY[i]);
            }
        }
        
        // Step 4: 构建虚拟断面框
        QVector<Box2D> virtualBoxes;
        
        for (const QVector<LineInfo> &cluster : clusters) {
            if (cluster.isEmpty()) continue;
            
            // 计算聚类中所有线的联合范围
            double minX = std::numeric_limits<double>::max();
            double maxX = std::numeric_limits<double>::min();
            double minY = std::numeric_limits<double>::max();
            double maxY = std::numeric_limits<double>::min();
            
            for (const LineInfo &info : cluster) {
                // 收集所有坐标点
                for (const Point2D &pt : info.line.points) {
                    minX = std::min(minX, pt.x);
                    maxX = std::max(maxX, pt.x);
                    minY = std::min(minY, pt.y);
                    maxY = std::max(maxY, pt.y);
                }
            }
            
            if (maxX > minX && maxY > minY) {
                virtualBoxes.append(Box2D(minX, minY, maxX, maxY));
            }
        }
        
        return virtualBoxes;
    }
    
    /**
     * 根据断面中心位置匹配虚拟框
     * 
     * @param sectCenter 断面中心点
     * @param virtualBoxes 虚拟断面框列表
     * @param tolerance 容差
     * @return 最佳匹配的虚拟框，如果没有匹配返回空Box2D
     */
    static Box2D matchVirtualBox(const Point2D &sectCenter,
                                  const QVector<Box2D> &virtualBoxes,
                                  double tolerance = 100.0) {
        Box2D bestBox;
        double bestDist = std::numeric_limits<double>::max();
        
        for (const Box2D &box : virtualBoxes) {
            Point2D boxCenter = box.center();
            double dist = sectCenter.distance(boxCenter);
            
            // 检查断面中心是否在虚拟框附近
            if (dist < bestDist && dist < tolerance) {
                bestDist = dist;
                bestBox = box;
            }
            
            // 或者检查断面中心是否在虚拟框内
            if (box.contains(sectCenter)) {
                return box;  // 优先返回包含中心的框
            }
        }
        
        return bestBox;
    }
    
    /**
     * 判断线是否与Box相交
     */
    static bool lineIntersectsBox(const Line2D &line, const Box2D &box) {
        // 检查线段的代表点是否在Box内
        Point2D midPt(line.midX(), line.midY());
        if (box.contains(midPt)) return true;
        
        // 检查起点和终点
        if (box.contains(line.startPoint())) return true;
        if (box.contains(line.endPoint())) return true;
        
        // 检查线段包围盒是否与Box相交
        Box2D lineBox(line.minX(), line.minY(), line.maxX(), line.maxY());
        return box.intersects(lineBox);
    }
    
    /**
     * 过滤在指定Box范围内的线
     */
    static QVector<Line2D> filterLinesInBox(const QVector<Line2D> &lines,
                                             const Box2D &box,
                                             double margin = 20.0) {
        QVector<Line2D> filtered;
        
        Box2D expandedBox = box.expand(margin, margin);
        
        for (const Line2D &line : lines) {
            if (lineIntersectsBox(line, expandedBox)) {
                filtered.append(line);
            }
        }
        
        return filtered;
    }
};

#endif // VIRTUAL_BOX_BUILDER_H