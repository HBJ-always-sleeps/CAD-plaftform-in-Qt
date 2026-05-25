#ifndef ENTITY_HELPER_H
#define ENTITY_HELPER_H

#include "Geometry.h"
#include <QString>
#include <QVector>

/**
 * EntityHelper类 - 复刻Python engine_cad_v3.py EntityHelper类
 * 
 * DXF实体转换工具集：
 * - to_linestring: 各种线类型转换为Line2D
 * - get_best_point: 获取文本实体最佳点
 * - get_text: 获取文本内容
 */
class EntityHelper
{
public:
    // ==================== DXF实体类型枚举 ====================
    enum class EntityType {
        LINE,
        LWPOLYLINE,
        POLYLINE,
        TEXT,
        MTEXT,
        HATCH,
        INSERT,
        ARC,
        UNKNOWN
    };
    
    /**
     * 统一处理各种线类型 -> Line2D
     * 
     * 对应Python: engine_cad_v3.py 第55-66行
     */
    static Line2D toLinestring(const void *entity, EntityType type) {
        // 注意：实际实现需要dxflib库支持
        // 此处为接口定义，具体实现在EntityHelper.cpp中
        
        Line2D result;
        
        switch (type) {
        case EntityType::LWPOLYLINE:
        case EntityType::POLYLINE:
            // 从多段线提取点
            // result.points = extractPolylinePoints(entity);
            break;
            
        case EntityType::LINE:
            // 从直线提取起点和终点
            // result.points = {getLineStart(entity), getLineEnd(entity)};
            break;
            
        case EntityType::ARC:
            // 从圆弧离散化提取点
            // result.points = flattenArc(entity, 0.1);
            break;
            
        default:
            break;
        }
        
        return result;
    }
    
    /**
     * 获取文本实体的最佳点
     * 
     * 对应Python: engine_cad_v3.py 第68-76行
     */
    static Point2D getBestPoint(const void *entity, EntityType type) {
        Point2D result(0, 0);
        
        switch (type) {
        case EntityType::TEXT:
            // TEXT实体优先使用align_point，否则使用insert
            // if (hasHorizontalAlignment(entity) || hasVerticalAlignment(entity))
            //     result = getAlignPoint(entity);
            // else
            //     result = getInsertPoint(entity);
            break;
            
        case EntityType::MTEXT:
            // MTEXT实体使用insert点
            // result = getInsertPoint(entity);
            break;
            
        default:
            // 其他实体使用insert点
            // result = getInsertPoint(entity);
            break;
        }
        
        return result;
    }
    
    /**
     * 获取文本内容
     * 
     * 对应Python: engine_cad_v3.py 第78-81行
     */
    static QString getText(const void *entity, EntityType type) {
        QString result;
        
        switch (type) {
        case EntityType::MTEXT:
            // MTEXT使用plain_text()方法
            // result = getMTextPlainText(entity);
            break;
            
        case EntityType::TEXT:
            // TEXT使用dxf.text属性
            // result = getTextContent(entity);
            break;
            
        default:
            break;
        }
        
        return result;
    }
    
    /**
     * 判断实体是否为线类型
     */
    static bool isLineType(EntityType type) {
        return type == EntityType::LINE ||
               type == EntityType::LWPOLYLINE ||
               type == EntityType::POLYLINE ||
               type == EntityType::ARC;
    }
    
    /**
     * 判断实体是否为文本类型
     */
    static bool isTextType(EntityType type) {
        return type == EntityType::TEXT || type == EntityType::MTEXT;
    }
    
    /**
     * 从字符串解析实体类型
     */
    static EntityType parseEntityType(const QString &dxftype) {
        QString type = dxftype.toUpper();
        
        if (type == "LINE") return EntityType::LINE;
        if (type == "LWPOLYLINE") return EntityType::LWPOLYLINE;
        if (type == "POLYLINE") return EntityType::POLYLINE;
        if (type == "TEXT") return EntityType::TEXT;
        if (type == "MTEXT") return EntityType::MTEXT;
        if (type == "HATCH") return EntityType::HATCH;
        if (type == "INSERT") return EntityType::INSERT;
        if (type == "ARC") return EntityType::ARC;
        
        return EntityType::UNKNOWN;
    }
};

#endif // ENTITY_HELPER_H