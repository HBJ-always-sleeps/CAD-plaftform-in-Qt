#ifndef CONFIG_H
#define CONFIG_H

#include <QtGlobal>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QColor>
#include <QVector>
#include <QPair>
#include <QRegularExpression>

/**
 * 全局配置类 - 复刻Python engine_cad_v3.py Config类
 * 
 * 包含：
 * - 默认图层名称
 * - 高对比度颜色列表
 * - 地层颜色映射
 * - 任务配置信息
 */
class Config
{
public:
    // ==================== 默认图层 ====================
    static const QString DEFAULT_OUTPUT_LAYER;
    static const QString DEFAULT_HATCH_LAYER;
    static const QString DEFAULT_FINAL_SECTION;
    
    // ==================== 高对比度颜色列表 ====================
    // 用于填充和标注的12种高对比度颜色
    static const QVector<QColor> HIGH_CONTRAST_COLORS;
    
    // ==================== 地层颜色映射 ====================
    // 地层名称 -> DXF颜色索引
    static const QMap<QString, int> STRATA_COLORS;
    
    // ==================== 任务配置 ====================
    struct TaskConfig {
        QString name;        // 任务名称（中文）
        QString desc;        // 任务描述
    };
    
    static const QMap<QString, TaskConfig> TOOL_CONFIG;
    
    // ==================== 几何参数 ====================
    static constexpr double AREA_SCALE_FACTOR = 1.0;
    static constexpr double DEFAULT_TEXT_HEIGHT = 3.0;
    static constexpr double HATCH_SCALE_FACTOR = 0.02;
    
    // ==================== 断面检测参数 ====================
    static constexpr double SECTION_WIDTH_MIN = 130.0;   // 小矩形宽度下限
    static constexpr double SECTION_WIDTH_MAX = 200.0;   // 小矩形宽度上限
    static constexpr double SECTION_HEIGHT_MIN = 95.0;   // 小矩形高度下限
    static constexpr double SECTION_HEIGHT_MAX = 140.0;  // 小矩形高度上限
    static constexpr double CURVE_VERTEX_THRESHOLD = 50; // 断面曲线顶点数阈值
    
    // ==================== 匹配参数 ====================
    static constexpr double STATION_MATCH_TOLERANCE = 500.0;  // 桩号匹配容差
    static constexpr double LINE_DISTANCE_THRESHOLD = 0.5;    // 线段距离阈值
    static constexpr double BASEPOINT_Y_TOLERANCE = 50.0;     // 基点Y容差
    static constexpr double BP_X_GROUP_TOLERANCE = 50.0;      // 基点X分组容差
    
    // ==================== 桩号解析正则 ====================
    static QString STATION_PATTERN() {
        return QStringLiteral("(\\d+\\+\\d+)");
    }
    
    // ==================== 标尺图层 ====================
    static QStringList RULER_LAYERS() {
        return {QStringLiteral("标尺"), QStringLiteral("0-标尺"), QStringLiteral("RULER")};
    }
    
    // ==================== 辅助函数 ====================
    
    /**
     * 桩号排序键 - 将桩号字符串转换为数值
     * 例如: "67+400" -> 67400
     */
    static int stationSortKey(const QString &stationStr) {
        QRegularExpression re(QStringLiteral("\\d+"));
        QRegularExpressionMatchIterator it = re.globalMatch(stationStr);
        int result = 0;
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            result = result * 1000 + match.captured().toInt();
        }
        return result;
    }
    
    /**
     * 地层排序键 - 提取地层级别数字
     * 例如: "3级淤泥" -> 3
     */
    static int strataSortKey(const QString &strataName) {
        QRegularExpression re(QStringLiteral("^\\d+"));
        QRegularExpressionMatch match = re.match(strataName);
        if (match.hasMatch()) {
            return match.captured().toInt();
        }
        return 999;  // 默认排在最后
    }

    /**
     * 获取地层颜色索引
     */
    static int getStrataColor(const QString &strataName) {
        if (STRATA_COLORS.contains(strataName)) {
            return STRATA_COLORS.value(strataName);
        }
        return 7;  // 默认白色
    }
    
    /**
     * 解析源文件桩号格式
     * 格式: 00+000.TIN 或 00+000
     * 返回桩号数值（米）
     */
    static int parseSourceStation(const QString &text) {
        // 简化实现：直接使用正则匹配
        QString upper = text.toUpper();
        QRegularExpression re("(\\d+)\\+(\\d+)");
        QRegularExpressionMatch m = re.match(upper);
        if (m.hasMatch()) {
            return m.captured(1).toInt() * 1000 + m.captured(2).toInt();
        }
        return -1;
    }

    /**
     * 解析目标文件桩号格式
     * 格式: K00+000 或 00+000
     */
    static int parseTargetStation(const QString &text) {
        QString upper = text.toUpper();
        QRegularExpression re("(\\d+)\\+(\\d+)");
        QRegularExpressionMatch m = re.match(upper);
        if (m.hasMatch()) {
            return m.captured(1).toInt() * 1000 + m.captured(2).toInt();
        }
        return -1;
    }
    
    /**
     * 格式化桩号值
     * 例如: 67400 -> "K67+400"
     */
    static QString formatStation(int stationValue) {
        if (stationValue < 0) {
            return QString();
        }
        int km = stationValue / 1000;
        int m = stationValue % 1000;
        return QStringLiteral("K%1+%2").arg(km, 2, 10, QChar('0')).arg(m, 3, 10, QChar('0'));
    }
};

// ==================== 静态成员初始化 ====================

inline const QString Config::DEFAULT_OUTPUT_LAYER = QStringLiteral("FINAL_BOTTOM_SURFACE");
inline const QString Config::DEFAULT_HATCH_LAYER = QStringLiteral("AA_填充算量层");
inline const QString Config::DEFAULT_FINAL_SECTION = QStringLiteral("AA_最终断面线");

inline const QVector<QColor> Config::HIGH_CONTRAST_COLORS = {
    QColor(255, 0, 0),      // 红色
    QColor(0, 200, 0),      // 绿色
    QColor(0, 0, 255),      // 蓝色
    QColor(255, 255, 0),    // 黄色
    QColor(255, 0, 255),    // 紫色
    QColor(0, 255, 255),    // 青色
    QColor(255, 128, 0),    // 橙色
    QColor(128, 0, 255),    // 紫罗兰
    QColor(0, 128, 255),    // 天蓝
    QColor(255, 0, 128),    // 玫红
    QColor(128, 255, 0),    // 黄绿
    QColor(0, 255, 128)     // 青绿
};

inline const QMap<QString, int> Config::STRATA_COLORS = {
    {QStringLiteral("1级淤泥"), 11},
    {QStringLiteral("1级淤泥质土"), 12},
    {QStringLiteral("2级淤泥"), 31},
    {QStringLiteral("3级淤泥"), 32},
    {QStringLiteral("3级粘土"), 33},
    {QStringLiteral("3级黏土"), 33},  // 黏(U+9ECF)与粘(U+7C98)同义
    {QStringLiteral("4级粘土"), 41},
    {QStringLiteral("4级黏土"), 41},
    {QStringLiteral("4级淤泥"), 42},
    {QStringLiteral("5级粘土"), 51},
    {QStringLiteral("5级黏土"), 51},
    {QStringLiteral("6级砂"), 61},
    {QStringLiteral("6级碎石"), 62},
    {QStringLiteral("7级砂"), 71},
    {QStringLiteral("8级砂"), 81},
    {QStringLiteral("9级碎石"), 91},
    // 填土类型
    {QStringLiteral("1级填土"), 13},
    {QStringLiteral("2级填土"), 34},
    {QStringLiteral("3级填土"), 35},
    {QStringLiteral("4级填土"), 43},
    {QStringLiteral("5级填土"), 52}
};

inline const QMap<QString, Config::TaskConfig> Config::TOOL_CONFIG = {
    {QStringLiteral("autoline"), {QStringLiteral("断面合并"), QStringLiteral("将两个断面线图层合并，生成上/下包络线")}},
    {QStringLiteral("autopaste"), {QStringLiteral("批量粘贴"), QStringLiteral("将源断面图批量粘贴到目标图纸")}},
    {QStringLiteral("autohatch"), {QStringLiteral("快速填充"), QStringLiteral("自动识别封闭区域并填充，计算面积")}},
    {QStringLiteral("autosection"), {QStringLiteral("分层算量"), QStringLiteral("计算指定高程分层线以下的面积，支持区分设计/超挖")}},
    {QStringLiteral("backfill"), {QStringLiteral("回淤计算"), QStringLiteral("计算DMX与设计断面线之间的回淤面积")}},
    {QStringLiteral("autosection_backfill"), {QStringLiteral("分层+回淤"), QStringLiteral("分层算量与回淤计算合并，一次运行完成两项计算")}}
};

#endif // CONFIG_H