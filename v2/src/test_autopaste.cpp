#include "EngineCad.h"
#include "DXFWrapper.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QFileInfo>
#include <QDateTime>
#include <cstdio>
#include <locale>
#include <windows.h>

static void messageHandler(QtMsgType, const QMessageLogContext &, const QString &msg) {
    QByteArray utf8 = msg.toUtf8();
    fwrite(utf8.constData(), 1, utf8.size(), stderr);
    fputc('\n', stderr);
}

int main(int argc, char *argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF-8");
    qInstallMessageHandler(messageHandler);
    QCoreApplication app(argc, argv);

    EngineCad engine;
    const QString customLayer = QStringLiteral("测试自定义粘贴断面");
    bool allPassed = true;

    QString dstPath = "D:/断面算量平台/测试文件/平台专用测试/autopaste_target.dxf";

    QStringList srcPaths = {
        "D:/断面算量平台/测试文件/平台专用测试/autopaste_source.dxf",
        "D:/断面算量平台/测试文件/平台专用测试/批量粘贴_源文件.dxf"
    };

    auto logFunc = [](const QString &msg, const QString &level) {
        Q_UNUSED(level);
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        QString line = QStringLiteral("[%1] %2").arg(timestamp, msg);
        QByteArray utf8 = line.toUtf8();
        fwrite(utf8.constData(), 1, utf8.size(), stderr);
        fputc('\n', stderr);
    };

    for (const QString &srcPath : srcPaths) {
        qDebug() << "";
        qDebug() << "============================================================";
        qDebug() << "测试: " << QFileInfo(srcPath).fileName() << " -> " << QFileInfo(dstPath).fileName();
        qDebug() << "============================================================";

        QMap<QString, QString> params;
        params["源文件名"] = srcPath;
        params["目标文件名"] = dstPath;
        params["输出图层名"] = customLayer;
        params["输出目录"] = QStringLiteral("D:/QtCADPlatform/v2/test_output/autopaste_layer");

        QJsonObject result;
        bool success = engine.runAutopaste(params, logFunc, result);

        qDebug() << "结果: " << (success ? "成功" : "失败");
        if (success) {
            qDebug() << "输出文件: " << result["outputPath"].toString();
            qDebug() << "匹配对数: " << result["matchedPairs"].toInt();
            qDebug() << "粘贴数量: " << result["pastedCount"].toInt();
            DXFWrapper verify;
            const bool readOk = verify.read(result["outputPath"].toString());
            const int layerCount = readOk ? verify.getLines(customLayer).size() : 0;
            qDebug() << "自定义图层: " << customLayer << "线数量:" << layerCount;
            if (result["pastedCount"].toInt() <= 0) {
                qDebug() << "自定义图层复核失败";
                allPassed = false;
            }
        } else {
            allPassed = false;
        }
    }

    return allPassed ? 0 : 1;
}
