#ifndef STATION_MATCHER_H
#define STATION_MATCHER_H

#include "Geometry.h"
#include "Config.h"
#include "LayerExtractor.h"
#include <QString>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QPair>
#include <QRegularExpression>
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
    
    struct CandidateInfo {
        double x;
        double y;
        int index;
        
        CandidateInfo() : x(0), y(0), index(-1) {}
        CandidateInfo(double px, double py, int idx) : x(px), y(py), index(idx) {}
    };
    
    static QMap<QString, QVector<StationInfo>> extractStations(const QVector<LayerExtractor::TextInfo> &texts) {
        QMap<QString, QVector<StationInfo>> stations;
        QRegularExpression re(Config::STATION_PATTERN());
        
        for (const LayerExtractor::TextInfo &textInfo : texts) {
            QString txt = textInfo.text.toUpper();
            QRegularExpressionMatch match = re.match(txt);
            
            if (match.hasMatch()) {
                QString sid = match.captured(1);
                int value = Config::parseSourceStation(sid);
                
                if (value >= 0) {
                    StationInfo info(sid, value, textInfo.x, textInfo.y);
                    if (!stations.contains(sid)) {
                        stations[sid] = QVector<StationInfo>();
                    }
                    stations[sid].append(info);
                }
            }
        }
        return stations;
    }
    
    static int stationSortKey(const QString &stationStr) {
        return Config::stationSortKey(stationStr);
    }
    
    static int strataSortKey(const QString &strataName) {
        return Config::strataSortKey(strataName);
    }
    
    static QPair<int, CandidateInfo> findNearest(const Point2D &targetPt,
                                                   const QVector<CandidateInfo> &candidates,
                                                   const QSet<int> &used = QSet<int>(),
                                                   double tolerance = 200.0) {
        int bestIdx = -1;
        CandidateInfo best;
        double bestDist = std::numeric_limits<double>::max();
        
        for (int i = 0; i < candidates.size(); ++i) {
            if (used.contains(i)) continue;
            
            const CandidateInfo &c = candidates[i];
            double dist = std::sqrt((c.x - targetPt.x) * (c.x - targetPt.x) +
                                    (c.y - targetPt.y) * (c.y - targetPt.y));
            
            if (dist < bestDist && dist < tolerance) {
                bestDist = dist;
                bestIdx = i;
                best = c;
            }
        }
        return QPair<int, CandidateInfo>(bestIdx, best);
    }
    
    static QVector<StationInfo> sortStationsByValue(const QVector<StationInfo> &stations) {
        QVector<StationInfo> sorted = stations;
        std::sort(sorted.begin(), sorted.end(), 
                  [](const StationInfo &a, const StationInfo &b) {
                      return a.value < b.value;
                  });
        return sorted;
    }
    
    static QVector<StationInfo> sortStationsByY(const QVector<StationInfo> &stations) {
        QVector<StationInfo> sorted = stations;
        std::sort(sorted.begin(), sorted.end(), 
                  [](const StationInfo &a, const StationInfo &b) {
                      return a.y > b.y;
                  });
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
        if (semicolonPos >= 0) {
            cleaned = cleaned.mid(semicolonPos + 1);
        }
        cleaned.replace('}', "");
        cleaned = cleaned.trimmed();
        return cleaned;
    }
    
    static double distance(double x1, double y1, double x2, double y2) {
        return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    }
    
    static double weightedDistance(double x1, double y1, double x2, double y2, double xWeight = 0.5) {
        return std::sqrt((x2 - x1) * (x2 - x1) * xWeight + (y2 - y1) * (y2 - y1));
    }
};

#endif // STATION_MATCHER_H