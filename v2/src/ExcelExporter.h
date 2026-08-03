#ifndef EXCEL_EXPORTER_H
#define EXCEL_EXPORTER_H

#include "Config.h"
#include <QString>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDebug>

/**
 * ExcelExporter类 - 数据导出工具
 *
 * 支持导出格式：
 * - CSV格式（不需要外部库）
 * - Excel格式（需要QtXlsx库，可选）
 *
 * 导出内容：
 * - 分层算量结果（设计量/超挖量/总量）
 * - 回淤面积结果
 * - 地层汇总
 * - 总汇总表
 */
class ExcelExporter
{
public:
    /**
     * 导出分层算量结果到CSV
     */
    static bool exportAutosectionToCSV(const QString &outputPath,
                                        const QVector<QMap<QString, QVariant>> &results,
                                        const QStringList &strataLayers,
                                        bool distinguishDesign,
                                        double targetElevation,
                                        const QString &calcMode) {

        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Cannot create output file:" << outputPath;
            return false;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        QString modeText = (calcMode == "below") ? "以下" : "以上";
        QString elevStr = (targetElevation != 0) ?
            QString("%1m%2").arg(targetElevation).arg(modeText) : "全算量";

        // 写标题行
        out << "航道断面算量自动化平台 - 分层算量结果\n";
        out << QString("目标高程: %1m\n").arg(targetElevation);
        out << QString("计算模式: %1\n").arg(calcMode);
        out << "\n";

        if (distinguishDesign) {
            // Sheet1: 设计量
            out << "=== 设计量 ===\n";
            writeCSVHeader(out, strataLayers);
            for (const QMap<QString, QVariant> &r : results) {
                writeCSVRow(out, r, strataLayers, "_设计");
            }
            out << "\n";

            // Sheet2: 超挖量
            out << "=== 超挖量 ===\n";
            writeCSVHeader(out, strataLayers);
            for (const QMap<QString, QVariant> &r : results) {
                writeCSVRow(out, r, strataLayers, "_超挖");
            }
            out << "\n";

            // Sheet3: 总量
            out << "=== 总量 ===\n";
            writeCSVHeader(out, strataLayers);
            for (const QMap<QString, QVariant> &r : results) {
                writeCSVRowTotal(out, r, strataLayers);
            }
            out << "\n";
        } else {
            // 明细表
            out << "=== 明细表 ===\n";
            writeCSVHeader(out, strataLayers);
            for (const QMap<QString, QVariant> &r : results) {
                writeCSVRow(out, r, strataLayers, "");
            }
            out << "\n";
        }

        // 地层汇总
        out << "=== 地层汇总 ===\n";
        out << "地层,面积(m2)\n";
        for (const QString &layer : strataLayers) {
            double total = 0;
            for (const QMap<QString, QVariant> &r : results) {
                QString key = distinguishDesign ? layer + "_设计" : layer;
                total += r.value(key).toDouble();
                if (distinguishDesign) {
                    total += r.value(layer + "_超挖").toDouble();
                }
            }
            out << QString("%1,%2\n").arg(layer).arg(total, 0, 'f', 3);
        }
        out << "\n";

        // 总汇总
        out << "=== 总汇总 ===\n";
        out << "统计项,数值\n";
        out << QString("总断面数,%1\n").arg(results.size());

        double totalArea = 0;
        for (const QMap<QString, QVariant> &r : results) {
            totalArea += r.value("总面积").toDouble();
        }
        out << QString("%1总面积,%2\n").arg(elevStr).arg(totalArea, 0, 'f', 3);

        file.close();
        qDebug() << "CSV exported:" << outputPath;
        return true;
    }

    /**
     * 导出回淤面积结果到CSV
     */
    static bool exportBackfillToCSV(const QString &outputPath,
                                     const QVector<QMap<QString, QVariant>> &results) {

        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Cannot create output file:" << outputPath;
            return false;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        // 标题
        out << "航道断面算量自动化平台 - 回淤面积结果\n\n";

        // 回淤面积汇总
        out << "=== 回淤面积汇总 ===\n";
        out << "桩号,回淤面积(m2)\n";
        for (const QMap<QString, QVariant> &r : results) {
            out << QString("%1,%2\n")
                .arg(r.value("桩号").toString())
                .arg(r.value("回淤面积").toDouble(), 0, 'f', 2);
        }
        out << "\n";

        // 带合计
        out << "=== 带合计 ===\n";
        out << "桩号,回淤面积(m2)\n";
        double totalBackfill = 0;
        for (const QMap<QString, QVariant> &r : results) {
            double area = r.value("回淤面积").toDouble();
            totalBackfill += area;
            out << QString("%1,%2\n")
                .arg(r.value("桩号").toString())
                .arg(area, 0, 'f', 2);
        }
        out << QString("合计,%1\n").arg(totalBackfill, 0, 'f', 2);

        file.close();
        qDebug() << "Backfill CSV exported:" << outputPath;
        return true;
    }

    /**
     * 导出合并任务结果到CSV
     */
    static bool exportCombinedToCSV(const QString &outputPath,
                                     const QVector<QMap<QString, QVariant>> &sectionResults,
                                     const QVector<QMap<QString, QVariant>> &backfillResults,
                                     const QStringList &strataLayers,
                                     bool distinguishDesign,
                                     double targetElevation,
                                     const QString &calcMode) {

        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Cannot create output file:" << outputPath;
            return false;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        QString modeText = (calcMode == "below") ? "以下" : "以上";
        QString elevStr = (targetElevation != 0) ?
            QString("%1m%2").arg(targetElevation).arg(modeText) : "全算量";

        // 标题
        out << "航道断面算量自动化平台 - 分层算量+回淤计算合并结果\n";
        out << QString("目标高程: %1m\n").arg(targetElevation);
        out << QString("计算模式: %1\n").arg(calcMode);
        out << "\n";

        // 合并明细表
        out << "=== 合并明细表 ===\n";
        QStringList headers;
        headers << "断面名称";
        for (const QString &layer : strataLayers) {
            if (distinguishDesign) {
                headers << layer + "_设计" << layer + "_超挖";
            } else {
                headers << layer;
            }
        }
        headers << "总面积" << "回淤面积";
        out << headers.join(",") << "\n";

        for (const QMap<QString, QVariant> &r : sectionResults) {
            QStringList row;
            row << r.value("断面名称").toString();
            for (const QString &layer : strataLayers) {
                if (distinguishDesign) {
                    row << QString::number(r.value(layer + "_设计").toDouble(), 'f', 3);
                    row << QString::number(r.value(layer + "_超挖").toDouble(), 'f', 3);
                } else {
                    row << QString::number(r.value(layer).toDouble(), 'f', 3);
                }
            }
            row << QString::number(r.value("总面积").toDouble(), 'f', 3);

            // 查找对应的回淤面积
            QString station = r.value("断面名称").toString();
            double backfillArea = 0;
            for (const QMap<QString, QVariant> &b : backfillResults) {
                if (b.value("桩号").toString() == station) {
                    backfillArea = b.value("回淤面积").toDouble();
                    break;
                }
            }
            row << QString::number(backfillArea, 'f', 2);

            out << row.join(",") << "\n";
        }
        out << "\n";

        // 地层汇总
        out << "=== 地层汇总 ===\n";
        out << "地层,设计面积(m2),超挖面积(m2),总面积(m2)\n";
        for (const QString &layer : strataLayers) {
            double designTotal = 0, overTotal = 0;
            for (const QMap<QString, QVariant> &r : sectionResults) {
                designTotal += r.value(layer + "_设计").toDouble();
                overTotal += r.value(layer + "_超挖").toDouble();
            }
            double layerTotal = designTotal + overTotal;
            if (distinguishDesign) {
                out << QString("%1,%2,%3,%4\n")
                    .arg(layer)
                    .arg(designTotal, 0, 'f', 3)
                    .arg(overTotal, 0, 'f', 3)
                    .arg(layerTotal, 0, 'f', 3);
            } else {
                out << QString("%1,%2\n").arg(layer).arg(layerTotal, 0, 'f', 3);
            }
        }
        out << "\n";

        // 总汇总
        out << "=== 总汇总 ===\n";
        out << "统计项,数值\n";
        out << QString("总断面数,%1\n").arg(sectionResults.size());

        double totalSectionArea = 0;
        for (const QMap<QString, QVariant> &r : sectionResults) {
            totalSectionArea += r.value("总面积").toDouble();
        }
        out << QString("%1总面积,%2\n").arg(elevStr).arg(totalSectionArea, 0, 'f', 3);

        double totalBackfill = 0;
        for (const QMap<QString, QVariant> &r : backfillResults) {
            totalBackfill += r.value("回淤面积").toDouble();
        }
        out << QString("总回淤面积,%1\n").arg(totalBackfill, 0, 'f', 2);

        file.close();
        qDebug() << "Combined CSV exported:" << outputPath;
        return true;
    }

    /**
     * 导出快速填充面积结果到CSV
     */
    static bool exportAutohatchToCSV(const QString &outputPath,
                                      const QVector<QMap<QString, QVariant>> &results) {

        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Cannot create output file:" << outputPath;
            return false;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        out << "航道断面算量自动化平台 - 快速填充面积结果\n\n";
        out << "编号,面积(m2)\n";

        for (const QMap<QString, QVariant> &r : results) {
            out << QString("%1,%2\n")
                .arg(r.value("编号").toInt())
                .arg(r.value("面积").toDouble(), 0, 'f', 3);
        }

        file.close();
        qDebug() << "Autohatch CSV exported:" << outputPath;
        return true;
    }

    /**
     * 生成输出文件名
     */
    static QString generateFilename(const QString &baseName,
                                     const QString &suffix,
                                     const QString &timestamp) {
        return QString("%1_%2_%3.csv").arg(baseName).arg(suffix).arg(timestamp);
    }

private:
    /**
     * 写CSV表头
     */
    static void writeCSVHeader(QTextStream &out, const QStringList &strataLayers) {
        out << "断面名称";
        for (const QString &layer : strataLayers) {
            out << "," << layer;
        }
        out << ",总面积\n";
    }

    /**
     * 写CSV数据行
     */
    static void writeCSVRow(QTextStream &out, const QMap<QString, QVariant> &r,
                            const QStringList &strataLayers, const QString &suffix = "") {
        out << r.value("断面名称").toString();
        for (const QString &layer : strataLayers) {
            QString key = layer + suffix;
            double value = r.value(key).toDouble();
            out << QString(",%1").arg(value, 0, 'f', 3);
        }
        out << QString(",%1\n").arg(r.value("总面积").toDouble(), 0, 'f', 3);
    }

    /**
     * 写CSV总量行（设计+超挖）
     */
    static void writeCSVRowTotal(QTextStream &out, const QMap<QString, QVariant> &r,
                                 const QStringList &strataLayers) {
        out << r.value("断面名称").toString();
        for (const QString &layer : strataLayers) {
            double total = r.value(layer + "_设计").toDouble() + r.value(layer + "_超挖").toDouble();
            out << QString(",%1").arg(total, 0, 'f', 3);
        }
        out << QString(",%1\n").arg(r.value("总面积").toDouble(), 0, 'f', 3);
    }
};

#endif // EXCEL_EXPORTER_H