#include "DXFWrapper.h"
#include "utils/LineUtils.h"
#include "Config.h"
#include "Geometry.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <QStringDecoder>

static QString decodeDxfBytes(const QByteArray &bytes) {
    static QStringDecoder gbk("GB18030");
    if (gbk.isValid()) return gbk.decode(bytes);
    return QString::fromLocal8Bit(bytes);
}

static QByteArray encodeDxfString(const QString &str) {
    static QStringEncoder gbk("GB18030");
    if (gbk.isValid()) return gbk.encode(str);
    return str.toLocal8Bit();
}

bool DXFWrapper::read(const QString &filePath)
{
    m_filePath = filePath;
    m_loaded = false;
    m_lines.clear(); m_texts.clear(); m_hatches.clear();
    m_layers.clear(); m_layerInfo.clear(); m_originalHeader.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) { qWarning() << "Cannot open DXF:" << filePath; return false; }

    // Pass 1: read HEADER
    bool foundHeader = false;
    while (!file.atEnd()) {
        QByteArray codeBytes = file.readLine(), valueBytes = file.readLine();
        QString code = decodeDxfBytes(codeBytes), value = decodeDxfBytes(valueBytes);
        while (code.endsWith('\r') || code.endsWith('\n')) code.chop(1);
        while (value.endsWith('\r') || value.endsWith('\n')) value.chop(1);
        m_originalHeader.append(code);
        m_originalHeader.append(value);
        if (code.trimmed() == "2" && value.trimmed() == "HEADER") foundHeader = true;
        if (foundHeader && code.trimmed() == "0" && value.trimmed() == "ENDSEC") break;
    }
    file.seek(0);

    QString curSection, curEntity, curLayer;
    double lineX1=0, lineY1=0, lineX2=0, lineY2=0;
    QVector<Point2D> polyPts; double polyX=0, polyY=0;
    QString textContent; double textX=0, textY=0;
    QVector<Point2D> hatchBoundary; int hatchEdgeCount=0; double hatchX=0, hatchY=0;
    QVector<Point2D> polylinePts;
    LayerInfo curLayerInfo; bool inTables=false, inLayerTable=false, inLayerRecord=false;

    auto saveLayerRecord = [&]() {
        if (inLayerRecord && !curLayerInfo.name.isEmpty()) {
            m_layerInfo[curLayerInfo.name] = curLayerInfo;
            if (!m_layers.contains(curLayerInfo.name)) m_layers.append(curLayerInfo.name);
        }
    };

    while (!file.atEnd()) {
        QString code = decodeDxfBytes(file.readLine()).trimmed();
        QString value = decodeDxfBytes(file.readLine()).trimmed();
        int codeNum = code.toInt();

        if (codeNum == 0) {
            if (curEntity == "LINE" && !curLayer.isEmpty()) {
                Line2D l; l.points.append(Point2D(lineX1,lineY1)); l.points.append(Point2D(lineX2,lineY2)); l.layerName = curLayer; m_lines.append(l);
            } else if (curEntity == "LWPOLYLINE" && polyPts.size() >= 2) {
                Line2D p; p.points = polyPts; p.layerName = curLayer; m_lines.append(p);
            } else if (curEntity == "POLYLINE" && polylinePts.size() >= 2) {
                Line2D p; p.points = polylinePts; p.layerName = curLayer; m_lines.append(p); polylinePts.clear();
            } else if ((curEntity == "TEXT" || curEntity == "MTEXT") && !textContent.isEmpty()) {
                TextInfo ti; ti.text = textContent; ti.x = textX; ti.y = textY; ti.layer = curLayer; m_texts.append(ti);
            } else if (curEntity == "HATCH" && hatchBoundary.size() >= 3) {
                Polygon2D hp(hatchBoundary); hp.layerName = curLayer; m_hatches.append(hp);
            }

            if (value == "SECTION") { curSection.clear(); curEntity.clear(); inTables=inLayerTable=inLayerRecord=false; }
            else if (value == "ENDSEC") { saveLayerRecord(); curSection.clear(); curEntity.clear(); inTables=inLayerTable=inLayerRecord=false; }
            else if (value == "SEQEND") { if (polylinePts.size()>=2) { Line2D p; p.points=polylinePts; p.layerName=curLayer; m_lines.append(p); } polylinePts.clear(); curEntity.clear(); }
            else if (value == "TABLE") { curEntity = "TABLE"; }
            else if (value == "ENDTAB") { saveLayerRecord(); curEntity.clear(); inLayerTable=inLayerRecord=false; }
            else if (value == "LAYER" && inLayerTable) { saveLayerRecord(); inLayerRecord=true; curLayerInfo=LayerInfo(); }
            else { curEntity = value.toUpper(); polyPts.clear(); textContent.clear(); hatchBoundary.clear(); hatchEdgeCount=0; }
            continue;
        }

        if (codeNum == 2) {
            if (curEntity.isEmpty() && curSection.isEmpty()) { curSection = value; if (value=="TABLES") inTables=true; }
            else if (curEntity == "TABLE" && value == "LAYER") inLayerTable = true;
            else if (inLayerRecord) curLayerInfo.name = value;
            continue;
        }
        if (codeNum == 8) { curLayer = value; if (!m_layers.contains(curLayer)) m_layers.append(curLayer); continue; }
        if (codeNum == 62 && inLayerRecord) { curLayerInfo.color = value.toInt(); continue; }
        if (codeNum == 6 && inLayerRecord) { curLayerInfo.linetype = value; continue; }
        if (codeNum == 370 && inLayerRecord) { curLayerInfo.lineweight = value.toInt(); continue; }

        if (codeNum == 10) {
            if (curEntity=="LINE") lineX1=value.toDouble();
            else if (curEntity=="LWPOLYLINE" || curEntity=="POLYLINE" || curEntity=="VERTEX") polyX=value.toDouble();
            else if (curEntity=="TEXT"||curEntity=="MTEXT") textX=value.toDouble();
            else if (curEntity=="HATCH") hatchX=value.toDouble();
        } else if (codeNum == 20) {
            if (curEntity=="LINE") lineY1=value.toDouble();
            else if (curEntity=="LWPOLYLINE") { polyY=value.toDouble(); polyPts.append(Point2D(polyX,polyY)); }
            else if (curEntity=="VERTEX") { polyY=value.toDouble(); polylinePts.append(Point2D(polyX,polyY)); }
            else if (curEntity=="TEXT"||curEntity=="MTEXT") textY=value.toDouble();
            else if (curEntity=="HATCH") { hatchY=value.toDouble(); if (hatchEdgeCount>0) hatchBoundary.append(Point2D(hatchX,hatchY)); }
        } else if (codeNum == 11 && curEntity=="LINE") lineX2=value.toDouble();
        else if (codeNum == 21 && curEntity=="LINE") lineY2=value.toDouble();
        else if (codeNum == 93 && curEntity=="HATCH") { hatchEdgeCount=value.toInt(); hatchBoundary.clear(); }
        else if (codeNum == 1 && (curEntity=="TEXT"||curEntity=="MTEXT")) textContent=value;
        else if (codeNum == 3 && curEntity=="MTEXT") textContent += value;
    }

    // Last entity
    if (curEntity == "LWPOLYLINE" && polyPts.size()>=2) { Line2D p; p.points=polyPts; p.layerName=curLayer; m_lines.append(p); }
    else if (curEntity == "POLYLINE" && polylinePts.size()>=2) { Line2D p; p.points=polylinePts; p.layerName=curLayer; m_lines.append(p); }
    else if ((curEntity=="TEXT"||curEntity=="MTEXT") && !textContent.isEmpty()) { TextInfo ti; ti.text=textContent; ti.x=textX; ti.y=textY; ti.layer=curLayer; m_texts.append(ti); }
    else if (curEntity == "HATCH" && hatchBoundary.size()>=3) { Polygon2D hp(hatchBoundary); hp.layerName=curLayer; m_hatches.append(hp); }

    file.close();
    m_loaded = true;
    return true;
}

// DXF table writing helpers
using WL = std::function<void(const QString&)>;

static void writeVPortTable(const WL &w) {
    w("  0"); w("TABLE"); w("  2"); w("VPORT"); w("  5"); w("8"); w("330"); w("0");
    w("100"); w("AcDbSymbolTable"); w(" 70"); w("1");
    w("  0"); w("VPORT"); w("  5"); w("EA"); w("330"); w("8");
    w("100"); w("AcDbSymbolTableRecord"); w("100"); w("AcDbViewportTableRecord");
    w("  2"); w("*Active"); w(" 70"); w("0");
    w(" 10"); w("0.0"); w(" 20"); w("0.0"); w(" 11"); w("1.0"); w(" 21"); w("1.0");
    w(" 12"); w("0.0"); w(" 22"); w("0.0"); w(" 13"); w("0.0"); w(" 23"); w("0.0");
    w(" 14"); w("10.0"); w(" 24"); w("10.0"); w(" 15"); w("10.0"); w(" 25"); w("10.0");
    w(" 16"); w("0.0"); w(" 26"); w("0.0"); w(" 36"); w("1.0");
    w(" 17"); w("0.0"); w(" 27"); w("0.0"); w(" 37"); w("0.0");
    w(" 40"); w("1000.0"); w(" 41"); w("1.0"); w(" 42"); w("50.0");
    w(" 43"); w("0.0"); w(" 44"); w("0.0"); w(" 50"); w("0.0"); w(" 51"); w("0.0");
    w(" 71"); w("0"); w(" 72"); w("1000"); w(" 73"); w("1"); w(" 74"); w("3");
    w(" 75"); w("0"); w(" 76"); w("0"); w(" 77"); w("0"); w(" 78"); w("0");
    w("281"); w("0"); w(" 65"); w("1"); w("  0"); w("ENDTAB");
}

static void writeLTypeTable(const WL &w) {
    w("  0"); w("TABLE"); w("  2"); w("LTYPE"); w("  5"); w("5"); w("330"); w("0");
    w("100"); w("AcDbSymbolTable"); w(" 70"); w("3");
    // ByBlock
    w("  0"); w("LTYPE"); w("  5"); w("14"); w("330"); w("5");
    w("100"); w("AcDbSymbolTableRecord"); w("100"); w("AcDbLinetypeTableRecord");
    w("  2"); w("ByBlock"); w(" 70"); w("0"); w("  3"); w(""); w(" 72"); w("65"); w(" 73"); w("0"); w(" 40"); w("0.0");
    // ByLayer
    w("  0"); w("LTYPE"); w("  5"); w("15"); w("330"); w("5");
    w("100"); w("AcDbSymbolTableRecord"); w("100"); w("AcDbLinetypeTableRecord");
    w("  2"); w("ByLayer"); w(" 70"); w("0"); w("  3"); w(""); w(" 72"); w("65"); w(" 73"); w("0"); w(" 40"); w("0.0");
    // Continuous
    w("  0"); w("LTYPE"); w("  5"); w("16"); w("330"); w("5");
    w("100"); w("AcDbSymbolTableRecord"); w("100"); w("AcDbLinetypeTableRecord");
    w("  2"); w("Continuous"); w(" 70"); w("0"); w("  3"); w("Solid line"); w(" 72"); w("65"); w(" 73"); w("0"); w(" 40"); w("0.0");
    w("  0"); w("ENDTAB");
}

bool DXFWrapper::save(const QString &filePath)
{
    QString outputPath = filePath.isEmpty() ? m_filePath : filePath;
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) { qWarning() << "Cannot save DXF:" << outputPath; return false; }

    auto w = [&file](const QString &s) { file.write(encodeDxfString(s)); file.write("\r\n"); };

    // HEADER
    if (!m_originalHeader.isEmpty()) {
        for (const QString &l : m_originalHeader) w(l);
    } else {
        w("  0"); w("SECTION"); w("  2"); w("HEADER");
        w("  9"); w("$ACADVER"); w("  1"); w("AC1032");
        w("  9"); w("$DWGCODEPAGE"); w("  3"); w("ANSI_936");
        w("  0"); w("ENDSEC");
    }

    // TABLES
    w("  0"); w("SECTION"); w("  2"); w("TABLES");
    writeVPortTable(w);
    writeLTypeTable(w);

    // LAYER table
    w("  0"); w("TABLE"); w("  2"); w("LAYER"); w("  5"); w("2"); w("330"); w("0");
    w("100"); w("AcDbSymbolTable"); w(" 70"); w(QString::number(m_layers.size()+1));
    // Default layer 0
    w("  0"); w("LAYER"); w("  5"); w("10"); w("100"); w("AcDbSymbolTableRecord");
    w("100"); w("AcDbLayerTableRecord"); w("  2"); w("0");
    w(" 70"); w("0"); w(" 62"); w("7"); w("  6"); w("Continuous"); w("370"); w("-3");

    int handleNum = 11;
    for (const QString &layer : m_layers) {
        if (layer == "0") continue;
        LayerInfo info = m_layerInfo.value(layer, LayerInfo());
        w("  0"); w("LAYER"); w("  5"); w(QString::number(handleNum++, 16).toUpper());
        w("100"); w("AcDbSymbolTableRecord"); w("100"); w("AcDbLayerTableRecord");
        w("  2"); w(layer); w(" 70"); w("0");
        w(" 62"); w(QString::number(info.color)); w("  6"); w(info.linetype);
        w("370"); w(QString::number(info.lineweight));
    }
    w("  0"); w("ENDTAB"); w("  0"); w("ENDSEC");

    // BLOCKS
    w("  0"); w("SECTION"); w("  2"); w("BLOCKS"); w("  0"); w("ENDSEC");

    // ENTITIES
    w("  0"); w("SECTION"); w("  2"); w("ENTITIES");
    int hc = 100000;
    auto nh = [&hc]() -> QString { return QString::number(hc++, 16).toUpper(); };

    for (const Line2D &line : m_lines) {
        if (line.points.size() == 2) {
            w("  0"); w("LINE"); w("  5"); w(nh()); w("330"); w("2E9");
            w("100"); w("AcDbEntity"); w("  8"); w(line.layerName); w("100"); w("AcDbLine");
            w(" 10"); w(QString::number(line.points[0].x,'f',6));
            w(" 20"); w(QString::number(line.points[0].y,'f',6)); w(" 30"); w("0.0");
            w(" 11"); w(QString::number(line.points[1].x,'f',6));
            w(" 21"); w(QString::number(line.points[1].y,'f',6)); w(" 31"); w("0.0");
        } else if (line.points.size() > 2) {
            w("  0"); w("LWPOLYLINE"); w("  5"); w(nh()); w("330"); w("2E9");
            w("100"); w("AcDbEntity"); w("  8"); w(line.layerName); w("100"); w("AcDbPolyline");
            w(" 90"); w(QString::number(line.points.size()));
            w(" 70"); w(line.isClosed() ? "1" : "0");
            for (const Point2D &pt : line.points) {
                w(" 10"); w(QString::number(pt.x,'f',6)); w(" 20"); w(QString::number(pt.y,'f',6));
            }
        }
    }

    for (const TextInfo &ti : m_texts) {
        w("  0"); w("TEXT"); w("  5"); w(nh()); w("330"); w("2E9");
        w("100"); w("AcDbEntity"); w("  8"); w(ti.layer); w("100"); w("AcDbText");
        w(" 10"); w(QString::number(ti.x,'f',6)); w(" 20"); w(QString::number(ti.y,'f',6)); w(" 30"); w("0.0");
        w(" 40"); w(QString::number(ti.height>0?ti.height:3.0,'f',2)); w("  1"); w(ti.text);
    }

    for (const Polygon2D &poly : m_hatches) {
        if (poly.exterior.size() < 3) continue;
        w("  0"); w("HATCH"); w("  5"); w(nh()); w("330"); w("2E9");
        w("100"); w("AcDbEntity"); w("  8"); w(poly.layerName); w("  6"); w("Continuous");
        w("100"); w("AcDbHatch");
        w(" 10"); w("0.0"); w(" 20"); w("0.0"); w(" 30"); w("0.0");
        w("210"); w("0.0"); w("220"); w("0.0"); w("230"); w("1.0");
        w("  2"); w("SOLID"); w(" 70"); w("1"); w(" 71"); w("0"); w(" 91"); w("1");
        w(" 92"); w("7"); w(" 72"); w("0"); w(" 73"); w("1");
        w(" 93"); w(QString::number(poly.exterior.size()));
        for (const Point2D &pt : poly.exterior) {
            w(" 10"); w(QString::number(pt.x,'f',6)); w(" 20"); w(QString::number(pt.y,'f',6));
        }
        w(" 97"); w("0");
    }

    w("  0"); w("ENDSEC");
    w("  0"); w("SECTION"); w("  2"); w("OBJECTS"); w("  0"); w("ENDSEC");
    w("  0"); w("EOF");

    file.close();
    return true;
}

DXFWrapper DXFWrapper::createCopy() {
    DXFWrapper c;
    c.m_filePath = m_filePath; c.m_loaded = m_loaded;
    c.m_lines = m_lines; c.m_texts = m_texts; c.m_hatches = m_hatches;
    c.m_layers = m_layers; c.m_layerInfo = m_layerInfo; c.m_originalHeader = m_originalHeader;
    return c;
}

QStringList DXFWrapper::getLayers() { return m_layers; }
bool DXFWrapper::hasLayer(const QString &n) { return m_layers.contains(n); }

void DXFWrapper::createLayer(const QString &n, int color) {
    if (!m_layers.contains(n)) {
        m_layers.append(n);
        LayerInfo i; i.name = n; i.color = color;
        m_layerInfo[n] = i;
    }
}

bool DXFWrapper::isLayerVisible(const QString &) { return true; }

QVector<Line2D> DXFWrapper::getLines(const QString &layer) {
    if (layer.isEmpty()) return m_lines;
    QVector<Line2D> r;
    for (const Line2D &l : m_lines) if (l.layerName == layer) r.append(l);
    return r;
}

QVector<DXFWrapper::TextInfo> DXFWrapper::getTexts(const QString &pat) {
    if (pat.isEmpty()) return m_texts;
    QVector<TextInfo> r;
    for (const TextInfo &t : m_texts) if (t.layer.contains(pat)) r.append(t);
    return r;
}

QVector<Polygon2D> DXFWrapper::getHatches(const QString &layer) {
    if (layer.isEmpty()) return m_hatches;
    QVector<Polygon2D> r;
    for (const Polygon2D &p : m_hatches) if (p.layerName == layer) r.append(p);
    return r;
}

void DXFWrapper::addLWPolyline(const QVector<Point2D> &pts, const QString &layer, int color) {
    if (pts.size() < 2) return;
    Line2D l; l.points = pts; l.layerName = layer; l.color = color;
    m_lines.append(l); createLayer(layer);
}

void DXFWrapper::addLine(const Point2D &s, const Point2D &e, const QString &layer, int color) {
    Line2D l; l.points.append(s); l.points.append(e); l.layerName = layer; l.color = color;
    m_lines.append(l); createLayer(layer);
}

void DXFWrapper::addHatch(const Polygon2D &poly, const QString &layer, const QString &, double, const QColor &) {
    if (poly.exterior.size() < 3) return;
    Polygon2D p = poly; p.layerName = layer;
    m_hatches.append(p); createLayer(layer);
}

void DXFWrapper::addMText(const QString &content, const Point2D &pos, double height, const QString &layer, const QColor &) {
    TextInfo t; t.text = content; t.x = pos.x; t.y = pos.y; t.layer = layer; t.height = height;
    m_texts.append(t); createLayer(layer);
}
