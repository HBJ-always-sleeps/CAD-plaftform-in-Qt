#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
autosection_compute.py - 分层算量 + 回淤计算（Shapely精确计算）

从C++导出的JSON读取原始数据，使用Shapely进行精确多边形布尔运算，
输出DXF（带HATCH填充）和XLSX（多sheet）。

用法: python autosection_compute.py <input.json>
"""

import json
import sys
import os
import re
import datetime
import numpy as np

try:
    from shapely.geometry import Polygon, LineString, box, Point
    from shapely.ops import unary_union
except ImportError:
    print("[ERROR] 需要安装shapely: pip install shapely")
    sys.exit(1)

try:
    import ezdxf
except ImportError:
    print("[ERROR] 需要安装ezdxf: pip install ezdxf")
    sys.exit(1)

try:
    import pandas as pd
except ImportError:
    print("[ERROR] 需要安装pandas和openpyxl: pip install pandas openpyxl")
    sys.exit(1)

# ==================== 配置常量 ====================

# 地层颜色映射 - 包含"粘"和"黏"两种写法
STRATA_COLORS = {
    '1级淤泥': 11, '1级淤泥质土': 12, '2级淤泥': 31, '3级淤泥': 32,
    '3级粘土': 33, '3级黏土': 33, '4级粘土': 41, '4级黏土': 41,
    '4级淤泥': 42, '5级粘土': 51, '5级黏土': 51,
    '6级砂': 61, '6级碎石': 62, '7级砂': 71, '8级砂': 81, '9级碎石': 91,
    # 填土类型
    '1级填土': 13, '2级填土': 34, '3级填土': 35, '4级填土': 43, '5级填土': 52,
}

HIGH_CONTRAST_COLORS = [
    (255, 0, 0), (0, 200, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255), (0, 255, 255),
    (255, 128, 0), (128, 0, 255), (0, 128, 255), (255, 0, 128), (128, 255, 0), (0, 255, 128),
]


# ==================== 工具函数 ====================

def LOG(msg):
    print(msg)


def strata_sort_key(layer_name):
    """地层排序键"""
    m = re.match(r'(\d+)', layer_name)
    return int(m.group(1)) if m else 999


def station_sort_key(station_text):
    """桩号排序键"""
    m = re.match(r'K(\d+)\+(\d+)', station_text)
    if m:
        return int(m.group(1)) * 10000 + int(m.group(2))
    m = re.match(r'S(\d+)', station_text)
    if m:
        return 1000000 + int(m.group(1))
    return 9999999


def get_y_at_x(line, x):
    """在线段上获取指定X处的Y值（线性插值）"""
    if line is None:
        return None
    coords = list(line.coords) if hasattr(line, 'coords') else line
    for i in range(len(coords) - 1):
        x1, y1 = coords[i][0], coords[i][1]
        x2, y2 = coords[i + 1][0], coords[i + 1][1]
        if x1 == x2:
            if abs(x - x1) < 0.01:
                return (y1 + y2) / 2
            continue
        if min(x1, x2) - 0.01 <= x <= max(x1, x2) + 0.01:
            t = (x - x1) / (x2 - x1)
            if -0.001 <= t <= 1.001:
                return y1 + t * (y2 - y1)
    return None


def build_design_polygon(excav_lines, sect_x_min, sect_x_max):
    """构建设计区多边形"""
    if not excav_lines:
        return None

    all_points = [p for l in excav_lines for p in l.coords]
    if not all_points:
        return None

    excav_x_min = min(p[0] for p in all_points)
    excav_x_max = max(p[0] for p in all_points)

    design_x_min = max(excav_x_min, sect_x_min)
    design_x_max = min(excav_x_max, sect_x_max)

    if design_x_max <= design_x_min:
        return None

    x_samples = []
    y_samples = []
    x_current = design_x_min

    while x_current <= design_x_max:
        min_y = None
        for line in excav_lines:
            y = get_y_at_x(line, x_current)
            if y is not None and (min_y is None or y < min_y):
                min_y = y
        if min_y is not None:
            x_samples.append(x_current)
            y_samples.append(min_y)
        x_current += 1.0

    if len(x_samples) < 2:
        return None

    sect_y_max = max(y_samples) + 50
    polygon_coords = list(zip(x_samples, y_samples))
    polygon_coords.append((x_samples[-1], sect_y_max))
    polygon_coords.append((x_samples[0], sect_y_max))
    polygon_coords.append(polygon_coords[0])

    poly = Polygon(polygon_coords)
    return poly if poly.is_valid else poly.buffer(0)


def generate_envelope(dmx_line, update_lines, envelope_type):
    """生成包络线（upper=上包络取max, lower=下包络取min）"""
    if not update_lines:
        return dmx_line

    dmx_coords = list(dmx_line.coords)
    dmx_x_min = min(c[0] for c in dmx_coords)
    dmx_x_max = max(c[0] for c in dmx_coords)

    all_lines = [dmx_line] + update_lines
    all_x_min = min(min(c[0] for c in l.coords) for l in all_lines)
    all_x_max = max(max(c[0] for c in l.coords) for l in all_lines)

    num_samples = max(int((all_x_max - all_x_min) / 0.5) + 1, 100)
    envelope_pts = []

    for i in range(num_samples + 1):
        x = all_x_min + (all_x_max - all_x_min) * i / num_samples
        values = []
        for line in all_lines:
            y = get_y_at_x(line, x)
            if y is not None:
                values.append(y)
        if values:
            envelope_pts.append((x, max(values) if envelope_type == 'upper' else min(values)))

    if len(envelope_pts) < 2:
        return dmx_line

    return LineString(envelope_pts)


def detect_ruler_scale_per_section(doc, msp, sect_x_min, sect_x_max, sect_y_min, sect_y_max):
    """逐断面检测标尺比例（对应原始engine_cad_v3.py的RulerDetector.detect_scale）"""
    ruler_layers = ['标尺', '0-标尺', 'RULER']
    ruler_candidates = []

    for layer_name in ruler_layers:
        try:
            for e in msp.query(f'*[layer=="{layer_name}"]'):
                if e.dxftype() == 'INSERT':
                    try:
                        insert_x = e.dxf.insert.x
                        insert_y = e.dxf.insert.y
                        if sect_x_min - 100 <= insert_x <= sect_x_max + 100:
                            y_min, y_max = insert_y, insert_y
                            try:
                                block_name = e.dxf.name
                                if block_name in doc.blocks:
                                    for be in doc.blocks[block_name]:
                                        if be.dxftype() in ('TEXT', 'MTEXT'):
                                            try:
                                                world_y = be.dxf.insert.y + insert_y
                                                y_min = min(y_min, world_y)
                                                y_max = max(y_max, world_y)
                                            except:
                                                pass
                            except:
                                pass
                            ruler_candidates.append({
                                'x': insert_x, 'y_min': y_min, 'y_max': y_max, 'entity': e
                            })
                    except:
                        pass
        except:
            pass

    if not ruler_candidates:
        return None

    # 选择最佳标尺：优先Y重叠最大的，其次选X距离最近的
    sect_y_center = (sect_y_min + sect_y_max) / 2
    sect_x_center = (sect_x_min + sect_x_max) / 2

    best_ruler = None
    best_overlap = -1
    for ruler in ruler_candidates:
        if ruler['y_max'] > ruler['y_min']:
            overlap = max(0, min(sect_y_max, ruler['y_max']) - max(sect_y_min, ruler['y_min']))
            overlap_ratio = overlap / (ruler['y_max'] - ruler['y_min'])
        else:
            overlap_ratio = 0
        if overlap_ratio > best_overlap:
            best_overlap = overlap_ratio
            best_ruler = ruler

    if best_ruler is None or best_overlap <= 0:
        best_ruler = min(ruler_candidates, key=lambda r: abs(r['x'] - sect_x_center))

    # 从最佳标尺提取高程点
    elevation_points = []
    if best_ruler.get('entity'):
        insert_y = best_ruler['entity'].dxf.insert.y
        try:
            block_name = best_ruler['entity'].dxf.name
            if block_name in doc.blocks:
                for be in doc.blocks[block_name]:
                    if be.dxftype() in ('TEXT', 'MTEXT'):
                        try:
                            world_y = be.dxf.insert.y + insert_y
                            text = (be.dxf.text if be.dxftype() == 'TEXT' else be.text).strip()
                            elevation_points.append((world_y, float(text)))
                        except:
                            pass
        except:
            pass

    if len(elevation_points) < 2:
        return None

    # 线性回归: y = a * elevation + b
    n = len(elevation_points)
    sum_y = sum(p[0] for p in elevation_points)
    sum_e = sum(p[1] for p in elevation_points)
    sum_ye = sum(p[0] * p[1] for p in elevation_points)
    sum_e2 = sum(p[1] ** 2 for p in elevation_points)
    denom = n * sum_e2 - sum_e ** 2
    if abs(denom) < 0.001:
        return None

    a = (n * sum_ye - sum_y * sum_e) / denom
    b = (sum_y - a * sum_e) / n

    if abs(a - 5.0) > 2.0 or abs(b - 2510) > 500:
        return None

    return lambda elev: a * elev + b


def shapely_to_boundary_pts(poly):
    """将Shapely多边形转换为边界点列表"""
    if poly.geom_type == 'MultiPolygon':
        # 只返回最大面积的多边形的外边界
        largest = max(poly.geoms, key=lambda g: g.area)
        pts = list(largest.exterior.coords)[:-1]
        return pts
    pts = list(poly.exterior.coords)[:-1]  # 去掉闭合点
    return pts


def add_hatch_to_dxf(msp, poly, layer_name, color_idx=7, rgb_color=None):
    """添加HATCH填充到DXF"""
    if poly.is_empty or poly.area < 0.01:
        return

    # 处理MultiPolygon：为每个子多边形创建HATCH
    if poly.geom_type == 'MultiPolygon':
        for g in poly.geoms:
            add_hatch_to_dxf(msp, g, layer_name, color_idx, rgb_color)
        return

    pts = shapely_to_boundary_pts(poly)
    if len(pts) < 3:
        return

    try:
        hatch = msp.add_hatch(dxfattribs={'layer': layer_name})
        # 使用set_solid_fill设置ACI颜色，rgb参数设置RGB颜色（优先级更高）
        hatch.set_solid_fill(color=color_idx, rgb=rgb_color)
        hatch.paths.add_polyline_path(pts, is_closed=True)
    except Exception as e:
        LOG(f"[WARN] HATCH创建失败: {e}")


# ==================== 核心计算 ====================

def compute_autosection(data):
    """分层算量计算"""
    input_dxf = data['input_dxf']
    output_dxf = data['output_dxf']
    output_xlsx = data.get('output_xlsx', '')
    target_elevation = data.get('target_elevation', None)
    calc_mode = data.get('calc_mode', 'below')
    distinguish_design = data.get('distinguish_design', False)
    merge_section = data.get('merge_section', False)
    strata_layers = data.get('strata_layers', [])
    sections = data.get('sections', [])
    excav_lines_data = data.get('excav_lines', [])

    LOG(f"[INFO] 分层算量: 高程={target_elevation}, 模式={calc_mode}")
    LOG(f"[INFO] 地层数: {len(strata_layers)}, 断面数: {len(sections)}")

    # 读取源DXF
    doc = ezdxf.readfile(input_dxf)
    output_doc = ezdxf.readfile(input_dxf)
    output_msp = output_doc.modelspace()

    # 创建分层线图层
    elev_layer_name = f"分层线_{target_elevation}m" if target_elevation is not None else "分层线_全算量"
    if elev_layer_name not in output_doc.layers:
        output_doc.layers.new(name=elev_layer_name, dxfattribs={'color': 1})

    # 转换开挖线为Shapely对象
    excav_lines_shapely = []
    for line_data in excav_lines_data:
        pts = line_data['points']
        if len(pts) >= 2:
            excav_lines_shapely.append(LineString(pts))

    msp = doc.modelspace()

    # 预加载所有地层HATCH（按图层分组，避免逐断面重复查询）
    all_hatches_by_layer = {}
    import re as re_mod
    for h in msp.query('HATCH'):
        try:
            layer = h.dxf.layer
            # 只收集地层HATCH（图层名以数字开头）
            if not re_mod.match(r'^\d', layer):
                continue
            pts = []
            for path in h.paths:
                if hasattr(path, 'vertices'):
                    pts = [(v[0], v[1]) for v in path.vertices]
                    break
            if len(pts) < 3:
                continue
            h_poly = Polygon(pts)
            if not h_poly.is_valid:
                h_poly = h_poly.buffer(0)
            if h_poly.is_empty:
                continue
            if layer not in all_hatches_by_layer:
                all_hatches_by_layer[layer] = []
            all_hatches_by_layer[layer].append(h_poly)
        except:
            pass

    LOG(f"[INFO] 预加载HATCH: {sum(len(v) for v in all_hatches_by_layer.values())}个, {len(all_hatches_by_layer)}个图层")

    # 匹配strata_layers到实际DXF图层名（模糊匹配）
    actual_layer_map = {}  # strata_layer -> actual_dxf_layer
    dxf_layers = list(all_hatches_by_layer.keys())
    for sl in strata_layers:
        # 尝试精确匹配
        if sl in dxf_layers:
            actual_layer_map[sl] = sl
            continue
        # 尝试模糊匹配：检查strata_layer是否是dxf_layer的子串，或反之
        best_match = None
        best_score = 0
        for dl in dxf_layers:
            # 检查是否共享数字前缀
            sl_num = re.match(r'^\d+', sl)
            dl_num = re.match(r'^\d+', dl)
            if sl_num and dl_num and sl_num.group() == dl_num.group():
                # 同一编号，检查剩余字符的相似度
                sl_rest = sl[sl_num.end():]
                dl_rest = dl[dl_num.end():]
                # 如果剩余部分有重叠字符
                common = set(sl_rest) & set(dl_rest)
                score = len(common) / max(len(sl_rest), len(dl_rest), 1)
                if score > best_score:
                    best_score = score
                    best_match = dl
        if best_match:
            actual_layer_map[sl] = best_match
        else:
            actual_layer_map[sl] = sl  # 保持原名（可能匹配不到）

    LOG(f"[INFO] 图层映射: {actual_layer_map}")
    results = []

    for idx, section in enumerate(sections):
        station = section['station']
        dmx_pts = section['dmx_points']

        if len(dmx_pts) < 2:
            continue

        dmx_line = LineString(dmx_pts)
        sect_x_min = min(p[0] for p in dmx_pts)
        sect_x_max = max(p[0] for p in dmx_pts)
        sect_y_min = min(p[1] for p in dmx_pts)
        sect_y_max = max(p[1] for p in dmx_pts)

        # 标尺计算高程线Y（逐断面检测）
        target_line_y = None
        if target_elevation is not None:
            elev_to_y = detect_ruler_scale_per_section(doc, msp, sect_x_min, sect_x_max, sect_y_min, sect_y_max)
            if elev_to_y:
                target_line_y = elev_to_y(target_elevation)
            else:
                target_line_y = 5.0 * target_elevation - 27.0

        # 合并辅助断面线
        final_section = dmx_line
        aux_lines_data = section.get('aux_lines', [])
        if merge_section and aux_lines_data:
            aux_shapely = []
            for pts in aux_lines_data:
                if len(pts) >= 2:
                    aux_shapely.append(LineString(pts))
            if aux_shapely:
                final_section = generate_envelope(dmx_line, aux_shapely, 'lower')

        # 构建开挖区域多边形
        sect_coords = list(final_section.coords)
        sect_x_min_actual = min(c[0] for c in sect_coords)
        sect_x_max_actual = max(c[0] for c in sect_coords)
        sect_y_min_actual = min(c[1] for c in sect_coords)
        sect_y_max_actual = max(c[1] for c in sect_coords)

        bottom_y = sect_y_min_actual - 50
        total_open_poly = Polygon(
            sect_coords + [(sect_x_max_actual, bottom_y), (sect_x_min_actual, bottom_y)]
        )
        if not total_open_poly.is_valid:
            total_open_poly = total_open_poly.buffer(0)

        if total_open_poly.is_empty:
            result = {'断面名称': station, '分层线高程': target_elevation, '总面积': 0.0}
            for layer in strata_layers:
                if distinguish_design:
                    result[f'{layer}_设计'] = 0.0
                    result[f'{layer}_超挖'] = 0.0
                else:
                    result[layer] = 0.0
            results.append(result)
            continue

        # 高程线裁剪
        layer_open = None
        if target_line_y is None:
            layer_open = total_open_poly
        elif calc_mode == 'below':
            if target_line_y < sect_y_min_actual:
                result = {'断面名称': station, '分层线高程': target_elevation, '总面积': 0.0}
                for layer in strata_layers:
                    if distinguish_design:
                        result[f'{layer}_设计'] = 0.0
                        result[f'{layer}_超挖'] = 0.0
                    else:
                        result[layer] = 0.0
                results.append(result)
                # 画高程线
                if target_line_y > sect_y_min_actual - 50:
                    output_msp.add_lwpolyline(
                        [(sect_x_min_actual - 5, target_line_y), (sect_x_max_actual + 5, target_line_y)],
                        dxfattribs={'layer': elev_layer_name, 'color': 1}
                    )
                continue
            else:
                clip_poly = box(sect_x_min_actual - 10, sect_y_min_actual - 100,
                                sect_x_max_actual + 10, target_line_y)
                layer_open = total_open_poly.intersection(clip_poly)
        else:  # above
            if target_line_y > sect_y_max_actual:
                result = {'断面名称': station, '分层线高程': target_elevation, '总面积': 0.0}
                for layer in strata_layers:
                    if distinguish_design:
                        result[f'{layer}_设计'] = 0.0
                        result[f'{layer}_超挖'] = 0.0
                    else:
                        result[layer] = 0.0
                results.append(result)
                continue
            else:
                clip_poly = box(sect_x_min_actual - 10, target_line_y,
                                sect_x_max_actual + 10, sect_y_max_actual + 100)
                layer_open = total_open_poly.intersection(clip_poly)

        # Debug: track clipping results
        if target_line_y is not None and idx < 5:
            LOG(f"  [{station}] target_y={target_line_y:.1f}, sect_y=[{sect_y_min_actual:.1f}, {sect_y_max_actual:.1f}], layer_open_area={layer_open.area:.1f}")

        if layer_open.is_empty:
            result = {'断面名称': station, '分层线高程': target_elevation, '总面积': 0.0}
            for layer in strata_layers:
                if distinguish_design:
                    result[f'{layer}_设计'] = 0.0
                    result[f'{layer}_超挖'] = 0.0
                else:
                    result[layer] = 0.0
            results.append(result)
            continue

        # 画高程线
        if target_line_y is not None:
            if calc_mode == 'below' and target_line_y > sect_y_min_actual:
                output_msp.add_lwpolyline(
                    [(sect_x_min_actual - 5, target_line_y), (sect_x_max_actual + 5, target_line_y)],
                    dxfattribs={'layer': elev_layer_name, 'color': 1}
                )
            elif calc_mode == 'above' and target_line_y < sect_y_max_actual:
                output_msp.add_lwpolyline(
                    [(sect_x_min_actual - 5, target_line_y), (sect_x_max_actual + 5, target_line_y)],
                    dxfattribs={'layer': elev_layer_name, 'color': 1}
                )

        # 构建设计区多边形
        design_polygon = None
        if distinguish_design:
            boundary_box = box(sect_x_min_actual - 20, sect_y_min_actual - 50,
                               sect_x_max_actual + 20, sect_y_max_actual + 50)
            excav_in_section = [l for l in excav_lines_shapely if boundary_box.intersects(l)]
            if excav_in_section:
                design_polygon = build_design_polygon(excav_in_section, sect_x_min_actual, sect_x_max_actual)

        # 统计各地层面积
        boundary_box = box(sect_x_min_actual - 20, sect_y_min_actual - 50,
                           sect_x_max_actual + 20, sect_y_max_actual + 50)
        strata_areas = {}
        total_area = 0.0

        # 使用预加载的HATCH数据
        for layer in strata_layers:
            design_area = 0.0
            over_area = 0.0
            total_layer_area = 0.0

            design_polys = []
            over_polys = []

            # 获取实际DXF图层名
            actual_layer = actual_layer_map.get(layer, layer)
            hatch_polys = all_hatches_by_layer.get(actual_layer, [])

            for h_poly in hatch_polys:
                try:
                    if not boundary_box.intersects(h_poly):
                        continue

                    inter = h_poly.intersection(layer_open)
                    if inter.is_empty:
                        continue

                    if distinguish_design and design_polygon:
                        design_part = inter.intersection(design_polygon)
                        over_part = inter.difference(design_polygon)

                        if not design_part.is_empty:
                            if design_part.geom_type == 'Polygon':
                                design_area += design_part.area
                                design_polys.append(design_part)
                            elif hasattr(design_part, 'geoms'):
                                for g in design_part.geoms:
                                    if g.geom_type == 'Polygon':
                                        design_area += g.area
                                        design_polys.append(g)

                        if not over_part.is_empty:
                            if over_part.geom_type == 'Polygon':
                                over_area += over_part.area
                                over_polys.append(over_part)
                            elif hasattr(over_part, 'geoms'):
                                for g in over_part.geoms:
                                    if g.geom_type == 'Polygon':
                                        over_area += g.area
                                        over_polys.append(g)
                    else:
                        if inter.geom_type == 'Polygon':
                            total_layer_area += inter.area
                            design_polys.append(inter)
                        elif hasattr(inter, 'geoms'):
                            for g in inter.geoms:
                                if g.geom_type == 'Polygon':
                                    total_layer_area += g.area
                                    design_polys.append(g)
                except:
                    pass

            if distinguish_design:
                strata_areas[f'{layer}_设计'] = round(design_area, 3)
                strata_areas[f'{layer}_超挖'] = round(over_area, 3)
                total_area += design_area + over_area

                # 输出HATCH填充
                color_idx = STRATA_COLORS.get(layer, 7)
                rgb_color = HIGH_CONTRAST_COLORS[strata_layers.index(layer) % len(HIGH_CONTRAST_COLORS)]

                for poly in design_polys:
                    add_hatch_to_dxf(output_msp, poly, f"{target_elevation}m_{layer}_设计", color_idx, rgb_color)
                for poly in over_polys:
                    add_hatch_to_dxf(output_msp, poly, f"{target_elevation}m_{layer}_超挖", color_idx, rgb_color)
            else:
                strata_areas[layer] = round(total_layer_area, 3)
                total_area += total_layer_area

                if total_layer_area > 0.01:
                    color_idx = STRATA_COLORS.get(layer, 7)
                    for poly in design_polys:
                        add_hatch_to_dxf(output_msp, poly, f"{target_elevation}m_{layer}", color_idx)

        result = {
            '断面名称': station,
            '分层线高程': target_elevation,
            **strata_areas,
            '总面积': round(total_area, 3)
        }
        results.append(result)

        if (idx + 1) % 50 == 0:
            LOG(f"  已处理 {idx + 1}/{len(sections)} 个断面...")

    # 调试统计
    nonzero_count = sum(1 for r in results if r.get('总面积', 0) > 0)
    total_all = sum(r.get('总面积', 0) for r in results)
    LOG(f"[INFO] 非零断面: {nonzero_count}/{len(results)}, 总面积: {total_all:.1f}")

    # 排序结果
    results.sort(key=lambda x: station_sort_key(x['断面名称']))

    # 保存DXF
    LOG(f"[INFO] 保存DXF: {output_dxf}")
    output_doc.saveas(output_dxf)
    LOG(f"[OK] DXF已保存")

    # 生成Excel
    if results and output_xlsx:
        _write_autosection_excel(results, strata_layers, distinguish_design,
                                 target_elevation, calc_mode, output_xlsx)

    return results


def compute_autosection_backfill(data):
    """分层算量 + 回淤计算合并"""
    input_dxf = data['input_dxf']
    output_dxf = data['output_dxf']
    output_xlsx = data.get('output_xlsx', '')
    target_elevation = data.get('target_elevation', None)
    calc_mode = data.get('calc_mode', 'below')
    distinguish_design = data.get('distinguish_design', False)
    merge_section = data.get('merge_section', False)
    strata_layers = data.get('strata_layers', [])
    sections = data.get('sections', [])
    excav_lines_data = data.get('excav_lines', [])

    LOG(f"[INFO] 分层+回淤合并: 高程={target_elevation}, 模式={calc_mode}")
    LOG(f"[INFO] 地层数: {len(strata_layers)}, 断面数: {len(sections)}")

    # 读取源DXF
    doc = ezdxf.readfile(input_dxf)
    output_doc = ezdxf.readfile(input_dxf)
    output_msp = output_doc.modelspace()

    # 创建图层
    elev_layer_name = f"分层线_{target_elevation}m" if target_elevation is not None else "分层线_全算量"
    if elev_layer_name not in output_doc.layers:
        output_doc.layers.new(name=elev_layer_name, dxfattribs={'color': 1})
    backfill_layer = "回淤面积填充"
    if backfill_layer not in output_doc.layers:
        output_doc.layers.new(name=backfill_layer, dxfattribs={'color': 1})

    # 转换开挖线
    excav_lines_shapely = []
    for line_data in excav_lines_data:
        pts = line_data['points']
        if len(pts) >= 2:
            excav_lines_shapely.append(LineString(pts))

    msp = doc.modelspace()

    # 预加载所有地层HATCH（按图层分组）
    all_hatches_by_layer = {}
    import re as re_mod
    for h in msp.query('HATCH'):
        try:
            layer = h.dxf.layer
            if not re_mod.match(r'^\d', layer):
                continue
            pts = []
            for path in h.paths:
                if hasattr(path, 'vertices'):
                    pts = [(v[0], v[1]) for v in path.vertices]
                    break
            if len(pts) < 3:
                continue
            h_poly = Polygon(pts)
            if not h_poly.is_valid:
                h_poly = h_poly.buffer(0)
            if h_poly.is_empty:
                continue
            if layer not in all_hatches_by_layer:
                all_hatches_by_layer[layer] = []
            all_hatches_by_layer[layer].append(h_poly)
        except:
            pass

    # 匹配strata_layers到实际DXF图层名
    actual_layer_map = {}
    dxf_layers = list(all_hatches_by_layer.keys())
    for sl in strata_layers:
        if sl in dxf_layers:
            actual_layer_map[sl] = sl
            continue
        best_match = None
        best_score = 0
        for dl in dxf_layers:
            sl_num = re.match(r'^\d+', sl)
            dl_num = re.match(r'^\d+', dl)
            if sl_num and dl_num and sl_num.group() == dl_num.group():
                sl_rest = sl[sl_num.end():]
                dl_rest = dl[dl_num.end():]
                common = set(sl_rest) & set(dl_rest)
                score = len(common) / max(len(sl_rest), len(dl_rest), 1)
                if score > best_score:
                    best_score = score
                    best_match = dl
        actual_layer_map[sl] = best_match if best_match else sl

    LOG(f"[INFO] 预加载HATCH: {sum(len(v) for v in all_hatches_by_layer.values())}个, {len(all_hatches_by_layer)}个图层")

    section_results = []
    backfill_results = []

    for idx, section in enumerate(sections):
        station = section['station']
        dmx_pts = section['dmx_points']
        update_lines_data = section.get('update_lines', [])

        if len(dmx_pts) < 2:
            continue

        dmx_line = LineString(dmx_pts)
        sect_x_min = min(p[0] for p in dmx_pts)
        sect_x_max = max(p[0] for p in dmx_pts)
        sect_y_min = min(p[1] for p in dmx_pts)
        sect_y_max = max(p[1] for p in dmx_pts)

        # 标尺计算高程线Y（逐断面检测）
        target_line_y = None
        if target_elevation is not None:
            elev_to_y = detect_ruler_scale_per_section(doc, msp, sect_x_min, sect_x_max, sect_y_min, sect_y_max)
            if elev_to_y:
                target_line_y = elev_to_y(target_elevation)
            else:
                target_line_y = 5.0 * target_elevation - 27.0

        # 转换更新断面线
        update_lines_shapely = []
        for pts in update_lines_data:
            if len(pts) >= 2:
                update_lines_shapely.append(LineString(pts))

        # 合并断面线
        final_section = dmx_line
        if merge_section and update_lines_shapely:
            final_section = generate_envelope(dmx_line, update_lines_shapely, 'lower')

        # 构建开挖区域多边形
        sect_coords = list(final_section.coords)
        sect_x_min_actual = min(c[0] for c in sect_coords)
        sect_x_max_actual = max(c[0] for c in sect_coords)
        sect_y_min_actual = min(c[1] for c in sect_coords)
        sect_y_max_actual = max(c[1] for c in sect_coords)
        bottom_y = sect_y_min_actual - 50

        total_open_poly = Polygon(
            sect_coords + [(sect_x_max_actual, bottom_y), (sect_x_min_actual, bottom_y)]
        )
        if not total_open_poly.is_valid:
            total_open_poly = total_open_poly.buffer(0)

        if total_open_poly.is_empty:
            sr = {'断面名称': station, '分层线高程': target_elevation, '总面积': 0.0}
            for layer in strata_layers:
                if distinguish_design:
                    sr[f'{layer}_设计'] = 0.0
                    sr[f'{layer}_超挖'] = 0.0
                else:
                    sr[layer] = 0.0
            section_results.append(sr)
            backfill_results.append({'桩号': station, '回淤面积': 0.0})
            continue

        # 高程线裁剪
        layer_open = None
        if target_line_y is None:
            layer_open = total_open_poly
        elif calc_mode == 'below':
            if target_line_y < sect_y_min_actual:
                sr = {'断面名称': station, '分层线高程': target_elevation, '总面积': 0.0}
                for layer in strata_layers:
                    if distinguish_design:
                        sr[f'{layer}_设计'] = 0.0
                        sr[f'{layer}_超挖'] = 0.0
                    else:
                        sr[layer] = 0.0
                section_results.append(sr)
                backfill_results.append({'桩号': station, '回淤面积': 0.0})
                if target_line_y > sect_y_min_actual - 50:
                    output_msp.add_lwpolyline(
                        [(sect_x_min_actual - 5, target_line_y), (sect_x_max_actual + 5, target_line_y)],
                        dxfattribs={'layer': elev_layer_name, 'color': 1}
                    )
                continue
            else:
                clip_poly = box(sect_x_min_actual - 10, sect_y_min_actual - 100,
                                sect_x_max_actual + 10, target_line_y)
                layer_open = total_open_poly.intersection(clip_poly)
        else:
            if target_line_y > sect_y_max_actual:
                sr = {'断面名称': station, '分层线高程': target_elevation, '总面积': 0.0}
                for layer in strata_layers:
                    if distinguish_design:
                        sr[f'{layer}_设计'] = 0.0
                        sr[f'{layer}_超挖'] = 0.0
                    else:
                        sr[layer] = 0.0
                section_results.append(sr)
                backfill_results.append({'桩号': station, '回淤面积': 0.0})
                continue
            else:
                clip_poly = box(sect_x_min_actual - 10, target_line_y,
                                sect_x_max_actual + 10, sect_y_max_actual + 100)
                layer_open = total_open_poly.intersection(clip_poly)

        if layer_open.is_empty:
            sr = {'断面名称': station, '分层线高程': target_elevation, '总面积': 0.0}
            for layer in strata_layers:
                if distinguish_design:
                    sr[f'{layer}_设计'] = 0.0
                    sr[f'{layer}_超挖'] = 0.0
                else:
                    sr[layer] = 0.0
            section_results.append(sr)
            backfill_results.append({'桩号': station, '回淤面积': 0.0})
            continue

        # 画高程线
        if target_line_y is not None:
            if calc_mode == 'below' and target_line_y > sect_y_min_actual:
                output_msp.add_lwpolyline(
                    [(sect_x_min_actual - 5, target_line_y), (sect_x_max_actual + 5, target_line_y)],
                    dxfattribs={'layer': elev_layer_name, 'color': 1}
                )
            elif calc_mode == 'above' and target_line_y < sect_y_max_actual:
                output_msp.add_lwpolyline(
                    [(sect_x_min_actual - 5, target_line_y), (sect_x_max_actual + 5, target_line_y)],
                    dxfattribs={'layer': elev_layer_name, 'color': 1}
                )

        # 构建设计区多边形
        design_polygon = None
        if distinguish_design:
            boundary_box = box(sect_x_min_actual - 20, sect_y_min_actual - 50,
                               sect_x_max_actual + 20, sect_y_max_actual + 50)
            excav_in_section = [l for l in excav_lines_shapely if boundary_box.intersects(l)]
            if excav_in_section:
                design_polygon = build_design_polygon(excav_in_section, sect_x_min_actual, sect_x_max_actual)

        # ===== 分层算量 =====
        boundary_box = box(sect_x_min_actual - 20, sect_y_min_actual - 50,
                           sect_x_max_actual + 20, sect_y_max_actual + 50)
        strata_areas = {}
        total_section_area = 0.0

        for layer in strata_layers:
            design_area = 0.0
            over_area = 0.0
            total_layer_area = 0.0

            design_polys = []
            over_polys = []

            actual_layer = actual_layer_map.get(layer, layer)
            hatch_polys = all_hatches_by_layer.get(actual_layer, [])

            for h_poly in hatch_polys:
                try:
                    if not boundary_box.intersects(h_poly):
                        continue

                    inter = h_poly.intersection(layer_open)
                    if inter.is_empty:
                        continue

                    if distinguish_design and design_polygon:
                        design_part = inter.intersection(design_polygon)
                        over_part = inter.difference(design_polygon)

                        if not design_part.is_empty:
                            if design_part.geom_type == 'Polygon':
                                design_area += design_part.area
                                design_polys.append(design_part)
                            elif hasattr(design_part, 'geoms'):
                                for g in design_part.geoms:
                                    if g.geom_type == 'Polygon':
                                        design_area += g.area
                                        design_polys.append(g)

                        if not over_part.is_empty:
                            if over_part.geom_type == 'Polygon':
                                over_area += over_part.area
                                over_polys.append(over_part)
                            elif hasattr(over_part, 'geoms'):
                                for g in over_part.geoms:
                                    if g.geom_type == 'Polygon':
                                        over_area += g.area
                                        over_polys.append(g)
                    else:
                        if inter.geom_type == 'Polygon':
                            total_layer_area += inter.area
                            design_polys.append(inter)
                        elif hasattr(inter, 'geoms'):
                            for g in inter.geoms:
                                if g.geom_type == 'Polygon':
                                    total_layer_area += g.area
                                    design_polys.append(g)
                except:
                    pass

            if distinguish_design:
                strata_areas[f'{layer}_设计'] = round(design_area, 3)
                strata_areas[f'{layer}_超挖'] = round(over_area, 3)
                total_section_area += design_area + over_area

                color_idx = STRATA_COLORS.get(layer, 7)
                rgb_color = HIGH_CONTRAST_COLORS[strata_layers.index(layer) % len(HIGH_CONTRAST_COLORS)]

                for poly in design_polys:
                    add_hatch_to_dxf(output_msp, poly, f"{target_elevation}m_{layer}_设计", color_idx, rgb_color)
                for poly in over_polys:
                    add_hatch_to_dxf(output_msp, poly, f"{target_elevation}m_{layer}_超挖", color_idx, rgb_color)
            else:
                strata_areas[layer] = round(total_layer_area, 3)
                total_section_area += total_layer_area

                if total_layer_area > 0.01:
                    color_idx = STRATA_COLORS.get(layer, 7)
                    for poly in design_polys:
                        add_hatch_to_dxf(output_msp, poly, f"{target_elevation}m_{layer}", color_idx)

        sr = {
            '断面名称': station,
            '分层线高程': target_elevation,
            **strata_areas,
            '总面积': round(total_section_area, 3)
        }
        section_results.append(sr)

        # ===== 回淤计算 =====
        backfill_area = 0.0
        if update_lines_shapely:
            upper_envelope = generate_envelope(dmx_line, update_lines_shapely, 'upper')

            if upper_envelope:
                dmx_coords = list(dmx_line.coords)
                envelope_coords = list(upper_envelope.coords)

                common_x_min = max(min(c[0] for c in dmx_coords), min(c[0] for c in envelope_coords))
                common_x_max = min(max(c[0] for c in dmx_coords), max(c[0] for c in envelope_coords))

                if common_x_max > common_x_min:
                    x_range = common_x_max - common_x_min
                    num_samples = max(int(x_range / 0.5) + 1, 50)

                    x_samples = []
                    envelope_y_samples = []
                    dmx_y_samples = []

                    for i in range(num_samples + 1):
                        x_current = common_x_min + (common_x_max - common_x_min) * i / num_samples
                        envelope_y = get_y_at_x(upper_envelope, x_current)
                        dmx_y = get_y_at_x(dmx_line, x_current)

                        if envelope_y is not None and dmx_y is not None:
                            x_samples.append(x_current)
                            envelope_y_samples.append(envelope_y)
                            dmx_y_samples.append(dmx_y)

                    if len(x_samples) >= 2:
                        polygon_coords = []
                        for x, y in zip(x_samples, envelope_y_samples):
                            polygon_coords.append((x, y))
                        for i in range(len(x_samples) - 1, -1, -1):
                            polygon_coords.append((x_samples[i], dmx_y_samples[i]))

                        if len(polygon_coords) >= 3:
                            backfill_polygon = Polygon(polygon_coords)
                            if not backfill_polygon.is_valid:
                                backfill_polygon = backfill_polygon.buffer(0)

                            if target_line_y is not None and not backfill_polygon.is_empty:
                                if calc_mode == 'below':
                                    clip_poly = box(common_x_min - 10, sect_y_min_actual - 100,
                                                    common_x_max + 10, target_line_y)
                                    backfill_polygon = backfill_polygon.intersection(clip_poly)
                                else:
                                    clip_poly = box(common_x_min - 10, target_line_y,
                                                    common_x_max + 10, sect_y_max_actual + 100)
                                    backfill_polygon = backfill_polygon.intersection(clip_poly)

                            if not backfill_polygon.is_empty:
                                backfill_area = backfill_polygon.area

                            if backfill_area > 0.01 and not backfill_polygon.is_empty:
                                add_hatch_to_dxf(output_msp, backfill_polygon, backfill_layer, 1, (255, 0, 0))

        backfill_results.append({'桩号': station, '回淤面积': round(backfill_area, 2)})

        if (idx + 1) % 50 == 0:
            LOG(f"  已处理 {idx + 1}/{len(sections)} 个断面...")

    # 保存DXF
    LOG(f"[INFO] 保存DXF: {output_dxf}")
    output_doc.saveas(output_dxf)
    LOG(f"[OK] DXF已保存")

    # 生成Excel
    if section_results and output_xlsx:
        _write_combined_excel(section_results, backfill_results, strata_layers,
                              distinguish_design, target_elevation, calc_mode, output_xlsx)

    return section_results, backfill_results


# ==================== Excel输出 ====================

def _write_autosection_excel(results, strata_layers, distinguish_design,
                              target_elevation, calc_mode, output_xlsx):
    """写入分层算量Excel"""
    df = pd.DataFrame(results)

    with pd.ExcelWriter(output_xlsx, engine='openpyxl') as writer:
        if distinguish_design:
            # 设计量sheet
            design_cols = ['断面名称'] + [c for c in df.columns if c.endswith('_设计')]
            df_design = df[design_cols].copy()
            df_design.columns = ['断面名称'] + [c.replace('_设计', '') for c in df.columns if c.endswith('_设计')]
            df_design.to_excel(writer, sheet_name='设计量', index=False)

            # 超挖量sheet
            over_cols = ['断面名称'] + [c for c in df.columns if c.endswith('_超挖')]
            df_over = df[over_cols].copy()
            df_over.columns = ['断面名称'] + [c.replace('_超挖', '') for c in df.columns if c.endswith('_超挖')]
            df_over.to_excel(writer, sheet_name='超挖量', index=False)

            # 总量sheet
            df_total = df[['断面名称']].copy()
            for layer in strata_layers:
                design_col = f'{layer}_设计'
                over_col = f'{layer}_超挖'
                total_val = 0.0
                if design_col in df.columns:
                    total_val = total_val + df[design_col].fillna(0)
                if over_col in df.columns:
                    total_val = total_val + df[over_col].fillna(0)
                df_total[layer] = total_val
            df_total.to_excel(writer, sheet_name='总量', index=False)
        else:
            df.to_excel(writer, sheet_name='明细表', index=False)

        # 地层汇总
        if distinguish_design:
            summary_data = {'地层': [], '设计面积(㎡)': [], '超挖面积(㎡)': []}
            for layer in strata_layers:
                summary_data['地层'].append(layer)
                design_col = f'{layer}_设计'
                over_col = f'{layer}_超挖'
                summary_data['设计面积(㎡)'].append(df[design_col].sum() if design_col in df.columns else 0.0)
                summary_data['超挖面积(㎡)'].append(df[over_col].sum() if over_col in df.columns else 0.0)
            df_summary = pd.DataFrame(summary_data)
            df_summary['总面积(㎡)'] = df_summary['设计面积(㎡)'] + df_summary['超挖面积(㎡)']
        else:
            strata_cols = [c for c in df.columns if '级' in c]
            summary_data = {'地层': strata_cols, '面积(㎡)': [df[c].sum() for c in strata_cols]}
            df_summary = pd.DataFrame(summary_data)
        df_summary.to_excel(writer, sheet_name='地层汇总', index=False)

        # 汇总
        mode_text = "以下" if calc_mode == 'below' else "以上"
        total_data = {
            '统计项': ['总断面数', f'{target_elevation}m{mode_text}总面积'],
            '数值': [len(results), df['总面积'].sum()]
        }
        pd.DataFrame(total_data).to_excel(writer, sheet_name='汇总', index=False)

    LOG(f"[OK] Excel已保存: {output_xlsx}")

    # 返回总面积供C++解析
    total_area = df['总面积'].sum() if '总面积' in df.columns else 0.0
    print(f"__RESULT__: {json.dumps({'totalArea': round(total_area, 1)})}")
    return total_area


def _write_combined_excel(section_results, backfill_results, strata_layers,
                           distinguish_design, target_elevation, calc_mode, output_xlsx):
    """写入分层+回淤合并Excel"""
    df_section = pd.DataFrame(section_results)
    df_backfill = pd.DataFrame(backfill_results)

    with pd.ExcelWriter(output_xlsx, engine='openpyxl') as writer:
        # Sheet1: 合并明细表
        combined_results = []
        for i, sr in enumerate(section_results):
            row = dict(sr)
            if i < len(backfill_results):
                row['回淤面积'] = backfill_results[i].get('回淤面积', 0.0)
            combined_results.append(row)
        df_combined = pd.DataFrame(combined_results)
        df_combined.to_excel(writer, sheet_name='合并明细表', index=False)

        # Sheet2: 分层算量明细
        if distinguish_design:
            # 设计量sheet
            design_cols = ['断面名称'] + [c for c in df_section.columns if c.endswith('_设计')]
            df_design = df_section[design_cols].copy()
            df_design.columns = ['断面名称'] + [c.replace('_设计', '') for c in df_section.columns if c.endswith('_设计')]
            df_design.to_excel(writer, sheet_name='设计量', index=False)

            # 超挖量sheet
            over_cols = ['断面名称'] + [c for c in df_section.columns if c.endswith('_超挖')]
            df_over = df_section[over_cols].copy()
            df_over.columns = ['断面名称'] + [c.replace('_超挖', '') for c in df_section.columns if c.endswith('_超挖')]
            df_over.to_excel(writer, sheet_name='超挖量', index=False)

            # 总量sheet
            df_total = df_section[['断面名称']].copy()
            for layer in strata_layers:
                design_col = f'{layer}_设计'
                over_col = f'{layer}_超挖'
                total_val = 0.0
                if design_col in df_section.columns:
                    total_val = total_val + df_section[design_col].fillna(0)
                if over_col in df_section.columns:
                    total_val = total_val + df_section[over_col].fillna(0)
                df_total[layer] = total_val
            df_total.to_excel(writer, sheet_name='分层总量', index=False)
        else:
            df_section.to_excel(writer, sheet_name='分层算量明细', index=False)

        # Sheet3: 回淤面积明细
        df_backfill.to_excel(writer, sheet_name='回淤面积明细', index=False)

        # 带合计的回淤
        summary_row = pd.DataFrame([{'桩号': '合计', '回淤面积': df_backfill['回淤面积'].sum()}])
        pd.concat([df_backfill, summary_row], ignore_index=True).to_excel(writer, sheet_name='回淤带合计', index=False)

        # Sheet4: 地层汇总
        if distinguish_design:
            summary_data = {'地层': [], '设计面积(㎡)': [], '超挖面积(㎡)': []}
            for layer in strata_layers:
                summary_data['地层'].append(layer)
                design_col = f'{layer}_设计'
                over_col = f'{layer}_超挖'
                summary_data['设计面积(㎡)'].append(df_section[design_col].sum() if design_col in df_section.columns else 0.0)
                summary_data['超挖面积(㎡)'].append(df_section[over_col].sum() if over_col in df_section.columns else 0.0)
            df_summary = pd.DataFrame(summary_data)
            df_summary['总面积(㎡)'] = df_summary['设计面积(㎡)'] + df_summary['超挖面积(㎡)']
        else:
            strata_cols = [c for c in df_section.columns if '级' in c]
            summary_data = {'地层': strata_cols, '面积(㎡)': [df_section[c].sum() for c in strata_cols]}
            df_summary = pd.DataFrame(summary_data)
        df_summary.to_excel(writer, sheet_name='地层汇总', index=False)

        # Sheet5: 总汇总
        mode_text = "以下" if calc_mode == 'below' else "以上"
        total_data = {
            '统计项': [
                '总断面数',
                f'{target_elevation}m{mode_text}总面积' if target_elevation else '开挖总面积',
                '总回淤面积'
            ],
            '数值': [
                len(combined_results),
                df_section['总面积'].sum(),
                df_backfill['回淤面积'].sum()
            ]
        }
        pd.DataFrame(total_data).to_excel(writer, sheet_name='总汇总', index=False)

    LOG(f"[OK] Excel已保存: {output_xlsx}")

    # 返回总面积供C++解析
    total_section = df_section['总面积'].sum() if '总面积' in df_section.columns else 0.0
    total_backfill = df_backfill['回淤面积'].sum() if '回淤面积' in df_backfill.columns else 0.0
    print(f"__RESULT__: {json.dumps({'totalArea': round(total_section, 1), 'backfillArea': round(total_backfill, 1)})}")
    return {'section': total_section, 'backfill': total_backfill}


# ==================== 入口 ====================

def main():
    if len(sys.argv) < 2:
        print("Usage: autosection_compute.py <input.json>")
        sys.exit(1)

    json_path = sys.argv[1]

    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        task_type = data.get('task_type', 'autosection')

        if task_type == 'autosection':
            compute_autosection(data)
        elif task_type == 'autosection_backfill':
            compute_autosection_backfill(data)
        else:
            print(f"[ERROR] 未知任务类型: {task_type}")
            sys.exit(1)

        sys.exit(0)
    except Exception as e:
        print(f"[ERROR] {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
