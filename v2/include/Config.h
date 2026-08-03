#ifndef CONFIG_H
#define CONFIG_H

#include <QtGlobal>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QColor>
#include <QVector>
#include <QRegularExpression>

class Config
{
public:
    static const QString DEFAULT_OUTPUT_LAYER;
    static const QString DEFAULT_HATCH_LAYER;
    static const QString DEFAULT_FINAL_SECTION;
    static const QVector<QColor> HIGH_CONTRAST_COLORS;
    static const QMap<QString, int> STRATA_COLORS;

    static constexpr double AREA_SCALE_FACTOR = 1.0;
    static constexpr double DEFAULT_TEXT_HEIGHT = 3.0;
    static constexpr double HATCH_SCALE_FACTOR = 0.02;
    static constexpr double SECTION_WIDTH_MIN = 130.0;
    static constexpr double SECTION_WIDTH_MAX = 200.0;
    static constexpr double SECTION_HEIGHT_MIN = 95.0;
    static constexpr double SECTION_HEIGHT_MAX = 140.0;
    static constexpr double CURVE_VERTEX_THRESHOLD = 50;
    static constexpr double STATION_MATCH_TOLERANCE = 500.0;
    static constexpr double LINE_DISTANCE_THRESHOLD = 0.5;
    static constexpr double BASEPOINT_Y_TOLERANCE = 50.0;
    static constexpr double BP_X_GROUP_TOLERANCE = 50.0;

    static int parseStation(const QString &text) {
        QRegularExpression re(QStringLiteral("(\\d+)\\+(\\d+)"));
        QRegularExpressionMatch m = re.match(text.toUpper());
        return m.hasMatch() ? m.captured(1).toInt() * 1000 + m.captured(2).toInt() : -1;
    }

    static QString formatStation(int stationValue) {
        if (stationValue < 0) return QString();
        return QStringLiteral("K%1+%2").arg(stationValue / 1000, 2, 10, QChar('0'))
                                       .arg(stationValue % 1000, 3, 10, QChar('0'));
    }
};

inline const QString Config::DEFAULT_OUTPUT_LAYER = QStringLiteral("FINAL_BOTTOM_SURFACE");
inline const QString Config::DEFAULT_HATCH_LAYER = QStringLiteral("AA_填充算量层");
inline const QString Config::DEFAULT_FINAL_SECTION = QStringLiteral("AA_最终断面线");

inline const QVector<QColor> Config::HIGH_CONTRAST_COLORS = {
    QColor(255,0,0), QColor(0,200,0), QColor(0,0,255), QColor(255,255,0),
    QColor(255,0,255), QColor(0,255,255), QColor(255,128,0), QColor(128,0,255),
    QColor(0,128,255), QColor(255,0,128), QColor(128,255,0), QColor(0,255,128)
};

inline const QMap<QString, int> Config::STRATA_COLORS = {
    {QStringLiteral("1级淤泥"), 11}, {QStringLiteral("1级淤泥质土"), 12},
    {QStringLiteral("2级淤泥"), 31}, {QStringLiteral("3级淤泥"), 32},
    {QStringLiteral("3级粘土"), 33}, {QStringLiteral("3级黏土"), 33},
    {QStringLiteral("4级粘土"), 41}, {QStringLiteral("4级黏土"), 41},
    {QStringLiteral("4级淤泥"), 42}, {QStringLiteral("5级粘土"), 51},
    {QStringLiteral("5级黏土"), 51}, {QStringLiteral("6级砂"), 61},
    {QStringLiteral("6级碎石"), 62}, {QStringLiteral("7级砂"), 71},
    {QStringLiteral("8级砂"), 81}, {QStringLiteral("9级碎石"), 91},
    {QStringLiteral("1级填土"), 13}, {QStringLiteral("2级填土"), 34},
    {QStringLiteral("3级填土"), 35}, {QStringLiteral("4级填土"), 43},
    {QStringLiteral("5级填土"), 52}
};

#endif // CONFIG_H
