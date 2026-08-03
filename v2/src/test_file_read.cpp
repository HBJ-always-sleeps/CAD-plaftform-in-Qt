#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString dxfPath = "D:/断面算量平台/测试文件/内湾段分层图（全航道底图20260331）2018_成套粘贴v2_20260422_113140.dxf";

    qDebug() << "===== 文件读取速度测试 =====";
    qDebug() << "文件: " << dxfPath;

    QFile file(dxfPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件";
        return 1;
    }

    QElapsedTimer timer;
    timer.start();

    qDebug() << "开始读取全部字节...";
    QByteArray rawData = file.readAll();
    qDebug() << "读取完成，耗时: " << timer.elapsed() << "ms";
    qDebug() << "数据大小: " << rawData.size() << "bytes";

    file.close();

    timer.restart();
    qDebug() << "开始转换为QString (fromLocal8Bit)...";
    QString content = QString::fromLocal8Bit(rawData);
    qDebug() << "转换完成，耗时: " << timer.elapsed() << "ms";

    timer.restart();
    qDebug() << "开始按换行分割...";
    QStringList lines = content.split('\n');
    qDebug() << "分割完成，耗时: " << timer.elapsed() << "ms";
    qDebug() << "行数: " << lines.size();

    qDebug() << "===== 测试完成 =====";
    return 0;
}