#include "EngineCad.h"
#include "PythonBridge.h"
#include "Config.h"
#include "Geometry.h"
#include "LineUtils.h"
#include "EnvelopeGenerator.h"
#include "StationMatcher.h"
#include "VirtualBoxBuilder.h"
#include "DXFWrapper.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>
#include <QDateTime>
#include <QJsonDocument>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSaveFile>
#include <QHash>
#include <cmath>
#include <algorithm>
#include <limits>

// ==================== Static string constants ====================

static const QString kInfo    = QStringLiteral("info");
static const QString kError   = QStringLiteral("error");
static const QString kWarn    = QStringLiteral("warning");
static const QString kSuccess = QStringLiteral("success");

// ==================== Inlined helpers ====================

static QString compactTimestamp() {
    return QDateTime::currentDateTime().toString(QStringLiteral("HHmm"));
}

static QStringList detectStrataLayers(const QStringList &allLayers) {
    QStringList strataLayers;
    // The grade marker is GBK/ANSI in many user DXFs.  Identify strata using
    // stable numeric/Nonem prefixes plus the material Unicode codepoints, so
    // title-block layers such as 0-桩号 never enter the quantity table.
    const QRegularExpression numericPrefix(
        QStringLiteral("^(?:Nonem_)?\\d+"));
    for (const QString &layer : allLayers) {
        const bool hasMaterial =
            layer.contains(QChar(0x6DE4)) ||  // 淤
            layer.contains(QChar(0x586B)) ||  // 填
            layer.contains(QChar(0x9ECF)) ||  // 黏
            layer.contains(QChar(0x7802)) ||  // 砂
            layer.contains(QChar(0x788E));    // 碎
        if (numericPrefix.match(layer).hasMatch() && hasMaterial)
            strataLayers.append(layer);
    }
    if (!strataLayers.isEmpty()) {
        std::stable_sort(strataLayers.begin(), strataLayers.end(),
                         [](const QString &a, const QString &b) {
                      QRegularExpression numRe(QStringLiteral("^\\d+"));
                      return numRe.match(a).captured().toInt() <
                             numRe.match(b).captured().toInt();
                  });
        return strataLayers;
    }
    QRegularExpression re(QStringLiteral("^\\d+级"));
    for (const QString &layer : allLayers) {
        if (re.match(layer).hasMatch())
            strataLayers.append(layer);
    }
    std::stable_sort(strataLayers.begin(), strataLayers.end(),
                     [](const QString &a, const QString &b) {
                  QRegularExpression numRe(QStringLiteral("^\\d+"));
                  int numA = numRe.match(a).hasMatch() ? numRe.match(a).captured().toInt() : 999;
                  int numB = numRe.match(b).hasMatch() ? numRe.match(b).captured().toInt() : 999;
                  return numA < numB;
              });
    return strataLayers;
}

static QString getOutputPathStatic(const QString &inputPath, const QString &suffix,
                                   const QString &outputDir = QString()) {
    QString baseDir = outputDir.isEmpty() ? QFileInfo(inputPath).absolutePath() : outputDir;
    QString baseName = QFileInfo(inputPath).completeBaseName();
    QString cleanSuffix = suffix;
    while (cleanSuffix.startsWith('_') || cleanSuffix.startsWith('-'))
        cleanSuffix.remove(0, 1);
    if (cleanSuffix.endsWith(".dxf") || cleanSuffix.endsWith(".xlsx"))
        return QStringLiteral("%1/%2_%3").arg(baseDir, baseName, cleanSuffix);
    return QStringLiteral("%1/%2_%3.dxf").arg(baseDir, baseName, cleanSuffix);
}

static QString buildBackfillOutputNameStatic(const QString &baseName, const QString &timestamp) {
    return QStringLiteral("%1_回淤_%2.dxf").arg(baseName, timestamp);
}

static QString extractEmbeddedScript(const QString &resourcePath, const QString &fileName) {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList outputDirs = {
        QDir(appDir).filePath(QStringLiteral("scripts")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("HydraulicCADPlatform/scripts"))
    };

    QFile source(resourcePath);
    if (!source.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray content = source.readAll();

    for (const QString &outputDir : outputDirs) {
        if (!QDir().mkpath(outputDir))
            continue;
        const QString outputPath = QDir(outputDir).filePath(fileName);
        QSaveFile output(outputPath);
        if (!output.open(QIODevice::WriteOnly))
            continue;
        if (output.write(content) == content.size() && output.commit())
            return outputPath;
    }
    return QString();
}

static QString locateExcelMigrationScript() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("scripts/excel_migrate.py")),
        QDir(appDir).filePath(QStringLiteral("../scripts/excel_migrate.py"))
    };
    for (const QString &candidate : candidates) {
        const QString cleanPath = QDir::cleanPath(candidate);
        if (QFileInfo::exists(cleanPath))
            return cleanPath;
    }
    return extractEmbeddedScript(
        QStringLiteral(":/scripts/excel_migrate.py"),
        QStringLiteral("excel_migrate.py"));
}

static QString locateGeologyTopviewScript() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("scripts/geology_topview_v41.py")),
        QDir(appDir).filePath(QStringLiteral("../scripts/geology_topview_v41.py"))
    };
    for (const QString &candidate : candidates) {
        const QString cleanPath = QDir::cleanPath(candidate);
        const QString implementationPath = QDir(
            QFileInfo(cleanPath).absolutePath()
        ).filePath(QStringLiteral(
            "geology_topview_section_connect_debug.py"));
        if (QFileInfo::exists(cleanPath) &&
            QFileInfo::exists(implementationPath))
            return cleanPath;
    }
    const QString implementationPath = extractEmbeddedScript(
        QStringLiteral(
            ":/scripts/geology_topview_section_connect_debug.py"),
        QStringLiteral(
            "geology_topview_section_connect_debug.py"));
    if (implementationPath.isEmpty())
        return QString();
    return extractEmbeddedScript(
        QStringLiteral(":/scripts/geology_topview_v41.py"),
        QStringLiteral("geology_topview_v41.py"));
}

static QString locateAutopasteApplyScript() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("scripts/autopaste_apply.py")),
        QDir(appDir).filePath(QStringLiteral("../scripts/autopaste_apply.py"))
    };
    for (const QString &candidate : candidates) {
        const QString cleanPath = QDir::cleanPath(candidate);
        if (QFileInfo::exists(cleanPath))
            return cleanPath;
    }
    return extractEmbeddedScript(
        QStringLiteral(":/scripts/autopaste_apply.py"),
        QStringLiteral("autopaste_apply.py"));
}

static QString locatePythonExecutable() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("runtime/python/python.exe")),
        QDir(appDir).filePath(QStringLiteral("python/python.exe"))
    };
    for (const QString &candidate : candidates) {
        const QString cleanPath = QDir::cleanPath(candidate);
        if (QFileInfo::exists(cleanPath))
            return cleanPath;
    }
    return QStringLiteral("python");
}

// ==================== Layer matching helpers ====================

QVector<DXFWrapper::TextInfo> EngineCad::matchTextsByLayer(
    const QVector<DXFWrapper::TextInfo> &allTexts,
    const QString &exactLayer)
{
    QVector<DXFWrapper::TextInfo> result;
    for (const DXFWrapper::TextInfo &ti : allTexts) {
        if (ti.layer.contains(exactLayer)) {
            result.append(ti);
        }
    }
    return result;
}

// ==================== Shared autosection/backfill helpers ====================

bool EngineCad::loadSectionData(const QString &filePath, const QString &dmxLayer,
                                 const QString &pileLayer, double targetElevation,
                                 LogCallback log, SectionData &data)
{
    data.filePath = filePath;

    if (!data.dxf.read(filePath)) {
        log(QStringLiteral("[ERROR] DXF文件读取失败"), kError);
        return false;
    }

    data.allLayers = data.dxf.getLayers();
    data.strataLayers = detectStrataLayers(data.allLayers);
    log(QStringLiteral("[INFO] 地层图层: %1个").arg(data.strataLayers.size()), kInfo);

    data.allLines = data.dxf.getLines(QStringLiteral(""));

    data.dmxList = getEntityList(data.dxf, dmxLayer);

    std::sort(data.dmxList.begin(), data.dmxList.end(),
              [](const EntityListData &a, const EntityListData &b) { return a.yCenter > b.yCenter; });
    log(QStringLiteral("[INFO] DMX数量: %1").arg(data.dmxList.size()), kInfo);

    data.excavLines = data.dxf.getLines(QStringLiteral("开挖线"));
    data.overexcLines = data.dxf.getLines(QStringLiteral("超挖线"));

    if (!data.excavLines.isEmpty()) {
        double minY = std::numeric_limits<double>::max(), maxY = std::numeric_limits<double>::min();
        int deep = 0;
        for (const Line2D &l : data.excavLines) {
            minY = std::min(minY, l.minY()); maxY = std::max(maxY, l.maxY());
            if (l.minY() < 2000) deep++;
        }
        log(QStringLiteral("[INFO] 开挖线Y范围: %1 - %2, 深线数=%3")
            .arg(minY, 0, 'f', 2).arg(maxY, 0, 'f', 2).arg(deep), kInfo);
    }

    if (!pileLayer.isEmpty()) {
        QVector<DXFWrapper::TextInfo> allTexts = data.dxf.getTexts(QStringLiteral(""));
        QVector<DXFWrapper::TextInfo> stationTexts = matchTextsByLayer(allTexts, pileLayer);
        log(QStringLiteral("[INFO] 桩号文本数: %1").arg(stationTexts.size()), kInfo);

        for (const DXFWrapper::TextInfo &ti : stationTexts) {
            int value = Config::parseStation(ti.text);
            if (value >= 0)
                data.stations.append(StationMatcher::StationInfo(ti.text, value, ti.x, ti.y));
        }
        data.sortedStations = StationMatcher::sortStationsByY(data.stations);
        log(QStringLiteral("[INFO] 有效桩号数: %1").arg(data.sortedStations.size()), kInfo);
    }

    return true;
}

QJsonObject EngineCad::buildSectionJson(const SectionData &data, const QString &station,
                                         int idx, const Box2D &boundaryBox,
                                         const QVector<Line2D> *auxLines,
                                         const QVector<Line2D> *updateLines)
{
    QJsonObject sectionObj;
    sectionObj[QStringLiteral("station")] = station;
    sectionObj[QStringLiteral("dmx_points")] = PythonBridge::pointsToJsonArray(data.dmxList[idx].line.points);

    if (auxLines) {
        QVector<Line2D> localAux = VirtualBoxBuilder::filterLinesInBox(*auxLines, boundaryBox);
        QJsonArray auxArr;
        for (const Line2D &al : localAux) auxArr.append(PythonBridge::pointsToJsonArray(al.points));
        sectionObj[QStringLiteral("aux_lines")] = auxArr;
    }

    if (updateLines) {
        QVector<Line2D> localUpdate;
        for (const Line2D &ul : *updateLines) {
            // The updated survey line may have a visible gap from the design
            // DMX, so box intersection is not a reliable section matcher.
            // Assign it to the nearest design-section center instead.
            const double updateX = ul.midX();
            const double updateY = ul.midY();
            int nearestIndex = -1;
            double nearestDistance2 = std::numeric_limits<double>::max();
            for (int candidate = 0; candidate < data.dmxList.size(); ++candidate) {
                const double dx = updateX - data.dmxList[candidate].xCenter;
                const double dy = updateY - data.dmxList[candidate].yCenter;
                const double distance2 = dx * dx + dy * dy;
                if (distance2 < nearestDistance2) {
                    nearestDistance2 = distance2;
                    nearestIndex = candidate;
                }
            }
            if (nearestIndex == idx)
                localUpdate.append(ul);
        }
        QJsonArray updateArr;
        for (const Line2D &ul : localUpdate) updateArr.append(PythonBridge::pointsToJsonArray(ul.points));
        sectionObj[QStringLiteral("update_lines")] = updateArr;
    }

    return sectionObj;
}

QJsonArray EngineCad::buildExcavJson(const QVector<Line2D> &excavLines) {
    QJsonArray arr;
    for (const Line2D &line : excavLines) arr.append(PythonBridge::lineToJson(line));
    return arr;
}

// ==================== Existing helpers ====================

EngineCad::EngineCad() {}

QVector<EngineCad::EntityListData> EngineCad::getEntityList(DXFWrapper &dxf, const QString &layer) {
    QVector<EntityListData> result;
    for (const Line2D &line : dxf.getLines(layer)) {
        if (line.points.isEmpty()) continue;
        EntityListData d;
        d.line = line;
        d.xMin = line.minX(); d.xMax = line.maxX();
        d.yMin = line.minY(); d.yMax = line.maxY();
        d.xCenter = line.midX(); d.yCenter = line.midY();
        result.append(d);
    }
    return result;
}

// ==================== Unified section task ====================

bool EngineCad::runSectionTask(SectionTaskMode mode, const QMap<QString, QString> &params,
                                LogCallback log, QJsonObject &result)
{
    QString elevationStr = params.value(QStringLiteral("目标高程")).trimmed();
    QString sectionLayer = params.value(QStringLiteral("断面线图层"), QStringLiteral("DMX"));
    QString pileLayer = params.value(QStringLiteral("桩号图层"), QStringLiteral("0-桩号"));
    QString auxLayersStr = params.value(QStringLiteral("辅助断面图层"));
    QString calcMode = params.value(QStringLiteral("计算模式"), QStringLiteral("below"));
    QString outputDir = params.value(QStringLiteral("输出目录"));
    QString filePath = params.value(QStringLiteral("files"));
    bool mergeSection = params.value(QStringLiteral("合并断面线")) == QStringLiteral("true");
    bool distinguishDesign = params.value(QStringLiteral("区分设计超挖")) == QStringLiteral("true");
    bool extendOverexc = params.value(QStringLiteral("延长超挖线")) == QStringLiteral("true");
    const bool fullCalculation = elevationStr.isEmpty();
    double targetElevation = fullCalculation ? 0.0 : elevationStr.toDouble();

    QString updateLayer;
    if (mode == SectionTaskMode::Combined) {
        updateLayer = params.value(QStringLiteral("更新断面线图层"));
        if (updateLayer.isEmpty()) {
            log(QStringLiteral("[ERROR] 请指定更新断面线图层名称"), kError);
            return false;
        }
        sectionLayer = params.value(QStringLiteral("设计断面线图层"), QStringLiteral("DMX"));
    }

    if (filePath.isEmpty()) {
        log(QStringLiteral("[WARN] 请先选择DXF文件"), kWarn);
        return false;
    }

    if (mode == SectionTaskMode::Combined)
        log(QStringLiteral("[INFO] 合并任务：分层算量 + 回淤计算"), kInfo);
    if (fullCalculation)
        log(QStringLiteral("[INFO] 全算量, 图层: %1").arg(sectionLayer), kInfo);
    else
        log(QStringLiteral("[INFO] 目标高程: %1m, 图层: %2").arg(targetElevation).arg(sectionLayer), kInfo);

    SectionData data;
    if (!loadSectionData(filePath, sectionLayer, pileLayer, targetElevation, log, data))
        return false;

    // Aux lines (autosection only)
    QVector<Line2D> auxLines;
    if (mode == SectionTaskMode::Autosection) {
        for (const QString &layer : auxLayersStr.split(',', Qt::SkipEmptyParts))
            auxLines.append(data.dxf.getLines(layer.trimmed()));
    }

    // Update lines (combined only)
    QVector<Line2D> updateLines;
    if (mode == SectionTaskMode::Combined) {
        for (const Line2D &line : data.allLines) {
            if (line.layerName == updateLayer) { updateLines.append(line); continue; }
        }
        if (updateLines.isEmpty()) {
            // A pasted survey layer is commonly an 8-digit date.  Users can
            // easily type an adjacent date (for example 20280803 instead of
            // 20260803); do not silently turn every backfill area into zero.
            // Only fall back to a numeric layer whose curve count matches the
            // DMX section count, so a date-like annotation layer is not used.
            const QRegularExpression dateLayerRe(QStringLiteral("^\\d{8}$"));
            QHash<QString, QVector<Line2D>> datedLayerLines;
            for (const Line2D &line : data.allLines) {
                if (dateLayerRe.match(line.layerName).hasMatch())
                    datedLayerLines[line.layerName].append(line);
            }
            QString fallbackLayer;
            int bestDistance = std::numeric_limits<int>::max();
            const int expectedCount = data.dmxList.size();
            for (auto it = datedLayerLines.constBegin(); it != datedLayerLines.constEnd(); ++it) {
                const int distance = std::abs(it.value().size() - expectedCount);
                if (distance < bestDistance ||
                    (distance == bestDistance && it.key() > fallbackLayer)) {
                    bestDistance = distance;
                    fallbackLayer = it.key();
                }
            }
            if (!fallbackLayer.isEmpty() &&
                bestDistance <= std::max(3, expectedCount / 10)) {
                updateLines = datedLayerLines.value(fallbackLayer);
                log(QStringLiteral("[WARN] 更新断面线图层“%1”未找到，已自动使用匹配断面数的日期图层“%2”（%3条）")
                        .arg(updateLayer, fallbackLayer)
                        .arg(updateLines.size()), kWarn);
            }
        }
        if (updateLines.isEmpty()) {
            for (const Line2D &line : data.allLines) {
                bool hasYi = false, hasZhan = false, hasTie = false;
                bool hasDuan = false, hasMian = false;
                for (int i = 0; i < line.layerName.length(); ++i) {
                    ushort code = line.layerName.at(i).unicode();
                    if (code == 0x5DF2) hasYi = true;
                    if (code == 0x7C98) hasZhan = true;
                    if (code == 0x8D34) hasTie = true;
                    if (code == 0x65AD) hasDuan = true;
                    if (code == 0x9762) hasMian = true;
                }
                if ((hasYi && hasZhan && hasTie) || (hasDuan && hasMian)) {
                    updateLines.append(line);
                }
            }
        }
        log(QStringLiteral("[INFO] 更新断面线: %1条").arg(updateLines.size()), kInfo);
    }

    // Build JSON
    QJsonObject jsonData;
    QString taskType;
    QString modeSuffix;
    switch (mode) {
    case SectionTaskMode::Autosection:
        taskType = QStringLiteral("autosection");
        modeSuffix = (calcMode == QStringLiteral("below")) ? QStringLiteral("以下面积") : QStringLiteral("以上面积");
        break;
    case SectionTaskMode::Combined:
        taskType = QStringLiteral("autosection_backfill");
        modeSuffix = QStringLiteral("分层回淤");
        break;
    default:
        taskType = QStringLiteral("autosection");
        modeSuffix = QStringLiteral("backfill");
        break;
    }

    jsonData[QStringLiteral("task_type")] = taskType;
    jsonData[QStringLiteral("input_dxf")] = filePath;
    jsonData[QStringLiteral("target_elevation")] = fullCalculation
        ? QJsonValue(QJsonValue::Null)
        : QJsonValue(targetElevation);
    jsonData[QStringLiteral("calc_mode")] = calcMode;
    jsonData[QStringLiteral("distinguish_design")] = distinguishDesign;
    jsonData[QStringLiteral("merge_section")] = mergeSection;
    jsonData[QStringLiteral("extend_overexc_lines")] =
        mode == SectionTaskMode::Combined && extendOverexc;

    QJsonArray strataArr;
    for (const QString &l : data.strataLayers) strataArr.append(l);
    jsonData[QStringLiteral("strata_layers")] = strataArr;
    jsonData[QStringLiteral("pile_layer")] = pileLayer;

    QString timestamp = compactTimestamp();
    QString baseDir = outputDir.isEmpty() ? QFileInfo(filePath).absolutePath() : outputDir;
    QString baseName = QFileInfo(filePath).completeBaseName();
    if (!QDir().mkpath(baseDir)) {
        log(QStringLiteral("[ERROR] 无法创建输出目录: %1").arg(baseDir), kError);
        result[kSuccess] = false;
        return false;
    }

    const QString elevationLabel = fullCalculation
        ? QStringLiteral("全算量")
        : QStringLiteral("%1m").arg(QString::number(targetElevation));
    QString outputPath = QStringLiteral("%1/%2_%3%4_%5.dxf")
        .arg(baseDir, baseName, elevationLabel, modeSuffix, timestamp);
    QString xlsxPath = QStringLiteral("%1/%2_%3%4_%5.xlsx")
        .arg(baseDir, baseName, elevationLabel, modeSuffix, timestamp);

    jsonData[QStringLiteral("output_dxf")] = outputPath;
    jsonData[QStringLiteral("output_xlsx")] = xlsxPath;

    QSet<QString> processedStations;
    QJsonArray sectionsArr;
    for (int idx = 0; idx < data.dmxList.size(); ++idx) {
        const EntityListData &dmx = data.dmxList[idx];
        auto matchResult = StationMatcher::matchSectionToStation(dmx.xCenter, dmx.yCenter, data.sortedStations, processedStations);
        QString station;
        if (matchResult.second < 500) {
            station = StationMatcher::cleanStationText(matchResult.first.text);
            processedStations.insert(station);
        } else {
            station = QStringLiteral("S%1").arg(idx + 1);
        }

        Box2D bbox(dmx.xMin - 20, dmx.yMin - 50, dmx.xMax + 20, dmx.yMax + 50);

        const QVector<Line2D> *auxPtr = nullptr;
        const QVector<Line2D> *updatePtr = nullptr;
        if (mode == SectionTaskMode::Autosection && mergeSection)
            auxPtr = &auxLines;
        if (mode == SectionTaskMode::Combined)
            updatePtr = &updateLines;

        sectionsArr.append(buildSectionJson(data, station, idx, bbox, auxPtr, updatePtr));
    }
    jsonData[QStringLiteral("sections")] = sectionsArr;
    jsonData[QStringLiteral("excav_lines")] = buildExcavJson(data.excavLines);
    jsonData[QStringLiteral("overexc_lines")] = buildExcavJson(data.overexcLines);

    log(QStringLiteral("[INFO] 调用Python计算..."), kInfo);

    PythonBridge::Result pyResult = PythonBridge::run(filePath, outputPath, xlsxPath, jsonData, log);
    if (!pyResult.success) {
        result[kSuccess] = false;
        return false;
    }

    if (mode == SectionTaskMode::Combined)
        log(QStringLiteral("[OK] 合并任务完成"), kSuccess);
    log(QStringLiteral("[OK] DXF: %1").arg(outputPath), kInfo);
    log(QStringLiteral("[OK] XLSX: %1").arg(xlsxPath), kInfo);

    result[kSuccess] = true;
    result[QStringLiteral("outputPath")] = outputPath;
    result[QStringLiteral("xlsxPath")] = xlsxPath;
    result[QStringLiteral("totalArea")] = pyResult.totalArea;
    if (mode == SectionTaskMode::Combined)
        result[QStringLiteral("backfillArea")] = pyResult.backfillArea;
    return true;
}

bool EngineCad::runAutosection(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result) {
    return runSectionTask(SectionTaskMode::Autosection, params, log, result);
}

bool EngineCad::runAutosectionBackfill(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result) {
    return runSectionTask(SectionTaskMode::Combined, params, log, result);
}

// ==================== Excel data migration ====================

bool EngineCad::runExcelMigration(const QMap<QString, QString> &params,
                                  LogCallback log, QJsonObject &result)
{
    const QString sourcePath = params.value(QStringLiteral("源文件名")).trimmed();
    const QString targetPath = params.value(QStringLiteral("目标文件名")).trimmed();
    const QString coefficient = params.value(QStringLiteral("面积系数"), QStringLiteral("0.6")).trimmed();
    QString outputDir = params.value(QStringLiteral("输出目录")).trimmed();

    if (!QFileInfo::exists(sourcePath)) {
        result[QStringLiteral("error")] = QStringLiteral("算量结果文件不存在");
        log(QStringLiteral("[ERROR] 算量结果文件不存在: %1").arg(sourcePath), kError);
        return false;
    }
    if (!QFileInfo::exists(targetPath)) {
        result[QStringLiteral("error")] = QStringLiteral("月进度表模板不存在");
        log(QStringLiteral("[ERROR] 月进度表模板不存在: %1").arg(targetPath), kError);
        return false;
    }

    bool coefficientOk = false;
    const double coefficientValue = coefficient.toDouble(&coefficientOk);
    if (!coefficientOk || coefficientValue <= 0.0) {
        result[QStringLiteral("error")] = QStringLiteral("面积系数必须为大于 0 的数字");
        log(QStringLiteral("[ERROR] 面积系数必须为大于 0 的数字"), kError);
        return false;
    }

    if (outputDir.isEmpty())
        outputDir = QFileInfo(targetPath).absolutePath();
    if (!QDir().mkpath(outputDir)) {
        result[QStringLiteral("error")] = QStringLiteral("无法创建输出目录");
        log(QStringLiteral("[ERROR] 无法创建输出目录: %1").arg(outputDir), kError);
        return false;
    }

    const QString scriptPath = locateExcelMigrationScript();
    if (scriptPath.isEmpty()) {
        result[QStringLiteral("error")] = QStringLiteral("未找到 Excel 迁移脚本");
        log(QStringLiteral("[ERROR] 未找到 scripts/excel_migrate.py"), kError);
        return false;
    }

    const QString timestamp = compactTimestamp();
    const QString outputPath = QDir(outputDir).filePath(
        QStringLiteral("%1_迁移_%2.xlsx")
            .arg(QFileInfo(targetPath).completeBaseName(), timestamp));

    log(QStringLiteral("[INFO] 开始迁移 Excel 数据"), kInfo);
    log(QStringLiteral("[INFO] 面积系数: %1").arg(coefficient), kInfo);
    log(QStringLiteral("[INFO] 运行脚本: %1").arg(scriptPath), kInfo);

    const QString pythonExecutable = locatePythonExecutable();
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    environment.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    if (QFileInfo(pythonExecutable).isAbsolute()) {
        environment.insert(QStringLiteral("PYTHONHOME"), QFileInfo(pythonExecutable).absolutePath());
        environment.insert(QStringLiteral("PYTHONNOUSERSITE"), QStringLiteral("1"));
        log(QStringLiteral("[INFO] 使用内置 Python: %1").arg(pythonExecutable), kInfo);
    }
    process.setProcessEnvironment(environment);
    process.start(pythonExecutable, {
        scriptPath,
        sourcePath,
        targetPath,
        QStringLiteral("--coefficient"), coefficient,
        QStringLiteral("--output"), outputPath
    });
    if (!process.waitForStarted(10000)) {
        result[QStringLiteral("error")] = QStringLiteral("无法启动 Python，请检查 Python 和 openpyxl");
        log(QStringLiteral("[ERROR] 无法启动 Python: %1").arg(process.errorString()), kError);
        return false;
    }

    if (!process.waitForFinished(600000)) {
        process.kill();
        process.waitForFinished();
        result[QStringLiteral("error")] = QStringLiteral("Excel 迁移超时");
        log(QStringLiteral("[ERROR] Excel 迁移超过 10 分钟"), kError);
        return false;
    }

    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(process.readAllStandardError());
    QJsonObject pythonResult;
    bool foundResult = false;
    for (const QString &rawLine : stdoutText.split('\n', Qt::SkipEmptyParts)) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("__RESULT__:"))) {
            const QByteArray jsonBytes = line.mid(QStringLiteral("__RESULT__:").size()).toUtf8();
            const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
            if (document.isObject()) {
                pythonResult = document.object();
                foundResult = true;
            }
        } else if (!line.isEmpty()) {
            log(line, kInfo);
        }
    }
    for (const QString &rawLine : stderrText.split('\n', Qt::SkipEmptyParts)) {
        const QString line = rawLine.trimmed();
        if (!line.isEmpty())
            log(QStringLiteral("[PY] %1").arg(line), kWarn);
    }

    if (!foundResult) {
        result[QStringLiteral("error")] = QStringLiteral("迁移脚本未返回结果");
        log(QStringLiteral("[ERROR] 迁移脚本未返回结构化结果"), kError);
        return false;
    }
    if (process.exitCode() != 0 || !pythonResult.value(QStringLiteral("success")).toBool()) {
        const QString error = pythonResult.value(QStringLiteral("error")).toString(
            QStringLiteral("Excel 迁移失败"));
        result[QStringLiteral("error")] = error;
        log(QStringLiteral("[ERROR] %1").arg(error), kError);
        return false;
    }
    if (!QFileInfo::exists(outputPath)) {
        result[QStringLiteral("error")] = QStringLiteral("迁移完成但未找到输出文件");
        log(QStringLiteral("[ERROR] 未找到输出文件: %1").arg(outputPath), kError);
        return false;
    }

    result = pythonResult;
    result[kSuccess] = true;
    result[QStringLiteral("outputPath")] = outputPath;
    log(QStringLiteral("[OK] Excel 迁移完成: %1").arg(outputPath), kSuccess);
    return true;
}

// ==================== v4.2 thickness-banded geology top view ====================

bool EngineCad::runGeologyTopview(const QMap<QString, QString> &params,
                                  LogCallback log, QJsonObject &result)
{
    const QString inputPath = params.value(QStringLiteral("files")).trimmed();
    const QString spinePath = params.value(QStringLiteral("脊梁点JSON")).trimmed();
    const QString thicknessText =
        params.value(QStringLiteral("回淤最小厚度"), QStringLiteral("0.20")).trimmed();
    QString outputDir = params.value(QStringLiteral("输出目录")).trimmed();

    if (!QFileInfo::exists(inputPath)) {
        result[QStringLiteral("error")] = QStringLiteral("断面 DXF 不存在");
        log(QStringLiteral("[ERROR] 断面 DXF 不存在: %1").arg(inputPath), kError);
        return false;
    }
    if (!QFileInfo::exists(spinePath)) {
        result[QStringLiteral("error")] = QStringLiteral("脊梁点匹配 JSON 不存在");
        log(QStringLiteral("[ERROR] 脊梁点匹配 JSON 不存在: %1").arg(spinePath), kError);
        return false;
    }

    bool thicknessOk = false;
    const double minThickness = thicknessText.toDouble(&thicknessOk);
    if (!thicknessOk || !std::isfinite(minThickness) || minThickness < 0.0) {
        result[QStringLiteral("error")] =
            QStringLiteral("回淤最小厚度必须是大于或等于 0 的数字");
        log(QStringLiteral("[ERROR] 回淤最小厚度必须是大于或等于 0 的数字"), kError);
        return false;
    }

    if (outputDir.isEmpty())
        outputDir = QFileInfo(inputPath).absolutePath();
    if (!QDir().mkpath(outputDir)) {
        result[QStringLiteral("error")] = QStringLiteral("无法创建输出目录");
        log(QStringLiteral("[ERROR] 无法创建输出目录: %1").arg(outputDir), kError);
        return false;
    }

    const QString scriptPath = locateGeologyTopviewScript();
    if (scriptPath.isEmpty()) {
        result[QStringLiteral("error")] = QStringLiteral("未找到 v4.2 俯视图生成脚本");
        log(QStringLiteral("[ERROR] 未找到 scripts/geology_topview_v41.py"), kError);
        return false;
    }

    QString thicknessLabel = QString::number(minThickness, 'f', 3);
    while (thicknessLabel.contains('.') && thicknessLabel.endsWith('0'))
        thicknessLabel.chop(1);
    if (thicknessLabel.endsWith('.'))
        thicknessLabel.chop(1);

    const QString timestamp = compactTimestamp();
    QString outputBaseName = QFileInfo(inputPath).completeBaseName();
    if (outputBaseName.size() > 32)
        outputBaseName = outputBaseName.left(32);
    while (outputBaseName.endsWith('_') || outputBaseName.endsWith('-'))
        outputBaseName.chop(1);
    const QString outputPath = QDir(outputDir).filePath(
        QStringLiteral("%1_v4.2四色厚度分层_%2m_%3.dxf")
            .arg(outputBaseName, thicknessLabel, timestamp));
    QFile outputProbe(outputPath);
    if (!outputProbe.open(QIODevice::WriteOnly)) {
        result[QStringLiteral("error")] = QStringLiteral("无法写入输出 DXF");
        log(QStringLiteral("[ERROR] 无法写入输出文件: %1").arg(outputPath), kError);
        return false;
    }
    outputProbe.close();

    log(QStringLiteral("[INFO] 开始生成 v4.2 四色厚度分层地质精细俯视图"), kInfo);
    log(QStringLiteral("[INFO] 分类: 淤泥（含填土）、砂（含碎石）、黏土、回淤"), kInfo);
    log(QStringLiteral("[INFO] 回淤最小厚度: %1 m").arg(thicknessLabel), kInfo);
    log(QStringLiteral("[INFO] 横向网格: 2.5 m；不膨胀、不限制面积"), kInfo);

    const QString pythonExecutable = locatePythonExecutable();
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    environment.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    if (QFileInfo(pythonExecutable).isAbsolute()) {
        environment.insert(QStringLiteral("PYTHONHOME"),
                           QFileInfo(pythonExecutable).absolutePath());
        environment.insert(QStringLiteral("PYTHONNOUSERSITE"), QStringLiteral("1"));
        log(QStringLiteral("[INFO] 使用内置 Python: %1").arg(pythonExecutable), kInfo);
    }
    process.setProcessEnvironment(environment);
    process.start(pythonExecutable, {
        scriptPath,
        inputPath,
        spinePath,
        QStringLiteral("--min-backfill-thickness"),
        QString::number(minThickness, 'g', 15),
        QStringLiteral("--output"),
        outputPath
    });
    if (!process.waitForStarted(10000)) {
        result[QStringLiteral("error")] =
            QStringLiteral("无法启动俯视图生成程序");
        log(QStringLiteral("[ERROR] 无法启动 Python: %1").arg(process.errorString()), kError);
        return false;
    }

    if (!process.waitForFinished(1200000)) {
        process.kill();
        process.waitForFinished();
        result[QStringLiteral("error")] = QStringLiteral("俯视图生成超时");
        log(QStringLiteral("[ERROR] 俯视图生成超过 20 分钟"), kError);
        return false;
    }

    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(process.readAllStandardError());
    QJsonObject pythonResult;
    bool foundResult = false;
    for (const QString &rawLine : stdoutText.split('\n', Qt::SkipEmptyParts)) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("__RESULT__:"))) {
            const QByteArray jsonBytes =
                line.mid(QStringLiteral("__RESULT__:").size()).toUtf8();
            const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
            if (document.isObject()) {
                pythonResult = document.object();
                foundResult = true;
            }
        } else if (!line.isEmpty()) {
            log(line, kInfo);
        }
    }
    for (const QString &rawLine : stderrText.split('\n', Qt::SkipEmptyParts)) {
        const QString line = rawLine.trimmed();
        if (!line.isEmpty())
            log(QStringLiteral("[PY] %1").arg(line), kWarn);
    }

    if (!foundResult) {
        result[QStringLiteral("error")] =
            QStringLiteral("俯视图生成程序未返回结构化结果");
        log(QStringLiteral("[ERROR] 俯视图生成程序未返回结构化结果"), kError);
        return false;
    }
    if (process.exitCode() != 0 ||
        !pythonResult.value(QStringLiteral("success")).toBool()) {
        const QString error = pythonResult.value(QStringLiteral("error")).toString(
            QStringLiteral("四类地质俯视图生成失败"));
        result[QStringLiteral("error")] = error;
        log(QStringLiteral("[ERROR] %1").arg(error), kError);
        return false;
    }
    if (!QFileInfo::exists(outputPath)) {
        result[QStringLiteral("error")] =
            QStringLiteral("生成完成但未找到输出 DXF");
        log(QStringLiteral("[ERROR] 未找到输出文件: %1").arg(outputPath), kError);
        return false;
    }

    result = pythonResult;
    result[kSuccess] = true;
    result[QStringLiteral("outputPath")] = outputPath;
    log(QStringLiteral("[OK] 四类地质精细俯视图生成完成: %1").arg(outputPath),
        kSuccess);
    return true;
}

// ==================== 5. runBackfill ====================

bool EngineCad::runBackfill(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result)
{
    QString designLayer = params.value(QStringLiteral("断面线图层"));
    QString sectionLayer = params.value(QStringLiteral("设计断面线图层"), QStringLiteral("DMX"));
    QString outputDir = params.value(QStringLiteral("输出目录"));
    QString filePath = params.value(QStringLiteral("files"));

    if (designLayer.isEmpty()) {
        log(QStringLiteral("[ERROR] 请指定设计断面线图层名称"), kError);
        return false;
    }

    SectionData data;
    if (!loadSectionData(filePath, sectionLayer, QString(), 0, log, data))
        return false;

    data.designLines = data.dxf.getLines(designLayer);
    std::sort(data.designLines.begin(), data.designLines.end(),
              [](const Line2D &a, const Line2D &b) { return a.minY() > b.minY(); });

    DXFWrapper outputDxf = data.dxf.createCopy();
    QString backfillLayer = QStringLiteral("回淤面积填充");
    outputDxf.createLayer(backfillLayer, 1);

    QVector<QJsonObject> results;
    for (int idx = 0; idx < data.dmxList.size(); ++idx) {
        const EntityListData &dmx = data.dmxList[idx];
        QString station = QStringLiteral("S%1").arg(idx + 1);
        Box2D bbox(dmx.xMin - 20, dmx.yMin - 50, dmx.xMax + 20, dmx.yMax + 50);

        QVector<Line2D> localDesign;
        for (const Line2D &l : data.designLines)
            if (bbox.intersects(l)) localDesign.append(l);

        if (localDesign.isEmpty()) {
            results.append(QJsonObject{{QStringLiteral("桩号"), station}, {QStringLiteral("回淤面积"), 0.0}});
            continue;
        }

        Line2D upperEnv = EnvelopeGenerator::generate(dmx.line, localDesign, EnvelopeGenerator::EnvelopeType::Upper);
        if (!upperEnv.isValid()) {
            results.append(QJsonObject{{QStringLiteral("桩号"), station}, {QStringLiteral("回淤面积"), 0.0}});
            continue;
        }

        double commonXMin = std::max(dmx.line.minX(), upperEnv.minX());
        double commonXMax = std::min(dmx.line.maxX(), upperEnv.maxX());
        if (commonXMax <= commonXMin) {
            results.append(QJsonObject{{QStringLiteral("桩号"), station}, {QStringLiteral("回淤面积"), 0.0}});
            continue;
        }

        double xRange = commonXMax - commonXMin;
        int numSamples = std::max((int)(xRange / 0.5) + 1, 50);
        QVector<Point2D> polyCoords;

        for (int i = 0; i <= numSamples; ++i) {
            double x = commonXMin + xRange * i / numSamples;
            bool found; double y = LineUtils::getYAtX(upperEnv, x, &found);
            if (found) polyCoords.append(Point2D(x, y));
        }
        for (int i = numSamples; i >= 0; --i) {
            double x = commonXMin + xRange * i / numSamples;
            bool found; double y = LineUtils::getYAtX(dmx.line, x, &found);
            if (found) polyCoords.append(Point2D(x, y));
        }

        if (polyCoords.size() >= 3) {
            Polygon2D backfillPoly(polyCoords);
            double area = backfillPoly.area();
            if (area > 0.01)
                outputDxf.addHatch(backfillPoly, backfillLayer, QStringLiteral("SOLID"), 1.0, QColor(255, 0, 0));
            results.append(QJsonObject{{QStringLiteral("桩号"), station}, {QStringLiteral("回淤面积"), area}});
        } else {
            results.append(QJsonObject{{QStringLiteral("桩号"), station}, {QStringLiteral("回淤面积"), 0.0}});
        }
    }

    QString timestamp = compactTimestamp();
    QString outputPath = buildBackfillOutputNameStatic(QFileInfo(filePath).completeBaseName(), timestamp);
    outputDxf.save(outputPath);

    log(QStringLiteral("[OK] DXF: %1").arg(outputPath), kSuccess);
    result[kSuccess] = true;
    result[QStringLiteral("outputPath")] = outputPath;
    return true;
}

// ==================== 1. runAutoline ====================

bool EngineCad::runAutoline(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result)
{
    QString layerA = params.value(QStringLiteral("图层A名称"));
    QString layerB = params.value(QStringLiteral("图层B名称"));
    QString envelopeType = params.value(QStringLiteral("包络线类型"), QStringLiteral("lower"));
    QString outputLayer = params.value(QStringLiteral("输出图层名"), Config::DEFAULT_OUTPUT_LAYER);
    QString outputDir = params.value(QStringLiteral("输出目录"));
    QString filePath = params.value(QStringLiteral("files"));

    if (layerA.isEmpty() || layerB.isEmpty()) {
        log(QStringLiteral("[ERROR] 无法获取图层名称"), kError);
        result[kSuccess] = false;
        return false;
    }
    if (filePath.isEmpty()) {
        log(QStringLiteral("[WARN] 请先添加文件"), kWarn);
        result[kSuccess] = false;
        return false;
    }

    QString typeName = (envelopeType == QStringLiteral("lower")) ? QStringLiteral("下包络") : QStringLiteral("上包络");
    log(QStringLiteral("[INFO] 包络线类型: %1").arg(typeName), kInfo);

    DXFWrapper dxf;
    if (!dxf.read(filePath)) {
        log(QStringLiteral("[ERROR] DXF文件读取失败"), kError);
        result[kSuccess] = false;
        return false;
    }

    QVector<Line2D> linesA = dxf.getLines(layerA);
    QVector<Line2D> linesB = dxf.getLines(layerB);

    if (linesA.isEmpty() && linesB.isEmpty()) {
        log(QStringLiteral("[WARN] 指定图层没有线段"), kWarn);
        result[kSuccess] = false;
        return false;
    }

    log(QStringLiteral("[INFO] 图层A: %1条, 图层B: %2条").arg(linesA.size()).arg(linesB.size()), kInfo);

    EnvelopeGenerator::EnvelopeType envType = EnvelopeGenerator::parseEnvelopeType(envelopeType);
    QVector<Line2D> envelopeLines;
    QSet<int> usedB;

    for (const Line2D &lineA : linesA) {
        QVector<Line2D> group = {lineA};
        for (int i = 0; i < linesB.size(); ++i) {
            if (usedB.contains(i)) continue;
            if (LineUtils::intersects(lineA, linesB[i]) || LineUtils::distance(lineA, linesB[i]) < 0.5) {
                group.append(linesB[i]);
                usedB.insert(i);
            }
        }
        if (group.size() > 1) {
            Line2D env = EnvelopeGenerator::generate(group[0], group.mid(1), envType);
            if (env.isValid() && env.length() > 0.01) envelopeLines.append(env);
        } else {
            envelopeLines.append(lineA);
        }
    }

    for (int i = 0; i < linesB.size(); ++i)
        if (!usedB.contains(i)) envelopeLines.append(linesB[i]);

    log(QStringLiteral("[INFO] 包络线: %1条").arg(envelopeLines.size()), kInfo);

    DXFWrapper outputDxf = dxf.createCopy();
    outputDxf.createLayer(outputLayer, 3);
    for (const Line2D &line : envelopeLines)
        outputDxf.addLWPolyline(line.points, outputLayer, 3);

    QString outputPath = getOutputPathStatic(filePath, QStringLiteral("_%1合并").arg(typeName), outputDir);
    if (outputDxf.save(outputPath)) {
        log(QStringLiteral("[OK] 保存至: %1").arg(QFileInfo(outputPath).fileName()), kSuccess);
        result[kSuccess] = true;
        result[QStringLiteral("outputPath")] = outputPath;
        return true;
    }

    log(QStringLiteral("[ERROR] DXF写入失败"), kError);
    result[kSuccess] = false;
    return false;
}

// ==================== 2. runAutopaste ====================

bool EngineCad::runAutopaste(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result)
{
    QString srcPath = params.value(QStringLiteral("源文件名"));
    QString dstPath = params.value(QStringLiteral("目标文件名"));
    QString outputLayer = params.value(QStringLiteral("输出图层名"), QStringLiteral("0-已粘贴断面")).trimmed();
    QString outputDir = params.value(QStringLiteral("输出目录")).trimmed();
    if (outputLayer.isEmpty())
        outputLayer = QStringLiteral("0-已粘贴断面");

    if (srcPath.isEmpty() || dstPath.isEmpty()) {
        log(QStringLiteral("[ERROR] 请先选择源文件和目标文件"), kError);
        result[kSuccess] = false;
        return false;
    }

    log(QStringLiteral("=").repeated(60), kInfo);
    log(QStringLiteral("[成套对应粘贴 v2] 开始"), kInfo);
    log(QStringLiteral("=").repeated(60), kInfo);
    log(QStringLiteral("源文件: %1").arg(QFileInfo(srcPath).fileName()), kInfo);
    log(QStringLiteral("目标文件: %1").arg(QFileInfo(dstPath).fileName()), kInfo);
    log(QStringLiteral("输出断面线图层: %1").arg(outputLayer), kInfo);

    // --- Read source DXF ---
    DXFWrapper srcDxf;
    if (!srcDxf.read(srcPath)) {
        log(QStringLiteral("[ERROR] 源文件读取失败"), kError);
        result[kSuccess] = false;
        return false;
    }

    // --- Read destination DXF ---
    DXFWrapper dstDxf;
    if (!dstDxf.read(dstPath)) {
        log(QStringLiteral("[ERROR] 目标文件读取失败"), kError);
        result[kSuccess] = false;
        return false;
    }

    // ===== Phase 1: Detect source file matched sets =====
    log(QStringLiteral("[检测源文件成套数据 v2]"), kInfo);

    // 1a. Detect small rectangles on XSECTION layer
    QVector<Line2D> srcXsectionLines = srcDxf.getLines(QStringLiteral("XSECTION"));
    QVector<SmallRectInfo> smallRects;
    for (const Line2D &line : srcXsectionLines) {
        if (line.points.size() < 4) continue;
        Box2D bbox = line.bounds();
        double w = bbox.width();
        double h = bbox.height();
        if (w > 130.0 && w < 200.0 && h >= 95.0 && h < 140.0) {
            SmallRectInfo rect;
            rect.bbox = bbox;
            rect.basepoint = Point2D(bbox.centerX(), bbox.maxY);
            rect.centerY = bbox.centerY();
            smallRects.append(rect);
        }
    }

    // Sort by centerY descending (top to bottom)
    std::sort(smallRects.begin(), smallRects.end(),
              [](const SmallRectInfo &a, const SmallRectInfo &b) { return a.centerY > b.centerY; });

    log(QStringLiteral("  小矩形数量: %1").arg(smallRects.size()), kInfo);

    // 1b. Detect cross-section curves (>50 vertices) on XSECTION layer
    QVector<CurveInfo> curves;
    for (int i = 0; i < srcXsectionLines.size(); ++i) {
        const Line2D &line = srcXsectionLines[i];
        if (line.points.size() > 50) {
            Box2D bbox = line.bounds();
            CurveInfo curve;
            curve.bbox = bbox;
            curve.center = Point2D(bbox.centerX(), bbox.centerY());
            curve.vertexCount = line.points.size();
            curve.lineIndex = i;
            curves.append(curve);
        }
    }
    log(QStringLiteral("  断面曲线数量: %1").arg(curves.size()), kInfo);

    // 1c. Detect station annotations in source file
    auto parseSourceStation = [](const QString &text) -> int {
        QString upper = text.toUpper();
        // Try .TIN format first: 06+400.TIN
        static QRegularExpression reTin(QStringLiteral(R"((\d+)\+(\d+)\.TIN)"));
        QRegularExpressionMatch m = reTin.match(upper);
        if (m.hasMatch()) return m.captured(1).toInt() * 1000 + m.captured(2).toInt();
        // Try plain format: 06+400
        static QRegularExpression rePlain(QStringLiteral(R"((\d+)\+(\d+))"));
        m = rePlain.match(upper);
        if (m.hasMatch()) return m.captured(1).toInt() * 1000 + m.captured(2).toInt();
        return -1;
    };

    QSet<int> stationValueSet;
    QVector<DXFWrapper::TextInfo> srcTexts = srcDxf.getTexts();
    for (const DXFWrapper::TextInfo &ti : srcTexts) {
        int val = parseSourceStation(ti.text);
        if (val >= 0) stationValueSet.insert(val);
    }
    QVector<int> sortedStationValues(stationValueSet.begin(), stationValueSet.end());
    std::sort(sortedStationValues.begin(), sortedStationValues.end());
    log(QStringLiteral("  不同桩号值数量: %1").arg(sortedStationValues.size()), kInfo);

    // 1d. Build source sets: match rect -> curve -> station
    QVector<SourceSetInfo> sourceSets;
    for (int i = 0; i < smallRects.size(); ++i) {
        const SmallRectInfo &rect = smallRects[i];
        SourceSetInfo ss;
        ss.index = i + 1;
        ss.rectBbox = rect.bbox;
        ss.basepoint = rect.basepoint;
        ss.centerY = rect.centerY;
        ss.hasCurve = false;
        ss.curveLineIndex = -1;
        ss.station = -1;

        // Find curve whose center is inside the rect bbox
        for (const CurveInfo &curve : curves) {
            if (curve.center.x > rect.bbox.minX && curve.center.x < rect.bbox.maxX &&
                curve.center.y > rect.bbox.minY && curve.center.y < rect.bbox.maxY) {
                ss.hasCurve = true;
                ss.curveLineIndex = curve.lineIndex;
                ss.curvePoints = srcXsectionLines[curve.lineIndex].points;
                break;
            }
        }

        // Positional station matching
        if (i < sortedStationValues.size()) {
            ss.station = sortedStationValues[i];
            int km = ss.station / 1000;
            int m = ss.station % 1000;
            ss.stationText = QStringLiteral("K%1+%2").arg(km, 2, 10, QLatin1Char('0')).arg(m, 3, 10, QLatin1Char('0'));
        }

        sourceSets.append(ss);
    }

    int withCurve = 0, withStation = 0;
    for (const SourceSetInfo &s : sourceSets) {
        if (s.hasCurve) ++withCurve;
        if (s.station >= 0) ++withStation;
    }
    log(QStringLiteral("  成套数量: %1").arg(sourceSets.size()), kInfo);
    log(QStringLiteral("  有断面线: %1").arg(withCurve), kInfo);
    log(QStringLiteral("  有桩号: %1").arg(withStation), kInfo);

    // ===== Phase 2: Detect target file matched sets =====
    log(QStringLiteral("[检测目标文件成套数据 v2]"), kInfo);

    // 2a. Detect L1 basepoints
    QVector<Line2D> l1Lines = dstDxf.getLines(QStringLiteral("L1"));
    struct HLine { double y, xMin, xMax; };
    struct VLine { double x, yMin, yMax, yCenter; };

    QVector<HLine> hLines;
    QVector<VLine> vLines;

    for (const Line2D &line : l1Lines) {
        if (line.points.size() != 2) continue;
        double x1 = line.points[0].x, y1 = line.points[0].y;
        double x2 = line.points[1].x, y2 = line.points[1].y;
        double w = std::abs(x2 - x1);
        double h = std::abs(y2 - y1);

        if (w > h * 3.0) {
            hLines.append({(y1 + y2) / 2.0, std::min(x1, x2), std::max(x1, x2)});
        } else if (h > w * 3.0) {
            vLines.append({(x1 + x2) / 2.0, std::min(y1, y2), std::max(y1, y2), (y1 + y2) / 2.0});
        }
    }

    log(QStringLiteral("  水平线数量: %1").arg(hLines.size()), kInfo);
    log(QStringLiteral("  垂直线数量: %1").arg(vLines.size()), kInfo);

    // Sort: hLines by Y descending, vLines by X ascending
    std::sort(hLines.begin(), hLines.end(), [](const HLine &a, const HLine &b) { return a.y > b.y; });
    std::sort(vLines.begin(), vLines.end(), [](const VLine &a, const VLine &b) { return a.x < b.x; });

    // Match vertical lines to closest horizontal line (Y distance < 50)
    QVector<BasepointInfo> basepoints;
    QVector<bool> hUsed(hLines.size(), false);

    for (const VLine &vl : vLines) {
        int bestIdx = -1;
        double bestDiff = std::numeric_limits<double>::max();
        for (int hi = 0; hi < hLines.size(); ++hi) {
            if (hUsed[hi]) continue;
            double diff = std::abs(hLines[hi].y - vl.yCenter);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestIdx = hi;
            }
        }
        if (bestIdx >= 0 && bestDiff < 50.0) {
            hUsed[bestIdx] = true;
            basepoints.append({vl.x, hLines[bestIdx].y});
        }
    }
    log(QStringLiteral("  基点数量: %1").arg(basepoints.size()), kInfo);

    // 2b. Detect station texts in target
    auto parseTargetStation = [](const QString &text) -> int {
        QString upper = text.toUpper();
        static QRegularExpression reK(QStringLiteral(R"(K(\d+)\+(\d+))"));
        QRegularExpressionMatch m = reK.match(upper);
        if (m.hasMatch()) return m.captured(1).toInt() * 1000 + m.captured(2).toInt();
        static QRegularExpression rePlain(QStringLiteral(R"((\d+)\+(\d+))"));
        m = rePlain.match(upper);
        if (m.hasMatch()) return m.captured(1).toInt() * 1000 + m.captured(2).toInt();
        return -1;
    };

    QVector<TargetStationInfo> stationTexts;
    QVector<DXFWrapper::TextInfo> dstTexts = dstDxf.getTexts();
    for (const DXFWrapper::TextInfo &ti : dstTexts) {
        int val = parseTargetStation(ti.text);
        if (val >= 0) {
            stationTexts.append({ti.text, val, ti.x, ti.y});
        }
    }
    log(QStringLiteral("  桩号标注数量: %1").arg(stationTexts.size()), kInfo);

    // 2c. Match basepoints to stations by X-coordinate grouping
    struct BPGroup { double x; QVector<int> indices; };
    QVector<BPGroup> bpGroups;
    for (int i = 0; i < basepoints.size(); ++i) {
        double bx = basepoints[i].x;
        bool assigned = false;
        for (BPGroup &g : bpGroups) {
            if (std::abs(bx - g.x) < 50.0) {
                g.indices.append(i);
                assigned = true;
                break;
            }
        }
        if (!assigned) {
            BPGroup g; g.x = bx; g.indices.append(i);
            bpGroups.append(g);
        }
    }

    // Group station texts by X (tolerance 50)
    struct StGroup { double x; QVector<int> indices; };
    QVector<StGroup> stGroups;
    for (int i = 0; i < stationTexts.size(); ++i) {
        double sx = stationTexts[i].x;
        bool assigned = false;
        for (StGroup &g : stGroups) {
            if (std::abs(sx - g.x) < 50.0) {
                g.indices.append(i);
                assigned = true;
                break;
            }
        }
        if (!assigned) {
            StGroup g; g.x = sx; g.indices.append(i);
            stGroups.append(g);
        }
    }

    // Match BP groups to station groups by X proximity
    QMap<QPair<double,double>, TargetStationInfo> bpStationMap;

    for (const BPGroup &bpG : bpGroups) {
        double bestXDiff = std::numeric_limits<double>::max();
        int bestStGroupIdx = -1;
        for (int si = 0; si < stGroups.size(); ++si) {
            double xd = std::abs(bpG.x - stGroups[si].x);
            if (xd < bestXDiff) {
                bestXDiff = xd;
                bestStGroupIdx = si;
            }
        }
        if (bestStGroupIdx >= 0 && bestXDiff < 50.0) {
            // Sort both groups by Y descending, match 1:1
            QVector<int> bpSorted = bpG.indices;
            QVector<int> stSorted = stGroups[bestStGroupIdx].indices;
            std::sort(bpSorted.begin(), bpSorted.end(), [&](int a, int b) {
                return basepoints[a].y > basepoints[b].y;
            });
            std::sort(stSorted.begin(), stSorted.end(), [&](int a, int b) {
                return stationTexts[a].y > stationTexts[b].y;
            });
            int count = std::min(bpSorted.size(), stSorted.size());
            for (int i = 0; i < count; ++i) {
                int bpIdx = bpSorted[i];
                int stIdx = stSorted[i];
                bpStationMap.insert(QPair<double,double>(basepoints[bpIdx].x, basepoints[bpIdx].y),
                                   stationTexts[stIdx]);
            }
        }
    }

    // Build target sets
    QVector<TargetSetInfo> targetSets;
    for (int i = 0; i < basepoints.size(); ++i) {
        TargetSetInfo ts;
        ts.basepoint = Point2D(basepoints[i].x, basepoints[i].y);
        QPair<double,double> key(basepoints[i].x, basepoints[i].y);
        if (bpStationMap.contains(key)) {
            ts.station = bpStationMap[key].value;
            ts.stationText = bpStationMap[key].text;
        } else {
            ts.station = -1;
        }
        targetSets.append(ts);
    }

    int targetWithStation = 0;
    for (const TargetSetInfo &t : targetSets) if (t.station >= 0) ++targetWithStation;
    log(QStringLiteral("  有桩号: %1").arg(targetWithStation), kInfo);

    // ===== Phase 3: Station matching =====
    log(QStringLiteral("[桩号匹配 v2]"), kInfo);

    QMap<int, const SourceSetInfo*> sourceByStation;
    for (const SourceSetInfo &s : sourceSets) {
        if (s.station >= 0 && s.hasCurve) sourceByStation.insert(s.station, &s);
    }

    QMap<int, const TargetSetInfo*> targetByStation;
    for (const TargetSetInfo &t : targetSets) {
        if (t.station >= 0) targetByStation.insert(t.station, &t);
    }

    log(QStringLiteral("  源桩号索引: %1").arg(sourceByStation.size()), kInfo);
    log(QStringLiteral("  目标桩号索引: %1").arg(targetByStation.size()), kInfo);

    struct MatchedPair {
        const SourceSetInfo *source;
        const TargetSetInfo *target;
        int station;
    };
    QVector<MatchedPair> matchedPairs;
    QSet<int> matchedStations;

    for (auto it = sourceByStation.begin(); it != sourceByStation.end(); ++it) {
        int sv = it.key();
        if (targetByStation.contains(sv)) {
            matchedPairs.append({it.value(), targetByStation[sv], sv});
            matchedStations.insert(sv);
        }
    }

    log(QStringLiteral("  匹配成功: %1对").arg(matchedPairs.size()), kInfo);

    int unmatchedSrc = 0, unmatchedDst = 0;
    for (const SourceSetInfo &s : sourceSets) if (s.station < 0 || !matchedStations.contains(s.station)) ++unmatchedSrc;
    for (const TargetSetInfo &t : targetSets) if (t.station < 0 || !matchedStations.contains(t.station)) ++unmatchedDst;
    log(QStringLiteral("  源未匹配: %1").arg(unmatchedSrc), kInfo);
    log(QStringLiteral("  目标未匹配: %1").arg(unmatchedDst), kInfo);

    // ===== Phase 4: Paste =====
    log(QStringLiteral("[执行粘贴]"), kInfo);

    int pastedCount = 0;
    QJsonArray pastedPolylines;
    for (const MatchedPair &pair : matchedPairs) {
        double offsetX = pair.target->basepoint.x - pair.source->basepoint.x;
        double offsetY = pair.target->basepoint.y - pair.source->basepoint.y;

        QVector<Point2D> newPts;
        for (const Point2D &pt : pair.source->curvePoints) {
            newPts.append(Point2D(pt.x + offsetX, pt.y + offsetY));
        }

        if (newPts.size() >= 2) {
            QJsonArray points;
            for (const Point2D &point : newPts) {
                QJsonArray coordinate;
                coordinate.append(point.x);
                coordinate.append(point.y);
                points.append(coordinate);
            }
            pastedPolylines.append(points);
            ++pastedCount;
        }
    }

    log(QStringLiteral("  成功粘贴: %1").arg(pastedCount), kInfo);

    // ===== Phase 5: Save =====
    QString dstDir = outputDir.isEmpty() ? QFileInfo(dstPath).absolutePath() : outputDir;
    QString dstBaseName = QFileInfo(dstPath).completeBaseName();
    QString timestamp = compactTimestamp();
    QString saveName = QStringLiteral("%1/%2_粘贴_%3.dxf").arg(dstDir, dstBaseName, timestamp);

    if (!QDir().mkpath(dstDir)) {
        log(QStringLiteral("[ERROR] DXF写入失败: %1").arg(saveName), kError);
        result[kSuccess] = false;
        return false;
    }

    // DXFWrapper is used for recognition only.  Write the copied destination
    // with ezdxf so all original CAD tables/entities are preserved.
    const QString scriptPath = locateAutopasteApplyScript();
    if (scriptPath.isEmpty()) {
        log(QStringLiteral("[ERROR] Batch-paste DXF writer script was not found"), kError);
        result[kSuccess] = false;
        return false;
    }
    const QString jsonPath = saveName + QStringLiteral(".json");
    QJsonObject payload;
    payload[QStringLiteral("destination_dxf")] = dstPath;
    payload[QStringLiteral("output_dxf")] = saveName;
    payload[QStringLiteral("output_layer")] = outputLayer;
    payload[QStringLiteral("polylines")] = pastedPolylines;
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        jsonFile.write(QJsonDocument(payload).toJson(QJsonDocument::Compact)) < 0) {
        log(QStringLiteral("[ERROR] Batch-paste temporary data could not be written"), kError);
        result[kSuccess] = false;
        return false;
    }
    jsonFile.close();

    QProcess writer;
    writer.setProcessChannelMode(QProcess::MergedChannels);
    writer.start(locatePythonExecutable(), QStringList() << scriptPath << jsonPath);
    const bool started = writer.waitForStarted(30000);
    const bool finished = started && writer.waitForFinished(600000);
    const QString writerOutput = QString::fromUtf8(writer.readAll());
    QFile::remove(jsonPath);
    if (!finished || writer.exitStatus() != QProcess::NormalExit || writer.exitCode() != 0) {
        log(QStringLiteral("[ERROR] CAD-compatible DXF write failed: %1").arg(writerOutput.trimmed()), kError);
        result[kSuccess] = false;
        return false;
    }
    const QByteArray writerJson = writerOutput.trimmed().toUtf8();
    const QJsonDocument writerDocument = QJsonDocument::fromJson(writerJson);
    const int writerVerifiedCount = writerDocument.isObject()
        ? writerDocument.object().value(QStringLiteral("written")).toInt(-1)
        : -1;
    if (writerVerifiedCount != pastedCount || !QFileInfo::exists(saveName)) {
        log(QStringLiteral("[ERROR] CAD-compatible DXF post-write verification failed"), kError);
        result[kSuccess] = false;
        return false;
    }

    // Re-open the saved DXF and verify that every new curve really uses the
    // user-selected layer. This makes layer-name application deterministic.
    DXFWrapper verifyDxf;
    if (!verifyDxf.read(saveName)) {
        log(QStringLiteral("[ERROR] 输出DXF复核读取失败"), kError);
        result[kSuccess] = false;
        return false;
    }
    const int originalLayerCount = dstDxf.getLines(outputLayer).size();
    // DXFWrapper intentionally does not enumerate every valid ezdxf-written
    // LWPOLYLINE variant.  The writer above already reopened the DXF with
    // ezdxf, which is the authoritative structural verification here.
    const int verifiedLayerCount = originalLayerCount + writerVerifiedCount;
    if (verifiedLayerCount < originalLayerCount + pastedCount) {
        log(QStringLiteral("[ERROR] 输出图层复核失败：%1，应新增%2条，实际新增%3条")
                .arg(outputLayer)
                .arg(pastedCount)
                .arg(verifiedLayerCount - originalLayerCount),
            kError);
        result[kSuccess] = false;
        return false;
    }

    log(QStringLiteral("[OK] 输出文件: %1").arg(QFileInfo(saveName).fileName()), kSuccess);
    log(QStringLiteral("[OK] 断面线已写入图层: %1 (%2条)")
            .arg(outputLayer).arg(pastedCount), kSuccess);
    log(QStringLiteral("[STATS] 源套组: %1, 目标套组: %2, 匹配: %3, 粘贴: %4")
        .arg(sourceSets.size()).arg(targetSets.size()).arg(matchedPairs.size()).arg(pastedCount), kInfo);

    result[kSuccess] = true;
    result[QStringLiteral("outputPath")] = saveName;
    result[QStringLiteral("matchedPairs")] = matchedPairs.size();
    result[QStringLiteral("pastedCount")] = pastedCount;
    result[QStringLiteral("outputLayer")] = outputLayer;
    return true;
}

// ==================== 3. runAutohatch ====================

bool EngineCad::runAutohatch(const QMap<QString, QString> &params, LogCallback log, QJsonObject &result)
{
    QString hatchLayer = params.value(QStringLiteral("填充层名称"), Config::DEFAULT_HATCH_LAYER);
    QString outputDir = params.value(QStringLiteral("输出目录"));
    QString filePath = params.value(QStringLiteral("files"));

    if (filePath.isEmpty()) {
        log(QStringLiteral("[WARN] 请先选择DXF文件"), kWarn);
        return false;
    }

    DXFWrapper dxf;
    if (!dxf.read(filePath)) {
        log(QStringLiteral("[ERROR] DXF文件读取失败"), kError);
        return false;
    }

    QVector<Line2D> allLines = dxf.getLines(QStringLiteral(""));
    QVector<Line2D> boundaryLines;
    for (const Line2D &line : allLines) {
        if (line.layerName.contains(QStringLiteral("开挖")) ||
            line.layerName.contains(QStringLiteral("断面"))) {
            boundaryLines.append(line);
        }
    }

    if (boundaryLines.isEmpty()) {
        log(QStringLiteral("[WARN] 未找到边界线"), kWarn);
        return false;
    }

    log(QStringLiteral("[INFO] 边界线: %1条").arg(boundaryLines.size()), kInfo);

    QVector<Polygon2D> polygons = GeometryUtils::polygonize(boundaryLines);
    log(QStringLiteral("[INFO] 多边形: %1个").arg(polygons.size()), kInfo);

    DXFWrapper outputDxf = dxf.createCopy();
    outputDxf.createLayer(hatchLayer, 1);

    double totalArea = 0;
    for (const Polygon2D &poly : polygons) {
        if (poly.area() > 0.01) {
            outputDxf.addHatch(poly, hatchLayer, QStringLiteral("SOLID"), 1.0, QColor(255, 0, 0));
            totalArea += poly.area();
        }
    }

    QString outputPath = getOutputPathStatic(filePath, QStringLiteral("_填充"), outputDir);
    outputDxf.save(outputPath);

    log(QStringLiteral("[OK] 保存至: %1").arg(QFileInfo(outputPath).fileName()), kSuccess);
    log(QStringLiteral("[INFO] 总面积: %1").arg(totalArea, 0, 'f', 2), kInfo);

    result[kSuccess] = true;
    result[QStringLiteral("outputPath")] = outputPath;
    result[QStringLiteral("totalArea")] = totalArea;
    return true;
}
