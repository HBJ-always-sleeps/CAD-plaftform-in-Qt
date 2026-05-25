#ifndef OUTPUT_HELPER_H
#define OUTPUT_HELPER_H

#include "Geometry.h"
#include <QString>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

/**
 * OutputHelper类 - 复刻Python engine_cad_v3.py OutputHelper类
 *
 * 文件输出工具集：
 * - get_output_path: 生成输出文件路径
 * - ensure_layer: 确保图层存在
 *
 * 注意：所有输出文件名添加QtTest前缀，便于区分和清理测试生成文件
 */
class OutputHelper
{
public:
    /**
     * Qt测试生成文件的前缀标记
     * 使用inline static确保可以正确初始化
     */
    inline static const QString QT_TEST_PREFIX = QStringLiteral("QtTest_");

    /**
     * 生成输出文件路径
     */
    static QString getOutputPath(const QString &inputPath,
                                  const QString &suffix,
                                  const QString &outputDir = QString()) {
        QString baseDir = outputDir.isEmpty()
            ? QFileInfo(inputPath).absolutePath()
            : outputDir;

        QString baseName = QFileInfo(inputPath).completeBaseName();

        // 添加QtTest前缀标记
        if (suffix.endsWith(".dxf") || suffix.endsWith(".xlsx")) {
            return QString("%1/%2%3%4").arg(baseDir, QT_TEST_PREFIX, baseName, suffix);
        }

        return QString("%1/%2%3%4.dxf").arg(baseDir, QT_TEST_PREFIX, baseName, suffix);
    }

    /**
     * 生成带时间戳的输出文件路径
     */
    static QString getOutputPathWithTimestamp(const QString &inputPath,
                                               const QString &suffix,
                                               const QString &outputDir = QString()) {
        QString baseDir = outputDir.isEmpty()
            ? QFileInfo(inputPath).absolutePath()
            : outputDir;

        QString baseName = QFileInfo(inputPath).completeBaseName();
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

        // 添加QtTest前缀标记
        if (suffix.endsWith(".dxf") || suffix.endsWith(".xlsx")) {
            return QString("%1/%2%3%4_%5").arg(baseDir, QT_TEST_PREFIX, baseName, suffix, timestamp);
        }

        return QString("%1/%2%3%4_%5.dxf").arg(baseDir, QT_TEST_PREFIX, baseName, suffix, timestamp);
    }

    /**
     * 构建输出文件名（分层算量专用）
     */
    static QString buildSectionOutputName(const QString &baseName,
                                          double targetElevation,
                                          const QString &mode,
                                          const QString &timestamp) {
        QString modeSuffix = (mode == "below") ? QStringLiteral("以下") : QStringLiteral("以上");

        // 添加QtTest前缀标记
        if (targetElevation != 0) {
            return QString("%1%2_%3m%4分层_%5.dxf")
                .arg(QT_TEST_PREFIX)
                .arg(baseName)
                .arg(targetElevation)
                .arg(modeSuffix)
                .arg(timestamp);
        } else {
            return QString("%1%2_全算量_%3.dxf")
                .arg(QT_TEST_PREFIX)
                .arg(baseName)
                .arg(timestamp);
        }
    }

    /**
     * 构建回淤输出文件名
     */
    static QString buildBackfillOutputName(const QString &baseName,
                                           const QString &timestamp) {
        // 添加QtTest前缀标记
        return QString("%1%2_回淤_%3.dxf")
            .arg(QT_TEST_PREFIX)
            .arg(baseName)
            .arg(timestamp);
    }

    /**
     * 构建合并任务输出文件名
     */
    static QString buildCombinedOutputName(const QString &baseName,
                                           double targetElevation,
                                           const QString &mode,
                                           const QString &timestamp) {
        QString modeSuffix = (mode == "below") ? QStringLiteral("以下") : QStringLiteral("以上");

        // 添加QtTest前缀标记
        if (targetElevation != 0) {
            return QString("%1%2_%3m%4分层回淤_%5.dxf")
                .arg(QT_TEST_PREFIX)
                .arg(baseName)
                .arg(targetElevation)
                .arg(modeSuffix)
                .arg(timestamp);
        } else {
            return QString("%1%2_全算量分层回淤_%3.dxf")
                .arg(QT_TEST_PREFIX)
                .arg(baseName)
                .arg(timestamp);
        }
    }
};

#endif // OUTPUT_HELPER_H