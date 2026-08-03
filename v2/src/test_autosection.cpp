#include "EngineCad.h"
#include "Config.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <cstdio>

static void messageHandler(QtMsgType, const QMessageLogContext &, const QString &msg) {
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
}

static QString envOrDefault(const char *name, const QString &fallback)
{
    const QString value = qEnvironmentVariable(name).trimmed();
    return value.isEmpty() ? fallback : value;
}

bool runTest1(EngineCad &engine) {
    // 测试1：全算量分层+回淤（合并下包络，区分设计超挖）
    qDebug() << "";
    qDebug() << "===== 测试1：全算量分层+回淤 =====";

    QMap<QString, QString> params;
    QString outputDir = qEnvironmentVariable("QT_CAD_TEST_OUTPUT_DIR");
    if (outputDir.isEmpty())
        outputDir = QStringLiteral("D:/断面算量平台/测试文件");
    params["files"] = envOrDefault("QT_CAD_TEST_INPUT", QStringLiteral("D:/断面算量平台/测试文件/202511.dxf"));
    params["目标高程"] = qEnvironmentVariable("QT_CAD_TEST_ELEVATION");  // 留空=全算量
    params["设计断面线图层"] = envOrDefault("QT_CAD_TEST_DESIGN_LAYER", QStringLiteral("DMX"));
    params["更新断面线图层"] = envOrDefault("QT_CAD_TEST_UPDATE_LAYER", QStringLiteral("202511"));
    params["桩号图层"] = envOrDefault("QT_CAD_TEST_PILE_LAYER", QStringLiteral("0-桩号"));
    params["合并断面线"] = "true";  // 合并下包络
    params["延长超挖线"] =
        envOrDefault("QT_CAD_TEST_EXTEND_OVEREXC", QStringLiteral("false"));
    params["区分设计超挖"] = "true";  // 区分设计超挖量
    params["计算模式"] = "below";
    params["输出目录"] = outputDir;

    auto logFunc = [](const QString &msg, const QString &level) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        qDebug().noquote() << QString("[%1] %2").arg(timestamp, msg);
    };

    qDebug() << "文件: " << params["files"];
    qDebug() << "断面线图层: " << params["设计断面线图层"];
    qDebug() << "合并下包络: " << params["合并断面线"];
    qDebug() << "区分设计超挖: " << params["区分设计超挖"];
    qDebug() << "延长超挖线: " << params["延长超挖线"];

    QJsonObject result;
    bool success = engine.runAutosectionBackfill(params, logFunc, result);

    qDebug() << "测试1结果: " << (success ? "成功" : "失败");
    if (success) {
        qDebug() << "DXF输出: " << result["outputPath"].toString();
        qDebug() << "Excel输出: " << result["xlsxPath"].toString();
    }
    return success;
}

bool runTest2(EngineCad &engine) {
    // 测试2：分层算量（高程-4m以上面积）- 纯分层，不含回淤
    qDebug() << "";
    qDebug() << "===== 测试2：分层算量（-4.0m以上面积） =====";
    qDebug() << "注意：此测试仅计算分层面积，不包含回淤计算";

    QMap<QString, QString> params;
    QString outputDir = qEnvironmentVariable("QT_CAD_TEST_OUTPUT_DIR");
    if (outputDir.isEmpty())
        outputDir = QStringLiteral("D:/断面算量平台/测试文件");
    params["files"] = "D:/断面算量平台/测试文件/202511.dxf";
    params["目标高程"] = "-4.0";  // 高程-4米
    params["设计断面线图层"] = "DMX";  // 原始断面线
    params["桩号图层"] = "0-桩号";
    params["合并断面线"] = "true";
    params["区分设计超挖"] = "true";
    params["计算模式"] = "above";  // 算高程线以上的面积
    params["输出目录"] = outputDir;

    auto logFunc = [](const QString &msg, const QString &level) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        qDebug().noquote() << QString("[%1] %2").arg(timestamp, msg);
    };

    qDebug() << "文件: " << params["files"];
    qDebug() << "目标高程: " << params["目标高程"];
    qDebug() << "计算模式: " << params["计算模式"] << "(高程线以上)";
    qDebug() << "使用函数: runAutosection (不含回淤)";

    QJsonObject result;
    // 使用runAutosection而非runAutosectionBackfill
    bool success = engine.runAutosection(params, logFunc, result);

    qDebug() << "测试2结果: " << (success ? "成功" : "失败");
    if (success) {
        qDebug() << "DXF输出: " << result["outputPath"].toString();
        qDebug() << "Excel输出: " << result["xlsxPath"].toString();
        qDebug() << "总面积: " << result["totalArea"].toDouble() << "㎡";
    }
    return success;
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(messageHandler);
    QCoreApplication app(argc, argv);

    EngineCad engine;

    qDebug() << "========================================";
    qDebug() << "Qt C++ 版本 autosection_backfill 测试";
    qDebug() << "对比 Python 原版输出";
    qDebug() << "========================================";

    QString mode = (argc > 1) ? QString::fromLocal8Bit(argv[1]).toLower() : QStringLiteral("all");
    bool success = true;
    if (mode == QStringLiteral("all") || mode == QStringLiteral("test1"))
        success = runTest1(engine) && success;
    if (mode == QStringLiteral("all") || mode == QStringLiteral("test2"))
        success = runTest2(engine) && success;

    qDebug() << "";
    qDebug() << "========================================";
    qDebug() << "测试完成，请对比输出文件";
    qDebug() << "========================================";

    return success ? 0 : 1;
}
