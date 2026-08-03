#ifndef STATION_MATCHER_H
#define STATION_MATCHER_H

#include "Geometry.h"
#include "Config.h"
#include <QString>
#include <QVector>
#include <QSet>
#include <QPair>
#include <cmath>
#include <algorithm>

class StationMatcher
{
public:
    struct StationInfo {
        QString text;
        int value;
        double x;
        double y;

        StationInfo() : text(""), value(-1), x(0), y(0) {}
        StationInfo(const QString &t, int v, double px, double py)
            : text(t), value(v), x(px), y(py) {}
    };

    static QVector<StationInfo> sortStationsByY(const QVector<StationInfo> &stations) {
        QVector<StationInfo> sorted = stations;
        std::sort(sorted.begin(), sorted.end(),
                  [](const StationInfo &a, const StationInfo &b) { return a.y > b.y; });
        return sorted;
    }

    static QPair<StationInfo, double> matchSectionToStation(
        double sectXCenter, double sectYCenter,
        const QVector<StationInfo> &stationTexts,
        QSet<QString> &usedStations,
        double tolerance = 500.0) {

        StationInfo bestStation;
        double bestDist = std::numeric_limits<double>::max();

        for (const StationInfo &st : stationTexts) {
            if (usedStations.contains(st.text)) continue;
            double dist = std::sqrt((st.x - sectXCenter) * (st.x - sectXCenter) * 0.5 +
                                    (st.y - sectYCenter) * (st.y - sectYCenter));
            if (dist < bestDist && dist < tolerance) {
                bestDist = dist;
                bestStation = st;
            }
        }
        return QPair<StationInfo, double>(bestStation, bestDist);
    }

    static QString cleanStationText(const QString &rawText) {
        QString cleaned = rawText;
        int semicolonPos = cleaned.indexOf(';');
        if (semicolonPos >= 0) cleaned = cleaned.mid(semicolonPos + 1);
        cleaned.replace('}', "");
        return cleaned.trimmed();
    }
};

#endif // STATION_MATCHER_H
