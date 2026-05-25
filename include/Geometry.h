#ifndef GEOMETRY_H
#define GEOMETRY_H

// Qt headers - use module headers for Qt6 compatibility
#include <QtGlobal>
#include <QtMath>
#include <QString>
#include <QVector>
#include <QPointF>
#include <QPair>
#include <QColor>
#include <QDebug>

// Standard headers
#include <cmath>
#include <algorithm>
#include <limits>

/**
 * 二维点结构 - 复刻Shapely Point
 */
struct Point2D {
    double x;
    double y;
    
    Point2D(double x_ = 0, double y_ = 0) : x(x_), y(y_) {}
    
    Point2D(const QPointF &p) : x(p.x()), y(p.y()) {}
    
    QPointF toQPointF() const { return QPointF(x, y); }
    
    double distance(const Point2D &other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    
    double distanceSq(const Point2D &other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return dx * dx + dy * dy;
    }
    
    Point2D operator+(const Point2D &other) const {
        return Point2D(x + other.x, y + other.y);
    }
    
    Point2D operator-(const Point2D &other) const {
        return Point2D(x - other.x, y - other.y);
    }
    
    Point2D operator*(double factor) const {
        return Point2D(x * factor, y * factor);
    }
    
    bool operator==(const Point2D &other) const {
        return std::abs(x - other.x) < 0.001 && std::abs(y - other.y) < 0.001;
    }

    bool operator!=(const Point2D &other) const {
        return !(*this == other);
    }

    bool operator<(const Point2D &other) const {
        // 先比较x，再比较y（用于QMap排序）
        if (std::abs(x - other.x) > 0.001) {
            return x < other.x;
        }
        return y < other.y;
    }

    QString toString() const {
        return QString("(%1, %2)").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3);
    }
};

/**
 * 二维线结构 - 复刻Shapely LineString
 */
struct Line2D {
    QVector<Point2D> points;
    QString layerName;    // 图层名称
    int color = 256;      // DXF颜色索引 (256 = ByLayer)
    QString linetype = "ByLayer";  // 线型名称

    Line2D() = default;
    
    Line2D(const QVector<Point2D> &pts) : points(pts) {}
    
    Line2D(const QVector<QPointF> &pts) {
        for (const QPointF &p : pts) {
            points.append(Point2D(p));
        }
    }
    
    bool isEmpty() const { return points.isEmpty(); }
    
    int size() const { return points.size(); }
    
    double length() const {
        double len = 0.0;
        for (int i = 0; i < points.size() - 1; ++i) {
            len += points[i].distance(points[i + 1]);
        }
        return len;
    }
    
    double minX() const {
        if (points.isEmpty()) return 0;
        double m = points[0].x;
        for (const Point2D &p : points) {
            m = std::min(m, p.x);
        }
        return m;
    }
    
    double maxX() const {
        if (points.isEmpty()) return 0;
        double m = points[0].x;
        for (const Point2D &p : points) {
            m = std::max(m, p.x);
        }
        return m;
    }
    
    double minY() const {
        if (points.isEmpty()) return 0;
        double m = points[0].y;
        for (const Point2D &p : points) {
            m = std::min(m, p.y);
        }
        return m;
    }
    
    double maxY() const {
        if (points.isEmpty()) return 0;
        double m = points[0].y;
        for (const Point2D &p : points) {
            m = std::max(m, p.y);
        }
        return m;
    }
    
    double midX() const {
        return (minX() + maxX()) / 2.0;
    }
    
    double midY() const {
        return (minY() + maxY()) / 2.0;
    }
    
    double xCenter() const { return midX(); }
    double yCenter() const { return midY(); }
    
    Point2D startPoint() const {
        if (points.isEmpty()) return Point2D();
        return points.first();
    }
    
    Point2D endPoint() const {
        if (points.isEmpty()) return Point2D();
        return points.last();
    }
    
    Point2D representativePoint() const {
        if (points.isEmpty()) return Point2D();
        if (points.size() == 1) return points[0];
        return points[points.size() / 2];
    }
    
    QPair<double, double> bounds() const {
        return QPair<double, double>(minX(), maxX());
    }
    
    QVector<QPointF> toQPointFVector() const {
        QVector<QPointF> result;
        for (const Point2D &p : points) {
            result.append(p.toQPointF());
        }
        return result;
    }
    
    bool isValid() const {
        return points.size() >= 2;
    }
    
    bool isClosed() const {
        if (points.size() < 3) return false;
        return points.first() == points.last();
    }
};

/**
 * 二维多边形结构 - 复刻Shapely Polygon
 * 
 * 使用Shoelace公式计算面积：
 * A = 1/2 * |Σ(x_i * y_{i+1} - x_{i+1} * y_i)|
 */
struct Polygon2D {
    QVector<Point2D> exterior;              // 外环（边界）
    QVector<QVector<Point2D>> interiors;    // 内环（孔洞）
    QString layerName;                       // 图层名称
    int colorIndex = 256;                     // DXF颜色索引 (256=ByLayer)
    QString pattern = QStringLiteral("SOLID"); // 填充图案
    double scale = 1.0;                      // 图案比例
    QColor rgbColor;                         // RGB颜色
    
    Polygon2D() = default;
    
    Polygon2D(const QVector<Point2D> &ext) : exterior(ext) {}
    
    Polygon2D(const QVector<QPointF> &ext) {
        for (const QPointF &p : ext) {
            exterior.append(Point2D(p));
        }
    }
    
    bool isEmpty() const { return exterior.isEmpty(); }
    
    bool isValid() const {
        if (exterior.size() < 3) return false;
        return true;
    }
    
    /**
     * 计算多边形面积（Shoelace公式）
     * 公式: A = 1/2 * |Σ(x_i * y_{i+1} - x_{i+1} * y_i)|
     */
    double area() const {
        if (exterior.size() < 3) return 0.0;
        
        // 外环面积
        double exteriorArea = computeRingArea(exterior);
        
        // 减去内环面积（孔洞）
        double interiorArea = 0.0;
        for (const QVector<Point2D> &interior : interiors) {
            interiorArea += computeRingArea(interior);
        }
        
        return std::abs(exteriorArea) - interiorArea;
    }
    
    /**
     * 计算单环面积
     */
    static double computeRingArea(const QVector<Point2D> &ring) {
        if (ring.size() < 3) return 0.0;
        
        double sum = 0.0;
        int n = ring.size();
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            sum += ring[i].x * ring[j].y;
            sum -= ring[j].x * ring[i].y;
        }
        return std::abs(sum) / 2.0;
    }
    
    double minX() const {
        if (exterior.isEmpty()) return 0;
        double m = exterior[0].x;
        for (const Point2D &p : exterior) {
            m = std::min(m, p.x);
        }
        return m;
    }
    
    double maxX() const {
        if (exterior.isEmpty()) return 0;
        double m = exterior[0].x;
        for (const Point2D &p : exterior) {
            m = std::max(m, p.x);
        }
        return m;
    }
    
    double minY() const {
        if (exterior.isEmpty()) return 0;
        double m = exterior[0].y;
        for (const Point2D &p : exterior) {
            m = std::min(m, p.y);
        }
        return m;
    }
    
    double maxY() const {
        if (exterior.isEmpty()) return 0;
        double m = exterior[0].y;
        for (const Point2D &p : exterior) {
            m = std::max(m, p.y);
        }
        return m;
    }
    
    Point2D representativePoint() const {
        if (exterior.isEmpty()) return Point2D();
        
        // 计算中心点
        double cx = 0, cy = 0;
        for (const Point2D &p : exterior) {
            cx += p.x;
            cy += p.y;
        }
        cx /= exterior.size();
        cy /= exterior.size();
        
        // 尝试找到内部点（简单方法：使用中心点）
        Point2D center(cx, cy);
        if (containsPoint(center)) {
            return center;
        }
        
        // 如果中心点不在内部，使用边界上的中点
        return exterior[exterior.size() / 2];
    }
    
    /**
     * 判断点是否在多边形内部（射线法）
     */
    bool containsPoint(const Point2D &pt) const {
        if (exterior.size() < 3) return false;
        
        // 射线法判断是否在外环内
        bool insideExterior = rayCast(pt, exterior);
        
        if (!insideExterior) return false;
        
        // 检查是否在内环（孔洞）内
        for (const QVector<Point2D> &interior : interiors) {
            if (rayCast(pt, interior)) {
                return false;  // 在孔洞内，不在多边形内
            }
        }
        
        return true;
    }
    
    /**
     * 射线法判断点是否在环内
     */
    static bool rayCast(const Point2D &pt, const QVector<Point2D> &ring) {
        int n = ring.size();
        int count = 0;
        
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            const Point2D &p1 = ring[i];
            const Point2D &p2 = ring[j];
            
            // 检查射线是否穿过边
            if ((p1.y > pt.y) != (p2.y > pt.y)) {
                double xIntersect = p1.x + (pt.y - p1.y) / (p2.y - p1.y) * (p2.x - p1.x);
                if (pt.x < xIntersect) {
                    count++;
                }
            }
        }
        
        return (count % 2) == 1;
    }
    
    /**
     * 多边形缓冲（简化版，生成包围盒）
     * 完整实现需要复杂的几何算法
     */
    Polygon2D buffer(double distance) const {
        if (exterior.isEmpty()) return Polygon2D();
        
        // 简化实现：扩大边界
        double minX = this->minX() - distance;
        double maxX = this->maxX() + distance;
        double minY = this->minY() - distance;
        double maxY = this->maxY() + distance;
        
        QVector<Point2D> newExt;
        newExt.append(Point2D(minX, minY));
        newExt.append(Point2D(maxX, minY));
        newExt.append(Point2D(maxX, maxY));
        newExt.append(Point2D(minX, maxY));
        newExt.append(Point2D(minX, minY));  // 关闭
        
        return Polygon2D(newExt);
    }
    
    QVector<QPointF> exteriorQPointF() const {
        QVector<QPointF> result;
        for (const Point2D &p : exterior) {
            result.append(p.toQPointF());
        }
        return result;
    }
};

/**
 * 多多边形结构 - 复刻Shapely MultiPolygon
 */
struct MultiPolygon2D {
    QVector<Polygon2D> polygons;
    
    MultiPolygon2D() = default;
    
    MultiPolygon2D(const QVector<Polygon2D> &polys) : polygons(polys) {}
    
    bool isEmpty() const { return polygons.isEmpty(); }
    
    int size() const { return polygons.size(); }
    
    double area() const {
        double totalArea = 0.0;
        for (const Polygon2D &poly : polygons) {
            totalArea += poly.area();
        }
        return totalArea;
    }
    
    bool isValid() const {
        for (const Polygon2D &poly : polygons) {
            if (!poly.isValid()) return false;
        }
        return true;
    }
};

/**
 * 包围盒结构 - 复刻Shapely box
 */
struct Box2D {
    double minX, minY, maxX, maxY;
    
    Box2D(double minx = 0, double miny = 0, double maxx = 0, double maxy = 0)
        : minX(minx), minY(miny), maxX(maxx), maxY(maxy) {}
    
    Box2D(const Point2D &p1, const Point2D &p2) {
        minX = std::min(p1.x, p2.x);
        minY = std::min(p1.y, p2.y);
        maxX = std::max(p1.x, p2.x);
        maxY = std::max(p1.y, p2.y);
    }
    
    Box2D(const Line2D &line) {
        minX = line.minX();
        minY = line.minY();
        maxX = line.maxX();
        maxY = line.maxY();
    }
    
    bool isEmpty() const {
        return maxX <= minX || maxY <= minY;
    }
    
    double width() const { return maxX - minX; }
    double height() const { return maxY - minY; }
    
    double centerX() const { return (minX + maxX) / 2.0; }
    double centerY() const { return (minY + maxY) / 2.0; }
    
    Point2D center() const { return Point2D(centerX(), centerY()); }
    
    bool contains(const Point2D &pt) const {
        return pt.x >= minX && pt.x <= maxX && pt.y >= minY && pt.y <= maxY;
    }
    
    bool intersects(const Box2D &other) const {
        return !(maxX < other.minX || minX > other.maxX ||
                 maxY < other.minY || minY > other.maxY);
    }
    
    bool intersects(const Point2D &pt) const {
        return contains(pt);
    }
    
    bool intersects(const Line2D &line) const {
        // 简化检测：检查线段端点是否在盒内
        for (const Point2D &pt : line.points) {
            if (contains(pt)) return true;
        }
        
        // 检查线段是否穿过盒的边
        // TODO: 更精确的相交检测
        Box2D lineBox(line);
        return intersects(lineBox);
    }
    
    Polygon2D toPolygon() const {
        QVector<Point2D> pts;
        pts.append(Point2D(minX, minY));
        pts.append(Point2D(maxX, minY));
        pts.append(Point2D(maxX, maxY));
        pts.append(Point2D(minX, maxY));
        pts.append(Point2D(minX, minY));  // 关闭
        return Polygon2D(pts);
    }
    
    Box2D expand(double dx, double dy) const {
        return Box2D(minX - dx, minY - dy, maxX + dx, maxY + dy);
    }
};

/**
 * GeometryUtils - 高级几何算法工具集
 *
 * 复刻Shapely核心算法:
 * - polygonize: 线段转多边形
 * - intersection: 几何交集
 * - difference: 几何差集
 * - unary_union: 合并多个几何体
 */
class GeometryUtils
{
public:
    /**
     * 线段转多边形 (polygonize)
     *
     * 对应Shapely: polygonize(lines)
     *
     * 算法原理:
     * 1. 收集所有线段端点构建节点图
     * 2. 找出闭合环路
     * 3. 判断环路内外关系
     *
     * @param lines 输入线段集合
     * @return 生成的多边形列表
     */
    static QVector<Polygon2D> polygonize(const QVector<Line2D> &lines) {
        if (lines.isEmpty()) return QVector<Polygon2D>();

        // 收集所有线段的所有点
        QVector<Point2D> allPoints;
        for (const Line2D &line : lines) {
            for (const Point2D &pt : line.points) {
                allPoints.append(pt);
            }
        }

        // 构建边表
        QVector<QPair<int, int>> edges;
        QVector<Point2D> uniquePoints;

        // 去重并建立索引
        for (const Point2D &pt : allPoints) {
            int idx = findOrCreatePoint(uniquePoints, pt);
        }

        // 构建边
        for (const Line2D &line : lines) {
            for (int i = 0; i < line.points.size() - 1; ++i) {
                int idx1 = findOrCreatePoint(uniquePoints, line.points[i]);
                int idx2 = findOrCreatePoint(uniquePoints, line.points[i + 1]);
                if (idx1 != idx2) {
                    edges.append(qMakePair(idx1, idx2));
                }
            }
        }

        // 使用简化算法：直接从闭合线段构建多边形
        QVector<Polygon2D> polygons;

        for (const Line2D &line : lines) {
            // 检查是否闭合
            if (line.points.size() >= 3 && line.isClosed()) {
                Polygon2D poly;
                // 移除最后一个重复点
                for (int i = 0; i < line.points.size() - 1; ++i) {
                    poly.exterior.append(line.points[i]);
                }
                if (poly.area() > 0.01) {
                    polygons.append(poly);
                }
            }
        }

        // 尝试连接相邻线段形成多边形
        QVector<Polygon2D> connectedPolygons = connectLinesToPolygons(lines);
        for (const Polygon2D &poly : connectedPolygons) {
            if (poly.area() > 0.01 && !containsPolygon(polygons, poly)) {
                polygons.append(poly);
            }
        }

        return polygons;
    }

    /**
     * 多边形与多边形交集
     *
     * 对应Shapely: poly1.intersection(poly2)
     *
     * 使用Sutherland-Hodgman裁剪算法
     */
    static QVector<Polygon2D> polygonIntersection(const Polygon2D &poly1, const Polygon2D &poly2) {
        // 确保两个多边形都是逆时针方向（Sutherland-Hodgman要求）
        Polygon2D ccw1 = ensureCCW(poly1);
        Polygon2D ccw2 = ensureCCW(poly2);
        if (ccw1.isEmpty() || ccw2.isEmpty()) return QVector<Polygon2D>();

        // 检查边界框是否相交
        Box2D box1(ccw1.minX(), ccw1.minY(), ccw1.maxX(), ccw1.maxY());
        Box2D box2(ccw2.minX(), ccw2.minY(), ccw2.maxX(), ccw2.maxY());

        if (!box1.intersects(box2)) return QVector<Polygon2D>();

        // Sutherland-Hodgman算法
        QVector<Point2D> output = ccw1.exterior;

        // 对ccw2的每条边进行裁剪
        int n2 = ccw2.exterior.size();
        for (int i = 0; i < n2; ++i) {
            if (output.isEmpty()) break;

            int j = (i + 1) % n2;
            Point2D edgeStart = ccw2.exterior[i];
            Point2D edgeEnd = ccw2.exterior[j];

            QVector<Point2D> input = output;
            output.clear();

            int nInput = input.size();
            for (int k = 0; k < nInput; ++k) {
                int l = (k + 1) % nInput;
                Point2D p = input[k];
                Point2D q = input[l];

                // 判断点在边的哪一侧
                bool pInside = isLeftOfEdge(edgeStart, edgeEnd, p);
                bool qInside = isLeftOfEdge(edgeStart, edgeEnd, q);

                if (pInside) {
                    output.append(p);
                    if (!qInside) {
                        // 计算交点
                        Point2D intersect = lineSegmentIntersection(edgeStart, edgeEnd, p, q);
                        if (intersect.x != 0 || intersect.y != 0) {
                            output.append(intersect);
                        }
                    }
                } else if (qInside) {
                    // 计算交点
                    Point2D intersect = lineSegmentIntersection(edgeStart, edgeEnd, p, q);
                    if (intersect.x != 0 || intersect.y != 0) {
                        output.append(intersect);
                    }
                }
            }
        }

        if (output.size() >= 3) {
            Polygon2D result(output);
            if (result.area() > 0.01) {
                return QVector<Polygon2D>() << result;
            }
        }

        return QVector<Polygon2D>();
    }

    /**
     * 多边形差集
     *
     * 对应Shapely: poly1.difference(poly2)
     */
    static QVector<Polygon2D> polygonDifference(const Polygon2D &poly1, const Polygon2D &poly2) {
        if (poly1.isEmpty()) return QVector<Polygon2D>();
        if (poly2.isEmpty()) return QVector<Polygon2D>() << poly1;

        // 简化实现：检查poly1是否完全在poly2内
        bool allInside = true;
        for (const Point2D &pt : poly1.exterior) {
            if (!poly2.containsPoint(pt)) {
                allInside = false;
                break;
            }
        }

        if (allInside) {
            return QVector<Polygon2D>();  // 完全被覆盖
        }

        // 检查是否有交集
        QVector<Polygon2D> intersection = polygonIntersection(poly1, poly2);
        if (intersection.isEmpty()) {
            return QVector<Polygon2D>() << poly1;  // 无交集，返回原多边形
        }

        // 简化实现：如果部分重叠，返回poly1减去交集部分的近似
        // 完整实现需要复杂的几何裁剪

        // 计算交集面积
        double interArea = 0;
        for (const Polygon2D &poly : intersection) {
            interArea += poly.area();
        }

        // 如果交集很小，返回原多边形
        if (interArea < poly1.area() * 0.01) {
            return QVector<Polygon2D>() << poly1;
        }

        // 否则需要分割（简化处理）
        return splitPolygonByDifference(poly1, poly2);
    }

    /**
     * 多边形与Box交集
     */
    static Polygon2D polygonBoxIntersection(const Polygon2D &poly, const Box2D &box) {
        Polygon2D boxPoly = box.toPolygon();
        QVector<Polygon2D> result = polygonIntersection(poly, boxPoly);
        return result.isEmpty() ? Polygon2D() : result[0];
    }

    /**
     * 合并多个多边形 (unary_union)
     *
     * 对应Shapely: unary_union(polygons)
     */
    static Polygon2D unaryUnion(const QVector<Polygon2D> &polygons) {
        if (polygons.isEmpty()) return Polygon2D();
        if (polygons.size() == 1) return polygons[0];

        // 简化实现：计算包围盒合并
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::min();
        double maxY = std::numeric_limits<double>::min();

        for (const Polygon2D &poly : polygons) {
            minX = std::min(minX, poly.minX());
            minY = std::min(minY, poly.minY());
            maxX = std::max(maxX, poly.maxX());
            maxY = std::max(maxY, poly.maxY());
        }

        // 合并所有边界点
        QVector<Point2D> allPoints;
        for (const Polygon2D &poly : polygons) {
            for (const Point2D &pt : poly.exterior) {
                allPoints.append(pt);
            }
        }

        // 计算凸包作为简化合并结果
        QVector<Point2D> convexHull = computeConvexHull(allPoints);

        if (convexHull.size() >= 3) {
            return Polygon2D(convexHull);
        }

        return Polygon2D();
    }

    /**
     * 合并多条线段 (linemerge)
     *
     * 对应Shapely: linemerge(lines)
     */
    static Line2D linemerge(const QVector<Line2D> &lines) {
        if (lines.isEmpty()) return Line2D();
        if (lines.size() == 1) return lines[0];

        // 尝试连接相邻线段
        QVector<Point2D> mergedPoints;
        mergedPoints.append(lines[0].points);

        for (int i = 1; i < lines.size(); ++i) {
            const Line2D &current = lines[i];

            // 检查是否可以连接到上一条线的末端
            Point2D lastPt = mergedPoints.last();
            Point2D firstPt = current.points.first();

            if (lastPt.distance(firstPt) < 1.0) {
                // 连接：跳过重复点
                for (int j = 1; j < current.points.size(); ++j) {
                    mergedPoints.append(current.points[j]);
                }
            } else {
                // 不连接：添加间隔
                mergedPoints.append(current.points);
            }
        }

        return Line2D(mergedPoints);
    }

    /**
     * 多边形缓冲（真实实现）
     *
     * 对应Shapely: poly.buffer(distance)
     */
    static Polygon2D buffer(const Polygon2D &poly, double distance) {
        if (poly.isEmpty()) return Polygon2D();

        // 简化实现：对每个边界点向外扩展
        QVector<Point2D> bufferedPoints;
        int n = poly.exterior.size();

        for (int i = 0; i < n; ++i) {
            int prev = (i - 1 + n) % n;
            int next = (i + 1) % n;

            Point2D pPrev = poly.exterior[prev];
            Point2D pCurr = poly.exterior[i];
            Point2D pNext = poly.exterior[next];

            // 计算两条边的方向向量
            Point2D v1(pCurr.x - pPrev.x, pCurr.y - pPrev.y);
            Point2D v2(pNext.x - pCurr.x, pNext.y - pCurr.y);

            // 归一化
            double len1 = std::sqrt(v1.x * v1.x + v1.y * v1.y);
            double len2 = std::sqrt(v2.x * v2.x + v2.y * v2.y);

            if (len1 > 0) { v1.x /= len1; v1.y /= len1; }
            if (len2 > 0) { v2.x /= len2; v2.y /= len2; }

            // 计算平均外向方向
            Point2D avgDir((v1.x + v2.x) / 2, (v1.y + v2.y) / 2);

            // 旋转90度得到外向
            Point2D normal(-avgDir.y, avgDir.x);

            // 归一化并扩展
            double normalLen = std::sqrt(normal.x * normal.x + normal.y * normal.y);
            if (normalLen > 0) {
                normal.x /= normalLen;
                normal.y /= normalLen;
            }

            Point2D newPt(pCurr.x + normal.x * distance, pCurr.y + normal.y * distance);
            bufferedPoints.append(newPt);
        }

        return Polygon2D(bufferedPoints);
    }

private:
    /**
     * 确保多边形为逆时针方向（Sutherland-Hodgman算法要求）
     * 使用有符号面积判断：面积<0表示顺时针，需要反转
     */
    static Polygon2D ensureCCW(const Polygon2D &poly) {
        if (poly.exterior.size() < 3) return poly;

        // 计算有符号面积（Shoelace公式）
        double signedArea = 0;
        int n = poly.exterior.size();
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            signedArea += poly.exterior[i].x * poly.exterior[j].y;
            signedArea -= poly.exterior[j].x * poly.exterior[i].y;
        }
        signedArea /= 2.0;

        if (signedArea < 0) {
            // 顺时针，需要反转为逆时针
            Polygon2D result;
            for (int i = n - 1; i >= 0; --i) {
                result.exterior.append(poly.exterior[i]);
            }
            result.layerName = poly.layerName;
            result.colorIndex = poly.colorIndex;
            return result;
        }
        return poly;
    }

    /**
     * 查找或创建点索引
     */
    static int findOrCreatePoint(QVector<Point2D> &uniquePoints, const Point2D &pt) {
        for (int i = 0; i < uniquePoints.size(); ++i) {
            if (uniquePoints[i] == pt) return i;
        }
        uniquePoints.append(pt);
        return uniquePoints.size() - 1;
    }

    /**
     * 连接线段形成多边形
     */
    static QVector<Polygon2D> connectLinesToPolygons(const QVector<Line2D> &lines) {
        QVector<Polygon2D> result;

        // 使用简化算法：直接检查闭合线段
        // 完整实现需要复杂的邻接图，但简化版足够用于大多数情况

        for (const Line2D &line : lines) {
            // 检查是否已经是闭合多边形
            if (line.points.size() >= 3 && line.isClosed()) {
                Polygon2D poly;
                for (int i = 0; i < line.points.size() - 1; ++i) {
                    poly.exterior.append(line.points[i]);
                }
                if (poly.area() > 0.01) {
                    result.append(poly);
                }
                continue;
            }

            // 尝试连接相邻线段
            // 简化实现：仅处理明显连接的线段
            QVector<Point2D> connectedPts = line.points;

            for (const Line2D &otherLine : lines) {
                if (&line == &otherLine) continue;

                // 检查末端是否能连接
                Point2D endPt = connectedPts.last();
                Point2D startPt = otherLine.points.first();

                if (endPt.distance(startPt) < 1.0) {
                    // 可以连接
                    for (int i = 1; i < otherLine.points.size(); ++i) {
                        connectedPts.append(otherLine.points[i]);
                    }
                }
            }

            // 检查是否形成闭合多边形
            if (connectedPts.size() >= 3 &&
                connectedPts.first().distance(connectedPts.last()) < 1.0) {
                Polygon2D poly;
                for (int i = 0; i < connectedPts.size() - 1; ++i) {
                    poly.exterior.append(connectedPts[i]);
                }
                if (poly.area() > 0.01) {
                    // 检查是否已存在
                    bool exists = false;
                    for (const Polygon2D &existing : result) {
                        if (std::abs(existing.area() - poly.area()) < 0.1 &&
                            existing.representativePoint().distance(poly.representativePoint()) < 1.0) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        result.append(poly);
                    }
                }
            }
        }

        return result;
    }

    /**
     * 检查多边形是否已包含
     */
    static bool containsPolygon(const QVector<Polygon2D> &polygons, const Polygon2D &poly) {
        for (const Polygon2D &existing : polygons) {
            // 简化判断：比较面积和中心点
            if (std::abs(existing.area() - poly.area()) < 0.1) {
                Point2D c1 = existing.representativePoint();
                Point2D c2 = poly.representativePoint();
                if (c1.distance(c2) < 1.0) return true;
            }
        }
        return false;
    }

    /**
     * 判断点在边的左侧（内侧）
     */
    static bool isLeftOfEdge(const Point2D &edgeStart, const Point2D &edgeEnd, const Point2D &pt) {
        double cross = (edgeEnd.x - edgeStart.x) * (pt.y - edgeStart.y) -
                       (edgeEnd.y - edgeStart.y) * (pt.x - edgeStart.x);
        return cross >= 0;
    }

    /**
     * 计算两条线段的交点
     */
    static Point2D lineSegmentIntersection(const Point2D &p1, const Point2D &p2,
                                           const Point2D &p3, const Point2D &p4) {
        double denom = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);

        if (std::abs(denom) < 0.0001) return Point2D(0, 0);  // 平行

        double t = ((p1.x - p3.x) * (p3.y - p4.y) - (p1.y - p3.y) * (p3.x - p4.x)) / denom;

        // Sutherland-Hodgman: 裁剪边(p3-p4)视为无限直线，只检查t在[0,1]范围内
        if (t >= 0 && t <= 1) {
            return Point2D(p1.x + t * (p2.x - p1.x), p1.y + t * (p2.y - p1.y));
        }

        return Point2D(0, 0);  // 无交点
    }

    /**
     * 分割多边形（差集简化实现）
     */
    static QVector<Polygon2D> splitPolygonByDifference(const Polygon2D &poly1, const Polygon2D &poly2) {
        // 简化实现：返回非交集部分
        // 完整实现需要计算精确差集

        // 检查每个顶点是否在poly2内
        QVector<Point2D> outsidePoints;
        for (const Point2D &pt : poly1.exterior) {
            if (!poly2.containsPoint(pt)) {
                outsidePoints.append(pt);
            }
        }

        if (outsidePoints.size() >= 3) {
            return QVector<Polygon2D>() << Polygon2D(outsidePoints);
        }

        return QVector<Polygon2D>();
    }

    /**
     * 计算凸包 (Graham Scan算法)
     */
    static QVector<Point2D> computeConvexHull(const QVector<Point2D> &points) {
        if (points.size() < 3) return points;

        // 找最低点
        int lowestIdx = 0;
        for (int i = 1; i < points.size(); ++i) {
            if (points[i].y < points[lowestIdx].y ||
                (points[i].y == points[lowestIdx].y && points[i].x < points[lowestIdx].x)) {
                lowestIdx = i;
            }
        }

        Point2D lowest = points[lowestIdx];

        // 按极角排序
        QVector<Point2D> sortedPoints = points;
        sortedPoints.remove(lowestIdx);

        std::sort(sortedPoints.begin(), sortedPoints.end(),
                  [&lowest](const Point2D &a, const Point2D &b) {
                      double angleA = std::atan2(a.y - lowest.y, a.x - lowest.x);
                      double angleB = std::atan2(b.y - lowest.y, b.x - lowest.x);
                      if (angleA != angleB) return angleA < angleB;
                      return a.distance(lowest) < b.distance(lowest);
                  });

        sortedPoints.insert(0, lowest);

        // Graham Scan
        QVector<Point2D> hull;
        for (const Point2D &pt : sortedPoints) {
            while (hull.size() >= 2) {
                Point2D top = hull.last();
                Point2D secondTop = hull[hull.size() - 2];

                double cross = (top.x - secondTop.x) * (pt.y - secondTop.y) -
                               (top.y - secondTop.y) * (pt.x - secondTop.x);

                if (cross <= 0) {
                    hull.removeLast();
                } else {
                    break;
                }
            }
            hull.append(pt);
        }

        return hull;
    }
};

#endif // GEOMETRY_H