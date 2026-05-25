#include "DXFWrapper.h"
#include "utils/LineUtils.h"
#include "Config.h"
#include "Geometry.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <QStringDecoder>

// 简化版DXF读取 - 不依赖第三方库
// 直接解析DXF文本格式
// 支持: LINE, LWPOLYLINE, TEXT, MTEXT, HATCH

// 编码转换函数 - DXF文件编码处理
// 重要发现：部分DXF文件虽然声明ANSI_936（GBK），但实际内容是UTF-8编码
// 这是由于某些CAD软件导出时编码声明与实际编码不匹配
// 解决方案：自动检测UTF-8特征字节（0xE0-0xEF范围），优先使用UTF-8解码
static QString fromGBK(const QByteArray &bytes) {
    // 检测UTF-8特征：以0xE开头的字节通常表示UTF-8中文（E5/E6/E7）
    bool likelyUTF8 = false;
    for (int i = 0; i < bytes.size() && i < 10; ++i) {
        unsigned char b = static_cast<unsigned char>(bytes[i]);
        if (b >= 0xE0 && b <= 0xEF) {
            likelyUTF8 = true;
            break;
        }
    }

    if (likelyUTF8) {
        // UTF-8解码（适用于大多数现代CAD导出的DXF）
        static QStringDecoder utf8Decoder("UTF-8");
        if (utf8Decoder.isValid()) {
            return utf8Decoder.decode(bytes);
        }
    }

    // 默认使用GB18030（GBK超集，兼容ANSI_936）
    static QStringDecoder gbkDecoder("GB18030");
    if (gbkDecoder.isValid()) {
        return gbkDecoder.decode(bytes);
    }

    // 最后fallback
    return QString::fromLocal8Bit(bytes);
}

// 编码写入函数 - 使用UTF-8输出
// 中望CAD和AutoCAD 2018+都支持UTF-8编码的DXF
static QByteArray toGBK(const QString &str) {
    // UTF-8编码输出（声明ANSI_936以兼容中望CAD）
    static QStringEncoder utf8Encoder("UTF-8");
    if (utf8Encoder.isValid()) {
        return utf8Encoder.encode(str);
    }
    // Fallback to GB18030
    static QStringEncoder gbkEncoder("GB18030");
    if (gbkEncoder.isValid()) {
        return gbkEncoder.encode(str);
    }
    return str.toLocal8Bit();
}

bool DXFWrapper::read(const QString &filePath)
{
    m_filePath = filePath;
    m_loaded = false;
    m_lines.clear();
    m_texts.clear();
    m_hatches.clear();
    m_layers.clear();
    m_layerInfo.clear();
    m_originalHeader.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open DXF file:" << filePath;
        return false;
    }

    // 第一遍：读取原始HEADER部分（保留原始格式，不trimmed）
    // HEADER格式: 0 SECTION, 2 HEADER, ... , 0 ENDSEC
    bool foundHeader = false;
    while (!file.atEnd()) {
        QByteArray codeBytes = file.readLine();
        QByteArray valueBytes = file.readLine();
        // 保留原始格式，只去掉换行符
        QString code = fromGBK(codeBytes);
        QString value = fromGBK(valueBytes);
        // 去掉末尾的CR/LF但保留前导空格
        while (code.endsWith('\r') || code.endsWith('\n')) code.chop(1);
        while (value.endsWith('\r') || value.endsWith('\n')) value.chop(1);

        // 保存所有行到HEADER（保留原始格式）
        m_originalHeader.append(code);
        m_originalHeader.append(value);

        // 检测HEADER开始
        if (code.trimmed() == "0" && value.trimmed() == "SECTION") {
            // 下一个应该是 2 HEADER
        }
        if (code.trimmed() == "2" && value.trimmed() == "HEADER") {
            foundHeader = true;
        }
        // 检测HEADER结束
        if (foundHeader && code.trimmed() == "0" && value.trimmed() == "ENDSEC") {
            break;  // HEADER结束，停止读取
        }
    }

    qDebug() << "Original HEADER lines:" << m_originalHeader.size();

    // 重置文件指针，重新读取解析实体
    file.seek(0);

    // 直接读取字节流，逐行处理避免大内存转换
    QString currentSection = "";
    QString currentEntity = "";
    QString currentLayer = "";

    // LINE坐标
    double lineX1 = 0, lineY1 = 0, lineX2 = 0, lineY2 = 0;

    // LWPOLYLINE顶点
    QVector<Point2D> polyPts;
    double polyX = 0, polyY = 0;

    // TEXT/MTEXT
    QString textContent = "";
    double textX = 0, textY = 0;

    // HATCH边界
    QVector<Point2D> hatchBoundary;
    int hatchEdgeCount = 0;
    double hatchX = 0, hatchY = 0;

    // POLYLINE顶点
    QVector<Point2D> polylinePts;

    // 图层表解析状态
    LayerInfo currentLayerInfo;
    bool inTablesSection = false;
    bool inLayerTable = false;
    bool inLayerRecord = false;

    // 逐行读取（使用GBK编码，DXF ANSI_936）
    while (!file.atEnd()) {
        QByteArray lineBytes = file.readLine();
        QString code = fromGBK(lineBytes).trimmed();
        lineBytes = file.readLine();
        QString value = fromGBK(lineBytes).trimmed();
        int codeNum = code.toInt();

        // code 0: 实体或节段标记
        if (codeNum == 0) {
            // 保存上一个实体
            if (currentEntity == "LINE" && !currentLayer.isEmpty()) {
                Line2D line;
                line.points.append(Point2D(lineX1, lineY1));
                line.points.append(Point2D(lineX2, lineY2));
                line.layerName = currentLayer;
                m_lines.append(line);
            }
            else if (currentEntity == "LWPOLYLINE" && polyPts.size() >= 2) {
                Line2D poly;
                poly.points = polyPts;
                poly.layerName = currentLayer;
                m_lines.append(poly);
            }
            else if (currentEntity == "POLYLINE" && polylinePts.size() >= 2) {
                Line2D poly;
                poly.points = polylinePts;
                poly.layerName = currentLayer;
                m_lines.append(poly);
                polylinePts.clear();
            }
            else if ((currentEntity == "TEXT" || currentEntity == "MTEXT") && !textContent.isEmpty()) {
                TextInfo ti;
                ti.text = textContent;
                ti.x = textX;
                ti.y = textY;
                ti.layer = currentLayer;
                m_texts.append(ti);
            }
            else if (currentEntity == "HATCH" && hatchBoundary.size() >= 3) {
                Polygon2D hatchPoly(hatchBoundary);
                hatchPoly.layerName = currentLayer;
                m_hatches.append(hatchPoly);
            }

            // 处理特殊标记
            if (value == "SECTION") {
                currentSection = "";
                currentEntity = "";
                inTablesSection = false;
                inLayerTable = false;
                inLayerRecord = false;
            }
            else if (value == "ENDSEC") {
                if (inLayerRecord && !currentLayerInfo.name.isEmpty()) {
                    m_layerInfo[currentLayerInfo.name] = currentLayerInfo;
                    if (!m_layers.contains(currentLayerInfo.name)) {
                        m_layers.append(currentLayerInfo.name);
                    }
                }
                currentSection = "";
                currentEntity = "";
                inTablesSection = false;
                inLayerTable = false;
                inLayerRecord = false;
            }
            else if (value == "SEQEND") {
                if (polylinePts.size() >= 2) {
                    Line2D poly;
                    poly.points = polylinePts;
                    poly.layerName = currentLayer;
                    m_lines.append(poly);
                }
                polylinePts.clear();
                currentEntity = "";
            }
            else if (value == "TABLE") {
                // 进入某个表
                currentEntity = "TABLE";
            }
            else if (value == "ENDTAB") {
                // 表结束
                if (inLayerRecord && !currentLayerInfo.name.isEmpty()) {
                    m_layerInfo[currentLayerInfo.name] = currentLayerInfo;
                    if (!m_layers.contains(currentLayerInfo.name)) {
                        m_layers.append(currentLayerInfo.name);
                    }
                }
                currentEntity = "";
                inLayerTable = false;
                inLayerRecord = false;
            }
            else if (value == "LAYER") {
                // LAYER作为图层记录
                if (inLayerTable) {
                    if (inLayerRecord && !currentLayerInfo.name.isEmpty()) {
                        m_layerInfo[currentLayerInfo.name] = currentLayerInfo;
                        if (!m_layers.contains(currentLayerInfo.name)) {
                            m_layers.append(currentLayerInfo.name);
                        }
                    }
                    inLayerRecord = true;
                    currentLayerInfo = LayerInfo();
                }
            }
            else {
                currentEntity = value.toUpper();
                polyPts.clear();
                textContent = "";
                hatchBoundary.clear();
                hatchEdgeCount = 0;
            }
            continue;
        }

        // code 2: 节段名或表名
        if (codeNum == 2) {
            if (currentEntity.isEmpty() && currentSection.isEmpty()) {
                currentSection = value;
                if (value == "TABLES") {
                    inTablesSection = true;
                }
            }
            else if (currentEntity == "TABLE" && value == "LAYER") {
                // LAYER表开始
                inLayerTable = true;
            }
            else if (inLayerRecord) {
                // 图层记录的名称
                currentLayerInfo.name = value;
            }
            continue;
        }

        // code 8: 实体图层名
        if (codeNum == 8) {
            currentLayer = value;
            if (!m_layers.contains(currentLayer)) {
                m_layers.append(currentLayer);
            }
            continue;
        }

        // code 62: 颜色
        if (codeNum == 62) {
            if (inLayerRecord) {
                currentLayerInfo.color = value.toInt();
            }
            continue;
        }

        // code 6: 线型
        if (codeNum == 6) {
            if (inLayerRecord) {
                currentLayerInfo.linetype = value;
            }
            continue;
        }

        // code 370: 线宽
        if (codeNum == 370) {
            if (inLayerRecord) {
                currentLayerInfo.lineweight = value.toInt();
            }
            continue;
        }

        // 坐标处理
        if (codeNum == 10) {
            if (currentEntity == "LINE") lineX1 = value.toDouble();
            else if (currentEntity == "LWPOLYLINE") polyX = value.toDouble();
            else if (currentEntity == "POLYLINE" || currentEntity == "VERTEX") polyX = value.toDouble();
            else if (currentEntity == "TEXT" || currentEntity == "MTEXT") textX = value.toDouble();
            else if (currentEntity == "HATCH") hatchX = value.toDouble();
        }
        else if (codeNum == 20) {
            if (currentEntity == "LINE") lineY1 = value.toDouble();
            else if (currentEntity == "LWPOLYLINE") {
                polyY = value.toDouble();
                polyPts.append(Point2D(polyX, polyY));
            }
            else if (currentEntity == "VERTEX") {
                polyY = value.toDouble();
                polylinePts.append(Point2D(polyX, polyY));
            }
            else if (currentEntity == "TEXT" || currentEntity == "MTEXT") textY = value.toDouble();
            else if (currentEntity == "HATCH") {
                hatchY = value.toDouble();
                if (hatchEdgeCount > 0) {
                    hatchBoundary.append(Point2D(hatchX, hatchY));
                }
            }
        }
        else if (codeNum == 11) {
            if (currentEntity == "LINE") lineX2 = value.toDouble();
        }
        else if (codeNum == 21) {
            if (currentEntity == "LINE") lineY2 = value.toDouble();
        }
        else if (codeNum == 93) {
            if (currentEntity == "HATCH") {
                hatchEdgeCount = value.toInt();
                hatchBoundary.clear();
            }
        }
        else if (codeNum == 1) {
            if (currentEntity == "TEXT" || currentEntity == "MTEXT") {
                textContent = value;
            }
        }
        else if (codeNum == 3) {
            if (currentEntity == "MTEXT") {
                textContent += value;
            }
        }
    }

    // 处理最后一个实体
    if (currentEntity == "LWPOLYLINE" && polyPts.size() >= 2) {
        Line2D poly;
        poly.points = polyPts;
        poly.layerName = currentLayer;
        m_lines.append(poly);
    }
    else if (currentEntity == "POLYLINE" && polylinePts.size() >= 2) {
        Line2D poly;
        poly.points = polylinePts;
        poly.layerName = currentLayer;
        m_lines.append(poly);
    }
    else if ((currentEntity == "TEXT" || currentEntity == "MTEXT") && !textContent.isEmpty()) {
        TextInfo ti;
        ti.text = textContent;
        ti.x = textX;
        ti.y = textY;
        ti.layer = currentLayer;
        m_texts.append(ti);
    }
    else if (currentEntity == "HATCH" && hatchBoundary.size() >= 3) {
        Polygon2D hatchPoly(hatchBoundary);
        hatchPoly.layerName = currentLayer;
        m_hatches.append(hatchPoly);
    }

    file.close();
    m_loaded = true;

    qDebug() << "DXF loaded:" << filePath
             << "lines:" << m_lines.size()
             << "texts:" << m_texts.size()
             << "hatches:" << m_hatches.size()
             << "layers:" << m_layers.size()
             << "layerInfo:" << m_layerInfo.size();

    return true;
}

bool DXFWrapper::save(const QString &filePath)
{
    QString outputPath = filePath.isEmpty() ? m_filePath : filePath;

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot save DXF file:" << outputPath;
        return false;
    }

    // DXF使用GBK编码（ANSI_936）
    auto writeLine = [&file](const QString &str) {
        file.write(toGBK(str));
        file.write("\r\n");
    };

    // ========== 使用原始HEADER（完整复制） ==========

    // 如果有保存的原始HEADER，直接输出
    if (m_originalHeader.size() > 0) {
        for (const QString &line : m_originalHeader) {
            writeLine(line);
        }
    } else {
        // 没有原始HEADER时，生成基本HEADER
        writeLine("  0");
        writeLine("SECTION");
        writeLine("  2");
        writeLine("HEADER");
        writeLine("  9");
        writeLine("$ACADVER");
        writeLine("  1");
        writeLine("AC1032");
        writeLine("  9");
        writeLine("$DWGCODEPAGE");
        writeLine("  3");
        writeLine("ANSI_936");
        writeLine("  0");
        writeLine("ENDSEC");
    }

    // TABLES节段
    writeLine("  0");
    writeLine("SECTION");
    writeLine("  2");
    writeLine("TABLES");

    // VPORT表（必须存在）
    writeLine("  0");
    writeLine("TABLE");
    writeLine("  2");
    writeLine("VPORT");
    writeLine("  5");
    writeLine("8");
    writeLine("330");
    writeLine("0");
    writeLine("100");
    writeLine("AcDbSymbolTable");
    writeLine(" 70");
    writeLine("1");
    writeLine("  0");
    writeLine("VPORT");
    writeLine("  5");
    writeLine("EA");
    writeLine("330");
    writeLine("8");
    writeLine("100");
    writeLine("AcDbSymbolTableRecord");
    writeLine("100");
    writeLine("AcDbViewportTableRecord");
    writeLine("  2");
    writeLine("*Active");
    writeLine(" 70");
    writeLine("0");
    writeLine(" 10");
    writeLine("0.0");
    writeLine(" 20");
    writeLine("0.0");
    writeLine(" 11");
    writeLine("1.0");
    writeLine(" 21");
    writeLine("1.0");
    writeLine(" 12");
    writeLine("0.0");
    writeLine(" 22");
    writeLine("0.0");
    writeLine(" 13");
    writeLine("0.0");
    writeLine(" 23");
    writeLine("0.0");
    writeLine(" 14");
    writeLine("10.0");
    writeLine(" 24");
    writeLine("10.0");
    writeLine(" 15");
    writeLine("10.0");
    writeLine(" 25");
    writeLine("10.0");
    writeLine(" 16");
    writeLine("0.0");
    writeLine(" 26");
    writeLine("0.0");
    writeLine(" 36");
    writeLine("1.0");
    writeLine(" 17");
    writeLine("0.0");
    writeLine(" 27");
    writeLine("0.0");
    writeLine(" 37");
    writeLine("0.0");
    writeLine(" 40");
    writeLine("1000.0");
    writeLine(" 41");
    writeLine("1.0");
    writeLine(" 42");
    writeLine("50.0");
    writeLine(" 43");
    writeLine("0.0");
    writeLine(" 44");
    writeLine("0.0");
    writeLine(" 50");
    writeLine("0.0");
    writeLine(" 51");
    writeLine("0.0");
    writeLine(" 71");
    writeLine("0");
    writeLine(" 72");
    writeLine("1000");
    writeLine(" 73");
    writeLine("1");
    writeLine(" 74");
    writeLine("3");
    writeLine(" 75");
    writeLine("0");
    writeLine(" 76");
    writeLine("0");
    writeLine(" 77");
    writeLine("0");
    writeLine(" 78");
    writeLine("0");
    writeLine("281");
    writeLine("0");
    writeLine(" 65");
    writeLine("1");
    writeLine("  0");
    writeLine("ENDTAB");

    // LTYPE表（必须存在）
    writeLine("  0");
    writeLine("TABLE");
    writeLine("  2");
    writeLine("LTYPE");
    writeLine("  5");
    writeLine("5");
    writeLine("330");
    writeLine("0");
    writeLine("100");
    writeLine("AcDbSymbolTable");
    writeLine(" 70");
    writeLine("3");
    // ByBlock
    writeLine("  0");
    writeLine("LTYPE");
    writeLine("  5");
    writeLine("14");
    writeLine("330");
    writeLine("5");
    writeLine("100");
    writeLine("AcDbSymbolTableRecord");
    writeLine("100");
    writeLine("AcDbLinetypeTableRecord");
    writeLine("  2");
    writeLine("ByBlock");
    writeLine(" 70");
    writeLine("0");
    writeLine("  3");
    writeLine("");
    writeLine(" 72");
    writeLine("65");
    writeLine(" 73");
    writeLine("0");
    writeLine(" 40");
    writeLine("0.0");
    // ByLayer
    writeLine("  0");
    writeLine("LTYPE");
    writeLine("  5");
    writeLine("15");
    writeLine("330");
    writeLine("5");
    writeLine("100");
    writeLine("AcDbSymbolTableRecord");
    writeLine("100");
    writeLine("AcDbLinetypeTableRecord");
    writeLine("  2");
    writeLine("ByLayer");
    writeLine(" 70");
    writeLine("0");
    writeLine("  3");
    writeLine("");
    writeLine(" 72");
    writeLine("65");
    writeLine(" 73");
    writeLine("0");
    writeLine(" 40");
    writeLine("0.0");
    // Continuous
    writeLine("  0");
    writeLine("LTYPE");
    writeLine("  5");
    writeLine("16");
    writeLine("330");
    writeLine("5");
    writeLine("100");
    writeLine("AcDbSymbolTableRecord");
    writeLine("100");
    writeLine("AcDbLinetypeTableRecord");
    writeLine("  2");
    writeLine("Continuous");
    writeLine(" 70");
    writeLine("0");
    writeLine("  3");
    writeLine("Solid line");
    writeLine(" 72");
    writeLine("65");
    writeLine(" 73");
    writeLine("0");
    writeLine(" 40");
    writeLine("0.0");
    writeLine("  0");
    writeLine("ENDTAB");

    // LAYER表
    writeLine("  0");
    writeLine("TABLE");
    writeLine("  2");
    writeLine("LAYER");
    writeLine("  5");
    writeLine("2");
    writeLine("330");
    writeLine("0");
    writeLine("100");
    writeLine("AcDbSymbolTable");
    writeLine(" 70");
    writeLine(QString::number(m_layers.size() + 1));

    // 默认图层0
    writeLine("  0");
    writeLine("LAYER");
    writeLine("  5");
    writeLine("10");
    writeLine("100");
    writeLine("AcDbSymbolTableRecord");
    writeLine("100");
    writeLine("AcDbLayerTableRecord");
    writeLine("  2");
    writeLine("0");
    writeLine(" 70");
    writeLine("0");
    writeLine(" 62");
    writeLine("7");
    writeLine("  6");
    writeLine("Continuous");
    writeLine("370");
    writeLine("-3");

    // 用户图层（保留原始属性）
    int handleNum = 11;
    for (const QString &layer : m_layers) {
        if (layer == "0") continue;

        LayerInfo info = m_layerInfo.value(layer, LayerInfo());

        writeLine("  0");
        writeLine("LAYER");
        writeLine("  5");
        writeLine(QString::number(handleNum++, 16).toUpper());
        writeLine("100");
        writeLine("AcDbSymbolTableRecord");
        writeLine("100");
        writeLine("AcDbLayerTableRecord");
        writeLine("  2");
        writeLine(layer);
        writeLine(" 70");
        writeLine("0");
        writeLine(" 62");
        writeLine(QString::number(info.color));
        writeLine("  6");
        writeLine(info.linetype);
        writeLine("370");
        writeLine(QString::number(info.lineweight));
    }
    writeLine("  0");
    writeLine("ENDTAB");
    writeLine("  0");
    writeLine("ENDSEC");

    // BLOCKS节段
    writeLine("  0");
    writeLine("SECTION");
    writeLine("  2");
    writeLine("BLOCKS");
    writeLine("  0");
    writeLine("ENDSEC");

    // ENTITIES节段
    writeLine("  0");
    writeLine("SECTION");
    writeLine("  2");
    writeLine("ENTITIES");

    int handleCounter = 100000;
    auto nextHandle = [&handleCounter]() -> QString {
        return QString::number(handleCounter++, 16).toUpper();
    };

    // 写所有线段
    for (const Line2D &line : m_lines) {
        if (line.points.size() == 2) {
            writeLine("  0");
            writeLine("LINE");
            writeLine("  5");
            writeLine(nextHandle());
            writeLine("330");
            writeLine("2E9");
            writeLine("100");
            writeLine("AcDbEntity");
            writeLine("  8");
            writeLine(line.layerName);
            writeLine("100");
            writeLine("AcDbLine");
            writeLine(" 10");
            writeLine(QString::number(line.points[0].x, 'f', 6));
            writeLine(" 20");
            writeLine(QString::number(line.points[0].y, 'f', 6));
            writeLine(" 30");
            writeLine("0.0");
            writeLine(" 11");
            writeLine(QString::number(line.points[1].x, 'f', 6));
            writeLine(" 21");
            writeLine(QString::number(line.points[1].y, 'f', 6));
            writeLine(" 31");
            writeLine("0.0");
        }
        else if (line.points.size() > 2) {
            writeLine("  0");
            writeLine("LWPOLYLINE");
            writeLine("  5");
            writeLine(nextHandle());
            writeLine("330");
            writeLine("2E9");
            writeLine("100");
            writeLine("AcDbEntity");
            writeLine("  8");
            writeLine(line.layerName);
            writeLine("100");
            writeLine("AcDbPolyline");
            writeLine(" 90");
            writeLine(QString::number(line.points.size()));
            writeLine(" 70");
            writeLine(line.isClosed() ? "1" : "0");
            for (const Point2D &pt : line.points) {
                writeLine(" 10");
                writeLine(QString::number(pt.x, 'f', 6));
                writeLine(" 20");
                writeLine(QString::number(pt.y, 'f', 6));
            }
        }
    }

    // 写所有文本
    for (const TextInfo &ti : m_texts) {
        writeLine("  0");
        writeLine("TEXT");
        writeLine("  5");
        writeLine(nextHandle());
        writeLine("330");
        writeLine("2E9");
        writeLine("100");
        writeLine("AcDbEntity");
        writeLine("  8");
        writeLine(ti.layer);
        writeLine("100");
        writeLine("AcDbText");
        writeLine(" 10");
        writeLine(QString::number(ti.x, 'f', 6));
        writeLine(" 20");
        writeLine(QString::number(ti.y, 'f', 6));
        writeLine(" 30");
        writeLine("0.0");
        writeLine(" 40");
        writeLine(QString::number(ti.height > 0 ? ti.height : 3.0, 'f', 2));
        writeLine("  1");
        writeLine(ti.text);
    }

    // 写所有HATCH
    for (const Polygon2D &poly : m_hatches) {
        if (poly.exterior.size() < 3) continue;

        writeLine("  0");
        writeLine("HATCH");
        writeLine("  5");
        writeLine(nextHandle());
        writeLine("330");
        writeLine("2E9");
        writeLine("100");
        writeLine("AcDbEntity");
        writeLine("  8");
        writeLine(poly.layerName);
        writeLine("  6");
        writeLine("Continuous");
        writeLine("100");
        writeLine("AcDbHatch");
        writeLine(" 10");
        writeLine("0.0");
        writeLine(" 20");
        writeLine("0.0");
        writeLine(" 30");
        writeLine("0.0");
        writeLine("210");
        writeLine("0.0");
        writeLine("220");
        writeLine("0.0");
        writeLine("230");
        writeLine("1.0");
        writeLine("  2");
        writeLine("SOLID");
        writeLine(" 70");
        writeLine("1");
        writeLine(" 71");
        writeLine("0");
        writeLine(" 91");
        writeLine("1");
        writeLine(" 92");
        writeLine("7");
        writeLine(" 72");
        writeLine("0");
        writeLine(" 73");
        writeLine("1");
        writeLine(" 93");
        writeLine(QString::number(poly.exterior.size()));
        for (const Point2D &pt : poly.exterior) {
            writeLine(" 10");
            writeLine(QString::number(pt.x, 'f', 6));
            writeLine(" 20");
            writeLine(QString::number(pt.y, 'f', 6));
        }
        writeLine(" 97");
        writeLine("0");
    }

    writeLine("  0");
    writeLine("ENDSEC");

    // OBJECTS节段
    writeLine("  0");
    writeLine("SECTION");
    writeLine("  2");
    writeLine("OBJECTS");
    writeLine("  0");
    writeLine("ENDSEC");

    writeLine("  0");
    writeLine("EOF");

    file.close();
    qDebug() << "DXF saved:" << outputPath
             << "lines:" << m_lines.size()
             << "texts:" << m_texts.size()
             << "hatches:" << m_hatches.size();
    return true;
}

DXFWrapper DXFWrapper::createCopy()
{
    DXFWrapper copy;
    copy.m_filePath = m_filePath;
    copy.m_loaded = m_loaded;
    copy.m_lines = m_lines;
    copy.m_texts = m_texts;
    copy.m_hatches = m_hatches;
    copy.m_layers = m_layers;
    copy.m_layerInfo = m_layerInfo;
    copy.m_originalHeader = m_originalHeader;
    return copy;
}

QStringList DXFWrapper::getLayers()
{
    return m_layers;
}

bool DXFWrapper::hasLayer(const QString &layerName)
{
    return m_layers.contains(layerName);
}

void DXFWrapper::createLayer(const QString &layerName, int color)
{
    if (!m_layers.contains(layerName)) {
        m_layers.append(layerName);
        LayerInfo info;
        info.name = layerName;
        info.color = color;
        m_layerInfo[layerName] = info;
    }
}

bool DXFWrapper::isLayerVisible(const QString &layerName)
{
    return true;
}

QVector<Line2D> DXFWrapper::getLines(const QString &layerName)
{
    if (layerName.isEmpty()) {
        return m_lines;
    }

    QVector<Line2D> result;
    for (const Line2D &line : m_lines) {
        if (line.layerName == layerName) {
            result.append(line);
        }
    }
    return result;
}

QVector<DXFWrapper::TextInfo> DXFWrapper::getTexts(const QString &layerPattern)
{
    if (layerPattern.isEmpty()) {
        return m_texts;
    }

    QVector<TextInfo> result;
    for (const TextInfo &ti : m_texts) {
        if (ti.layer.contains(layerPattern)) {
            result.append(ti);
        }
    }
    return result;
}

QVector<Polygon2D> DXFWrapper::getHatches(const QString &layerName)
{
    if (layerName.isEmpty()) {
        return m_hatches;
    }

    QVector<Polygon2D> result;
    for (const Polygon2D &poly : m_hatches) {
        if (poly.layerName == layerName) {
            result.append(poly);
        }
    }
    return result;
}

void DXFWrapper::addLWPolyline(const QVector<Point2D> &points,
                                const QString &layerName,
                                int color)
{
    if (points.size() < 2) return;

    Line2D line;
    line.points = points;
    line.layerName = layerName;
    line.color = color;
    m_lines.append(line);

    createLayer(layerName);
}

void DXFWrapper::addLine(const Point2D &start, const Point2D &end,
                          const QString &layerName,
                          int color)
{
    Line2D line;
    line.points.append(start);
    line.points.append(end);
    line.layerName = layerName;
    line.color = color;
    m_lines.append(line);

    createLayer(layerName);
}

void DXFWrapper::addHatch(const Polygon2D &polygon,
                           const QString &layerName,
                           const QString &pattern,
                           double scale,
                           const QColor &rgbColor)
{
    if (polygon.exterior.size() < 3) return;

    Polygon2D poly = polygon;
    poly.layerName = layerName;
    m_hatches.append(poly);

    createLayer(layerName);
}

void DXFWrapper::addMText(const QString &content,
                           const Point2D &position,
                           double height,
                           const QString &layerName,
                           const QColor &rgbColor)
{
    TextInfo ti;
    ti.text = content;
    ti.x = position.x;
    ti.y = position.y;
    ti.layer = layerName;
    ti.height = height;
    m_texts.append(ti);

    createLayer(layerName);
}