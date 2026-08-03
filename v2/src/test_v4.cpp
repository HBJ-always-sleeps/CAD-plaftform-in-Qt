#include "EngineCad.h"
#include "Config.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <cstdio>

static void messageHandler(QtMsgType, const QMessageLogContext &, const QString &msg) {
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);

    EngineCad engine;

    qDebug() << "========================================";
    qDebug() << "V4 分层算量测试";
    qDebug() << "========================================";

    QMap<QString, QString> params;
    params["files"] = "D:/断面算量平台/测试文件/内湾段分层图20260516_已粘贴断面_20260622_152702_下包络合并_分层回淤合并_20260622_153251.dxf";
    params["目标高程"] = "0";
    params["断面线图层"] = "V4";
    params["桩号图层"] = "0-桩号";
    params["区分设计超挖"] = "true";
    params["计算模式"] = "below";
    params["输出目录"] = "D:/断面算量平台/测试文件";

    auto logFunc = [](const QString &msg, const QString &level) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        fprintf(stderr, "[%s] %s\n", timestamp.toLocal8Bit().constData(), msg.toLocal8Bit().constData());
    };

    QJsonObject result;
    bool success = engine.runAutosection(params, logFunc, result);

    qDebug() << "";
    qDebug() << "========================================";
    qDebug() << "结果: " << (success ? "成功" : "失败");
    if (success) {
        qDebug() << "DXF输出: " << result["outputPath"].toString();
        qDebug() << "Excel输出: " << result["xlsxPath"].toString();
        qDebug() << "总面积: " << result["totalArea"].toDouble();
    } else {
        qDebug() << "错误: " << result["error"].toString();
    }
    qDebug() << "========================================";

    return success ? 0 : 1;
}
