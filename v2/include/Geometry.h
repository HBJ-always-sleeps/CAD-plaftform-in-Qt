#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <QtGlobal>
#include <QtMath>
#include <QString>
#include <QVector>
#include <QPointF>
#include <QPair>
#include <QColor>
#include <QDebug>
#include <QHash>
#include <cmath>
#include <algorithm>
#include <limits>

// Parametric segment-segment intersection test (no allocation)
inline bool segmentIntersects(double x1, double y1, double x2, double y2,
                               double x3, double y3, double x4, double y4) {
    double d = (x1-x2)*(y3-y4) - (y1-y2)*(x3-x4);
    if (std::abs(d) < 1e-10) return false;
    double t = ((x1-x3)*(y3-y4) - (y1-y3)*(x3-x4)) / d;
    double u = ((x1-x3)*(y1-y2) - (y1-y3)*(x1-x2)) / d;
    return t >= 0 && t <= 1 && u >= 0 && u <= 1;
}

struct Point2D {
    double x, y;
    Point2D(double x_ = 0, double y_ = 0) : x(x_), y(y_) {}
    Point2D(const QPointF &p) : x(p.x()), y(p.y()) {}
    QPointF toQPointF() const { return QPointF(x, y); }
    double distance(const Point2D &o) const { double dx=x-o.x, dy=y-o.y; return std::sqrt(dx*dx+dy*dy); }
    double distanceSq(const Point2D &o) const { double dx=x-o.x, dy=y-o.y; return dx*dx+dy*dy; }
    Point2D operator+(const Point2D &o) const { return Point2D(x+o.x, y+o.y); }
    Point2D operator-(const Point2D &o) const { return Point2D(x-o.x, y-o.y); }
    Point2D operator*(double f) const { return Point2D(x*f, y*f); }
    bool operator==(const Point2D &o) const { return std::abs(x-o.x)<0.001 && std::abs(y-o.y)<0.001; }
    bool operator!=(const Point2D &o) const { return !(*this==o); }
    bool operator<(const Point2D &o) const { return std::abs(x-o.x)>0.001 ? x<o.x : y<o.y; }
    QString toString() const { return QString("(%1, %2)").arg(x,0,'f',3).arg(y,0,'f',3); }
};

struct Line2D;     // forward declaration
struct Polygon2D;  // forward declaration

struct Box2D {
    double minX, minY, maxX, maxY;
    Box2D(double x0=0, double y0=0, double x1=0, double y1=0) : minX(x0), minY(y0), maxX(x1), maxY(y1) {}
    Box2D(const Point2D &p1, const Point2D &p2)
        : minX(std::min(p1.x,p2.x)), minY(std::min(p1.y,p2.y)), maxX(std::max(p1.x,p2.x)), maxY(std::max(p1.y,p2.y)) {}
    Box2D(const Line2D &l);  // defined after Line2D
    bool isEmpty() const { return maxX<=minX || maxY<=minY; }
    double width() const { return maxX-minX; }
    double height() const { return maxY-minY; }
    double centerX() const { return (minX+maxX)/2.0; }
    double centerY() const { return (minY+maxY)/2.0; }
    double midX() const { return centerX(); }
    double midY() const { return centerY(); }
    Point2D center() const { return Point2D(centerX(), centerY()); }
    bool contains(const Point2D &pt) const { return pt.x>=minX && pt.x<=maxX && pt.y>=minY && pt.y<=maxY; }
    bool intersects(const Box2D &o) const { return !(maxX<o.minX || minX>o.maxX || maxY<o.minY || minY>o.maxY); }
    bool intersects(const Point2D &pt) const { return contains(pt); }
    bool intersects(const Line2D &line) const;  // defined after Line2D
    Polygon2D toPolygon() const;  // defined after Polygon2D
    Box2D expand(double dx, double dy) const { return Box2D(minX-dx, minY-dy, maxX+dx, maxY+dy); }
};

struct Line2D {
    QVector<Point2D> points;
    QString layerName;
    int color = 256;
    QString linetype = "ByLayer";

    Line2D() = default;
    Line2D(const QVector<Point2D> &pts) : points(pts) {}
    Line2D(const QVector<QPointF> &pts) { for (const QPointF &p : pts) points.append(Point2D(p)); }

    bool isEmpty() const { return points.isEmpty(); }
    int size() const { return points.size(); }

    void invalidateBounds() { m_boundsDirty = true; }

    Box2D bounds() const {
        if (!m_boundsDirty) return m_bounds;
        if (points.isEmpty()) return Box2D();

        double x0=points[0].x, y0=points[0].y, x1=x0, y1=y0;
        for (int i=1; i<points.size(); ++i) {
            x0 = std::min(x0, points[i].x); y0 = std::min(y0, points[i].y);
            x1 = std::max(x1, points[i].x); y1 = std::max(y1, points[i].y);
        }
        m_bounds = Box2D(x0, y0, x1, y1);
        m_boundsDirty = false;
        return m_bounds;
    }

    double minX() const { return bounds().minX; }
    double maxX() const { return bounds().maxX; }
    double minY() const { return bounds().minY; }
    double maxY() const { return bounds().maxY; }
    double midX() const { return bounds().midX(); }
    double midY() const { return bounds().midY(); }
    double xCenter() const { return midX(); }
    double yCenter() const { return midY(); }

    double length() const {
        double len = 0;
        for (int i = 0; i < points.size()-1; ++i) len += points[i].distance(points[i+1]);
        return len;
    }

    Point2D startPoint() const { return points.isEmpty() ? Point2D() : points.first(); }
    Point2D endPoint() const { return points.isEmpty() ? Point2D() : points.last(); }
    Point2D representativePoint() const {
        if (points.isEmpty()) return Point2D();
        return points[points.size()/2];
    }

    bool isValid() const { return points.size() >= 2; }
    bool isClosed() const { return points.size() >= 3 && points.first() == points.last(); }

private:
    mutable Box2D m_bounds;
    mutable bool m_boundsDirty = true;
};

// Box2D methods that depend on Line2D
inline Box2D::Box2D(const Line2D &l) : minX(l.minX()), minY(l.minY()), maxX(l.maxX()), maxY(l.maxY()) {}
inline bool Box2D::intersects(const Line2D &line) const {
    for (const Point2D &pt : line.points) if (contains(pt)) return true;
    // Check if any line segment crosses any box edge
    double bx[4] = {minX, maxX, maxX, minX};
    double by[4] = {minY, minY, maxY, maxY};
    for (int i = 0; i < line.points.size()-1; ++i) {
        for (int j = 0; j < 4; ++j) {
            int k = (j+1) % 4;
            if (segmentIntersects(line.points[i].x, line.points[i].y,
                                   line.points[i+1].x, line.points[i+1].y,
                                   bx[j], by[j], bx[k], by[k]))
                return true;
        }
    }
    return false;
}

struct Polygon2D {
    QVector<Point2D> exterior;
    QVector<QVector<Point2D>> interiors;
    QString layerName;
    int colorIndex = 256;
    QString pattern = QStringLiteral("SOLID");
    double scale = 1.0;
    QColor rgbColor;

    Polygon2D() = default;
    Polygon2D(const QVector<Point2D> &ext) : exterior(ext) {}
    Polygon2D(const QVector<QPointF> &ext) { for (const QPointF &p : ext) exterior.append(Point2D(p)); }

    bool isEmpty() const { return exterior.isEmpty(); }
    bool isValid() const { return exterior.size() >= 3; }

    double area() const {
        if (exterior.size() < 3) return 0;
        double a = computeRingArea(exterior);
        for (const auto &h : interiors) a -= computeRingArea(h);
        return std::abs(a);
    }

    static double computeRingArea(const QVector<Point2D> &ring) {
        if (ring.size() < 3) return 0;
        double sum = 0; int n = ring.size();
        for (int i = 0; i < n; ++i) { int j = (i+1) % n; sum += ring[i].x * ring[j].y - ring[j].x * ring[i].y; }
        return std::abs(sum) / 2.0;
    }

    void invalidateBounds() { m_boundsDirty = true; }

    Box2D bounds() const {
        if (!m_boundsDirty) return m_bounds;
        if (exterior.isEmpty()) return Box2D();

        double x0=exterior[0].x, y0=exterior[0].y, x1=x0, y1=y0;
        for (int i=1; i<exterior.size(); ++i) {
            x0 = std::min(x0, exterior[i].x); y0 = std::min(y0, exterior[i].y);
            x1 = std::max(x1, exterior[i].x); y1 = std::max(y1, exterior[i].y);
        }
        m_bounds = Box2D(x0, y0, x1, y1);
        m_boundsDirty = false;
        return m_bounds;
    }

    double minX() const { return bounds().minX; }
    double maxX() const { return bounds().maxX; }
    double minY() const { return bounds().minY; }
    double maxY() const { return bounds().maxY; }

    Point2D representativePoint() const {
        if (exterior.isEmpty()) return Point2D();
        double cx=0, cy=0;
        for (const Point2D &p : exterior) { cx+=p.x; cy+=p.y; }
        cx /= exterior.size(); cy /= exterior.size();
        Point2D center(cx, cy);
        return containsPoint(center) ? center : exterior[exterior.size()/2];
    }

    bool containsPoint(const Point2D &pt) const {
        if (exterior.size() < 3) return false;
        if (!rayCast(pt, exterior)) return false;
        for (const auto &h : interiors) if (rayCast(pt, h)) return false;
        return true;
    }

    static bool rayCast(const Point2D &pt, const QVector<Point2D> &ring) {
        int count = 0, n = ring.size();
        for (int i = 0; i < n; ++i) {
            int j = (i+1) % n;
            if ((ring[i].y > pt.y) != (ring[j].y > pt.y)) {
                double xi = ring[i].x + (pt.y-ring[i].y) / (ring[j].y-ring[i].y) * (ring[j].x-ring[i].x);
                if (pt.x < xi) count++;
            }
        }
        return (count % 2) == 1;
    }

private:
    mutable Box2D m_bounds;
    mutable bool m_boundsDirty = true;
};

// Box2D methods that depend on Polygon2D
inline Polygon2D Box2D::toPolygon() const {
    return Polygon2D(QVector<Point2D>{{minX,minY},{maxX,minY},{maxX,maxY},{minX,maxY},{minX,minY}});
}

struct MultiPolygon2D {
    QVector<Polygon2D> polygons;
    MultiPolygon2D() = default;
    MultiPolygon2D(const QVector<Polygon2D> &p) : polygons(p) {}
    bool isEmpty() const { return polygons.isEmpty(); }
    int size() const { return polygons.size(); }
    double area() const { double a=0; for (const auto &p : polygons) a+=p.area(); return a; }
    bool isValid() const { for (const auto &p : polygons) if (!p.isValid()) return false; return true; }
};

class GeometryUtils
{
public:
    static QVector<Polygon2D> polygonize(const QVector<Line2D> &lines) {
        if (lines.isEmpty()) return {};
        QVector<Polygon2D> polygons;
        for (const Line2D &line : lines) {
            if (line.points.size() >= 3 && line.isClosed()) {
                Polygon2D poly;
                for (int i = 0; i < line.points.size()-1; ++i) poly.exterior.append(line.points[i]);
                if (poly.area() > 0.01) polygons.append(poly);
            }
        }
        QVector<Polygon2D> connected = connectLinesToPolygons(lines);
        for (const Polygon2D &p : connected)
            if (p.area() > 0.01 && !containsPolygon(polygons, p)) polygons.append(p);
        return polygons;
    }

    static QVector<Polygon2D> polygonIntersection(const Polygon2D &poly1, const Polygon2D &poly2) {
        Polygon2D c1 = ensureCCW(poly1), c2 = ensureCCW(poly2);
        if (c1.isEmpty() || c2.isEmpty()) return {};
        Box2D b1(c1.bounds()), b2(c2.bounds());
        if (!b1.intersects(b2)) return {};

        QVector<Point2D> output = c1.exterior;
        int n2 = c2.exterior.size();
        for (int i = 0; i < n2; ++i) {
            if (output.isEmpty()) break;
            int j = (i+1) % n2;
            Point2D es = c2.exterior[i], ee = c2.exterior[j];
            QVector<Point2D> input = output; output.clear();
            int ni = input.size();
            for (int k = 0; k < ni; ++k) {
                int l = (k+1) % ni;
                bool pIn = isLeftOfEdge(es, ee, input[k]);
                bool qIn = isLeftOfEdge(es, ee, input[l]);
                if (pIn) {
                    output.append(input[k]);
                    if (!qIn) { Point2D ix = lineSegIsect(es, ee, input[k], input[l]); if (ix.x||ix.y) output.append(ix); }
                } else if (qIn) {
                    Point2D ix = lineSegIsect(es, ee, input[k], input[l]); if (ix.x||ix.y) output.append(ix);
                }
            }
        }
        if (output.size() >= 3) {
            Polygon2D r(output);
            if (r.area() > 0.01) return {r};
        }
        return {};
    }

    static QVector<Polygon2D> polygonDifference(const Polygon2D &poly1, const Polygon2D &poly2) {
        if (poly1.isEmpty()) return {};
        if (poly2.isEmpty()) return {poly1};
        Box2D b1(poly1.bounds()), b2(poly2.bounds());
        if (!b1.intersects(b2)) return {poly1};
        bool allIn = true;
        for (const Point2D &pt : poly1.exterior)
            if (!poly2.containsPoint(pt)) { allIn = false; break; }
        if (allIn) return {};
        QVector<Polygon2D> ix = polygonIntersection(poly1, poly2);
        if (ix.isEmpty()) return {poly1};
        double ia = 0; for (const auto &p : ix) ia += p.area();
        if (ia < poly1.area() * 0.01) return {poly1};
        return splitPolygonByDifference(poly1, poly2);
    }

    static Polygon2D polygonBoxIntersection(const Polygon2D &poly, const Box2D &box) {
        auto r = polygonIntersection(poly, box.toPolygon());
        return r.isEmpty() ? Polygon2D() : r[0];
    }

    static Polygon2D unaryUnion(const QVector<Polygon2D> &polygons) {
        if (polygons.isEmpty()) return {};
        if (polygons.size() == 1) return polygons[0];
        double x0=std::numeric_limits<double>::max(), y0=x0;
        double x1=std::numeric_limits<double>::min(), y1=x1;
        QVector<Point2D> allPts;
        for (const auto &p : polygons) {
            x0=std::min(x0,p.minX()); y0=std::min(y0,p.minY());
            x1=std::max(x1,p.maxX()); y1=std::max(y1,p.maxY());
            for (const Point2D &pt : p.exterior) allPts.append(pt);
        }
        auto hull = computeConvexHull(allPts);
        return hull.size()>=3 ? Polygon2D(hull) : Polygon2D();
    }

    static Line2D linemerge(const QVector<Line2D> &lines) {
        if (lines.isEmpty()) return {};
        if (lines.size() == 1) return lines[0];
        QVector<Point2D> merged = lines[0].points;
        for (int i = 1; i < lines.size(); ++i) {
            if (merged.last().distance(lines[i].points.first()) < 1.0) {
                for (int j = 1; j < lines[i].points.size(); ++j) merged.append(lines[i].points[j]);
            } else {
                merged.append(lines[i].points);
            }
        }
        return Line2D(merged);
    }

    static Polygon2D buffer(const Polygon2D &poly, double dist) {
        if (poly.isEmpty()) return {};
        QVector<Point2D> buf;
        int n = poly.exterior.size();
        for (int i = 0; i < n; ++i) {
            int prev = (i-1+n)%n, next = (i+1)%n;
            Point2D v1(poly.exterior[i].x-poly.exterior[prev].x, poly.exterior[i].y-poly.exterior[prev].y);
            Point2D v2(poly.exterior[next].x-poly.exterior[i].x, poly.exterior[next].y-poly.exterior[i].y);
            double l1=std::sqrt(v1.x*v1.x+v1.y*v1.y), l2=std::sqrt(v2.x*v2.x+v2.y*v2.y);
            if (l1>0) { v1.x/=l1; v1.y/=l1; }
            if (l2>0) { v2.x/=l2; v2.y/=l2; }
            Point2D avg((v1.x+v2.x)/2, (v1.y+v2.y)/2);
            Point2D norm(-avg.y, avg.x);
            double nl = std::sqrt(norm.x*norm.x+norm.y*norm.y);
            if (nl>0) { norm.x/=nl; norm.y/=nl; }
            buf.append(Point2D(poly.exterior[i].x+norm.x*dist, poly.exterior[i].y+norm.y*dist));
        }
        return Polygon2D(buf);
    }

private:
    static Polygon2D ensureCCW(const Polygon2D &poly) {
        if (poly.exterior.size() < 3) return poly;
        double sa = 0; int n = poly.exterior.size();
        for (int i = 0; i < n; ++i) { int j=(i+1)%n; sa += poly.exterior[i].x*poly.exterior[j].y - poly.exterior[j].x*poly.exterior[i].y; }
        if (sa < 0) {
            Polygon2D r;
            for (int i = n-1; i >= 0; --i) r.exterior.append(poly.exterior[i]);
            r.layerName = poly.layerName; r.colorIndex = poly.colorIndex;
            return r;
        }
        return poly;
    }

    static int findOrCreatePoint(QVector<Point2D> &pts, const Point2D &p, QHash<QString, int> &index) {
        QString key = QStringLiteral("%1,%2").arg(std::round(p.x * 1000)).arg(std::round(p.y * 1000));
        auto it = index.find(key);
        if (it != index.end()) return *it;
        int idx = pts.size();
        pts.append(p);
        index.insert(key, idx);
        return idx;
    }

    static QVector<Polygon2D> connectLinesToPolygons(const QVector<Line2D> &lines) {
        QVector<Polygon2D> result;
        for (const Line2D &line : lines) {
            if (line.points.size() >= 3 && line.isClosed()) {
                Polygon2D poly;
                for (int i = 0; i < line.points.size()-1; ++i) poly.exterior.append(line.points[i]);
                if (poly.area() > 0.01) result.append(poly);
                continue;
            }
            QVector<Point2D> pts = line.points;
            for (const Line2D &other : lines) {
                if (&line == &other) continue;
                if (pts.last().distance(other.points.first()) < 1.0)
                    for (int i = 1; i < other.points.size(); ++i) pts.append(other.points[i]);
            }
            if (pts.size() >= 3 && pts.first().distance(pts.last()) < 1.0) {
                Polygon2D poly;
                for (int i = 0; i < pts.size()-1; ++i) poly.exterior.append(pts[i]);
                if (poly.area() > 0.01 && !containsPolygon(result, poly)) result.append(poly);
            }
        }
        return result;
    }

    static bool containsPolygon(const QVector<Polygon2D> &polys, const Polygon2D &p) {
        for (const auto &e : polys)
            if (std::abs(e.area()-p.area()) < 0.1 && e.representativePoint().distance(p.representativePoint()) < 1.0) return true;
        return false;
    }

    static bool isLeftOfEdge(const Point2D &es, const Point2D &ee, const Point2D &pt) {
        return (ee.x-es.x)*(pt.y-es.y) - (ee.y-es.y)*(pt.x-es.x) >= 0;
    }

    static Point2D lineSegIsect(const Point2D &p1, const Point2D &p2, const Point2D &p3, const Point2D &p4) {
        double d = (p1.x-p2.x)*(p3.y-p4.y) - (p1.y-p2.y)*(p3.x-p4.x);
        if (std::abs(d) < 0.0001) return Point2D(0,0);
        double t = ((p1.x-p3.x)*(p3.y-p4.y) - (p1.y-p3.y)*(p3.x-p4.x)) / d;
        double u = ((p1.x-p3.x)*(p1.y-p2.y) - (p1.y-p3.y)*(p1.x-p2.x)) / d;
        if (t >= 0 && t <= 1 && u >= 0 && u <= 1) return Point2D(p1.x+t*(p2.x-p1.x), p1.y+t*(p2.y-p1.y));
        return Point2D(0,0);
    }

    static QVector<Polygon2D> splitPolygonByDifference(const Polygon2D &p1, const Polygon2D &p2) {
        QVector<Point2D> outside;
        for (const Point2D &pt : p1.exterior) if (!p2.containsPoint(pt)) outside.append(pt);
        return outside.size() >= 3 ? QVector<Polygon2D>{Polygon2D(outside)} : QVector<Polygon2D>{};
    }

    static QVector<Point2D> computeConvexHull(const QVector<Point2D> &points) {
        if (points.size() < 3) return points;
        int li = 0;
        for (int i = 1; i < points.size(); ++i)
            if (points[i].y < points[li].y || (points[i].y == points[li].y && points[i].x < points[li].x)) li = i;
        Point2D lowest = points[li];
        QVector<Point2D> sorted = points; sorted.remove(li);
        std::sort(sorted.begin(), sorted.end(), [&lowest](const Point2D &a, const Point2D &b) {
            double aa = std::atan2(a.y-lowest.y, a.x-lowest.x), ab = std::atan2(b.y-lowest.y, b.x-lowest.x);
            return aa != ab ? aa < ab : a.distance(lowest) < b.distance(lowest);
        });
        sorted.insert(0, lowest);
        QVector<Point2D> hull;
        for (const Point2D &pt : sorted) {
            while (hull.size() >= 2) {
                Point2D top = hull.last(), sec = hull[hull.size()-2];
                double cross = (top.x-sec.x)*(pt.y-sec.y) - (top.y-sec.y)*(pt.x-sec.x);
                if (cross <= 0) hull.removeLast(); else break;
            }
            hull.append(pt);
        }
        return hull;
    }
};

#endif // GEOMETRY_H
