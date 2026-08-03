#include "DXFWrapper.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString dxfPath = "D:/断面算量平台/测试文件/内湾段分层图（全航道底图20260331）2018_成套粘贴v2_20260422_113140.dxf";

    qDebug() << "===== 测试 DXFWrapper 图层解析 =====";
    qDebug() << "文件: " << dxfPath;

    DXFWrapper dxf;
    qDebug() << "开始读取...";

    QElapsedTimer timer;
    timer.start();

    bool success = dxf.read(dxfPath);

    qDebug() << "读取耗时: " << timer.elapsed() << "ms";
    qDebug() << "读取成功: " << success;

    if (success) {
        qDebug() << "\n=== 图层列表 ===";
        QStringList layers = dxf.getLayers();
        qDebug() << "图层数量: " << layers.size();

        QMap<QString, DXFWrapper::LayerInfo> layerInfo = dxf.getAllLayerInfo();
        qDebug() << "图层属性数量: " << layerInfo.size();

        // 打印前10个图层及其属性
        int count = 0;
        for (const QString &layer : layers) {
            if (count++ >= 10) break;

            DXFWrapper::LayerInfo info = dxf.getLayerInfo(layer);
            qDebug() << QString("  %1: color=%2, linetype=%3, lineweight=%4")
                .arg(layer)
                .arg(info.color)
                .arg(info.linetype)
                .arg(info.lineweight);
        }

        qDebug() << "\n=== 实体统计 ===";
        qDebug() << "线段数量: " << dxf.getLines("").size();
        qDebug() << "文本数量: " << dxf.getTexts("").size();
        qDebug() << "填充数量: " << dxf.getHatches("").size();
    }

    return 0;
}