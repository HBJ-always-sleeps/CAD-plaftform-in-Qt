#include "ExcelExporter.h"

/**
 * ExcelExporter.cpp - 数据导出实现
 *
 * 使用CSV格式导出，不依赖外部库
 * 支持导出分层算量、回淤面积、合并任务结果
 */

// 静态方法已在头文件中实现，此处为编译兼容

#ifdef USE_QTXLSX

// QtXlsx集成代码将在此时启用
// 参见 LIBRARY_INSTALL_GUIDE.md
// 需要安装QtXlsxWriter库后启用USE_QTXLSX宏

#endif