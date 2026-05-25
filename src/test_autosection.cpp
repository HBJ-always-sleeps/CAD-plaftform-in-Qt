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

void runTest1(EngineCad &engine) {
    // 测试1：全算量分层+回淤（合并下包络，区分设计超挖）
    qDebug() << "";
    qDebug() << "===== 测试1：全算量分层+回淤 =====";

    QMap<QString, QString> params;
    params["files"] = "D:/断面算量平台/测试文件/202511.dxf";
    params["目标高程"] = "0";  // 全算量
    params["设计断面线图层"] = "DMX";  // 原始断面线（回淤下边界）
    params["更新断面线图层"] = "202511";  // 更新断面线（回淤上边界）
    params["桩号图层"] = "0-桩号";
    params["合并断面线"] = "true";  // 合并下包络
    params["区分设计超挖"] = "true";  // 区分设计超挖量
    params["计算模式"] = "below";
    params["输出目录"] = "D:/断面算量平台/测试文件";

    auto logFunc = [](const QString &msg, const QString &level) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        qDebug().noquote() << QString("[%1] %2").arg(timestamp, msg);
    };

    qDebug() << "文件: " << params["files"];
    qDebug() << "断面线图层: " << params["设计断面线图层"];
    qDebug() << "合并下包络: " << params["合并断面线"];
    qDebug() << "区分设计超挖: " << params["区分设计超挖"];

    QJsonObject result;
    bool success = engine.runAutosectionBackfill(params, logFunc, result);

    qDebug() << "测试1结果: " << (success ? "成功" : "失败");
    if (success) {
        qDebug() << "DXF输出: " << result["outputPath"].toString();
        qDebug() << "Excel输出: " << result["xlsxPath"].toString();
    }
}

void runTest2(EngineCad &engine) {
    // 测试2：分层算量（高程-4m以上面积）- 纯分层，不含回淤
    qDebug() << "";
    qDebug() << "===== 测试2：分层算量（-4.0m以上面积） =====";
    qDebug() << "注意：此测试仅计算分层面积，不包含回淤计算";

    QMap<QString, QString> params;
    params["files"] = "D:/断面算量平台/测试文件/202511.dxf";
    params["目标高程"] = "-4.0";  // 高程-4米
    params["设计断面线图层"] = "DMX";  // 原始断面线
    params["桩号图层"] = "0-桩号";
    params["合并断面线"] = "true";
    params["区分设计超挖"] = "true";
    params["计算模式"] = "above";  // 算高程线以上的面积
    params["输出目录"] = "D:/断面算量平台/测试文件";

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
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    EngineCad engine;

    qDebug() << "========================================";
    qDebug() << "Qt C++ 版本 autosection_backfill 测试";
    qDebug() << "对比 Python 原版输出";
    qDebug() << "========================================";

    // 执行两个测试
    runTest1(engine);
    runTest2(engine);

    qDebug() << "";
    qDebug() << "========================================";
    qDebug() << "测试完成，请对比输出文件";
    qDebug() << "========================================";

    return 0;
}