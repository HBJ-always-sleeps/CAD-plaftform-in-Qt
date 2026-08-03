#include <QApplication>
#include "MainWindow.h"

/**
 * 航道断面算量自动化平台 Qt C++版本 v4.1.0
 *
 * 主入口程序
 *
 * 编译说明：
 * 1. 确保已安装 Qt 6.x 开发环境
 * 2. 使用 cmake 编译：
 *    mkdir build && cd build
 *    cmake ..
 *    cmake --build .
 */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");

    MainWindow window;
    window.show();

    return app.exec();
}
