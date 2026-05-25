#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString dxfPath = "D:/断面算量平台/测试文件/内湾段分层图（全航道底图20260331）2018_成套粘贴v2_20260422_113140.dxf";

    qDebug() << "===== 简化版DXF读取测试 =====";
    qDebug() << "文件: " << dxfPath;

    QFile file(dxfPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件";
        return 1;
    }

    QByteArray rawData = file.readAll();
    QString content = QString::fromLocal8Bit(rawData);
    file.close();

    QStringList allLines = content.split('\n');
    qDebug() << "总行数: " << allLines.size();

    QElapsedTimer timer;
    timer.start();

    // 简化解析 - 只统计实体数量
    int lineCount = 0;
    int lwpolylineCount = 0;
    int textCount = 0;
    int hatchCount = 0;
    int layerCount = 0;
    QString currentEntity = "";
    int lastPrintTime = 0;

    for (int i = 0; i < allLines.size() - 1; i++) {
        QString code = allLines[i].trimmed();
        QString value = allLines[i + 1].trimmed();
        int codeNum = code.toInt();

        // 每1000行打印一次进度
        if (timer.elapsed() - lastPrintTime > 500) {
            qDebug() << QString("进度: %1/%2 行, 耗时: %3ms")
                .arg(i).arg(allLines.size()).arg(timer.elapsed());
            lastPrintTime = timer.elapsed();
        }

        if (codeNum == 0) {
            if (value == "LINE") {
                lineCount++;
                currentEntity = "LINE";
            }
            else if (value == "LWPOLYLINE") {
                lwpolylineCount++;
                currentEntity = "LWPOLYLINE";
            }
            else if (value == "TEXT" || value == "MTEXT") {
                textCount++;
                currentEntity = value.toUpper();
            }
            else if (value == "HATCH") {
                hatchCount++;
                currentEntity = "HATCH";
            }
            else if (value == "LAYER") {
                layerCount++;
            }
            else {
                currentEntity = "";
            }
        }
    }

    qDebug() << "\n===== 解析完成 =====";
    qDebug() << "总耗时: " << timer.elapsed() << "ms";
    qDebug() << "LINE数量: " << lineCount;
    qDebug() << "LWPOLYLINE数量: " << lwpolylineCount;
    qDebug() << "TEXT数量: " << textCount;
    qDebug() << "HATCH数量: " << hatchCount;
    qDebug() << "LAYER数量: " << layerCount;

    return 0;
}