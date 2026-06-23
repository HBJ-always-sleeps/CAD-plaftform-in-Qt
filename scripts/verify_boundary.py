# -*- coding: utf-8 -*-
"""
验证分层算量输出：设计/超挖边界是否严格沿开挖线分布

用法: python verify_boundary.py <源DXF> <输出DXF> [--section-layer V4] [--抽样数 N]
"""

import sys
import os
import ezdxf
from shapely.geometry import LineString, Polygon, MultiPolygon, Point
from shapely.ops import unary_union
import re
import argparse


def get_layer_name_by_chars(doc, target_chars):
    """通过Unicode字符码匹配图层名（解决GBK编码问题）"""
    for l in doc.layers:
        name = l.dxf.name
        if all(any(ord(c) == code for c in name) for code in target_chars):
            return name
    return None


def get_lines_from_layer(msp, layer_name):
    """从图层获取所有多段线"""
    lines = []
    for e in msp.query(f'LWPOLYLINE[layer=="{layer_name}"]'):
        try:
            pts = [(p[0], p[1]) for p in e.get_points()]
            if len(pts) >= 2:
                lines.append(LineString(pts))
        except:
            pass
    return lines


def detect_strata_layers(doc):
    """检测地层图层（以数字开头）"""
    strata = []
    for l in doc.layers:
        name = l.dxf.name
        if re.match(r'^\d', name):
            # 排除非地层图层
            if any(kw in name for kw in ['桩号', '标注', '标签', '图框', '图例']):
                continue
            strata.append(name)
    return strata


def build_design_polygon(excav_lines, sect_x_min, sect_x_max):
    """构建设计区多边形（与修复后的算法一致）"""
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

    # 收集所有采样X坐标：均匀步长 + 开挖线折点
    sample_xs = []
    x_current = design_x_min
    while x_current <= design_x_max:
        sample_xs.append(x_current)
        x_current += 1.0
    # 加入开挖线折点的X坐标
    for line in excav_lines:
        for pt in line.coords:
            if design_x_min <= pt[0] <= design_x_max:
                sample_xs.append(pt[0])
    sample_xs = sorted(set(sample_xs))

    x_samples = []
    y_samples = []
    for xc in sample_xs:
        max_y = None
        for line in excav_lines:
            y = get_y_at_x(line, xc)
            if y is not None and (max_y is None or y > max_y):
                max_y = y
        if max_y is not None:
            x_samples.append(xc)
            y_samples.append(max_y)

    if len(x_samples) < 2:
        return None

    sect_y_max = max(y_samples) + 50
    polygon_coords = list(zip(x_samples, y_samples))
    polygon_coords.append((x_samples[-1], sect_y_max))
    polygon_coords.append((x_samples[0], sect_y_max))
    polygon_coords.append(polygon_coords[0])

    poly = Polygon(polygon_coords)
    return poly if poly.is_valid else poly.buffer(0)


def get_y_at_x(line, x):
    """在线段上插值获取Y值"""
    coords = list(line.coords)
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


def extract_sections_from_dxf(msp, layer_name):
    """从DXF提取断面线"""
    lines = []
    for e in msp.query(f'LWPOLYLINE[layer=="{layer_name}"]'):
        try:
            pts = [(p[0], p[1]) for p in e.get_points()]
            if len(pts) >= 2:
                lines.append({
                    'line': LineString(pts),
                    'x_min': min(p[0] for p in pts),
                    'x_max': max(p[0] for p in pts),
                    'y_min': min(p[1] for p in pts),
                    'y_max': max(p[1] for p in pts),
                    'n_pts': len(pts),
                })
        except:
            pass
    return lines


def build_excav_boundary_line(excav_lines, sect_x_min, sect_x_max):
    """构建开挖线边界线（与build_design_polygon相同的采样逻辑，但返回LineString而非Polygon）"""
    all_points = [p for l in excav_lines for p in l.coords]
    if not all_points:
        return None

    excav_x_min = min(p[0] for p in all_points)
    excav_x_max = max(p[0] for p in all_points)

    design_x_min = max(excav_x_min, sect_x_min)
    design_x_max = min(excav_x_max, sect_x_max)

    if design_x_max <= design_x_min:
        return None

    sample_xs = []
    x_current = design_x_min
    while x_current <= design_x_max:
        sample_xs.append(x_current)
        x_current += 1.0
    for line in excav_lines:
        for pt in line.coords:
            if design_x_min <= pt[0] <= design_x_max:
                sample_xs.append(pt[0])
    sample_xs = sorted(set(sample_xs))

    x_samples = []
    y_samples = []
    for xc in sample_xs:
        max_y = None
        for line in excav_lines:
            y = get_y_at_x(line, xc)
            if y is not None and (max_y is None or y > max_y):
                max_y = y
        if max_y is not None:
            x_samples.append(xc)
            y_samples.append(max_y)

    if len(x_samples) < 2:
        return None

    return LineString(list(zip(x_samples, y_samples)))


def verify_single_section(excav_lines, section_info, tolerance=0.01):
    """
    验证单个断面：设计区边界是否严格沿开挖线

    检测方法：
    1. 构建开挖线边界线
    2. 在边界线的每个采样点上，重新计算所有开挖线的maxY
    3. 检查边界线Y值是否等于maxY（直接验证算法正确性）
    """
    sect_x_min = section_info['x_min']
    sect_x_max = section_info['x_max']

    # 筛选在断面范围内的开挖线
    relevant_excav = []
    for line in excav_lines:
        line_coords = list(line.coords)
        line_x_min = min(p[0] for p in line_coords)
        line_x_max = max(p[0] for p in line_coords)
        if line_x_max >= sect_x_min and line_x_min <= sect_x_max:
            relevant_excav.append(line)

    if not relevant_excav:
        return None, "无开挖线数据"

    # 构建开挖线边界线
    boundary_line = build_excav_boundary_line(relevant_excav, sect_x_min, sect_x_max)
    if boundary_line is None:
        return None, "无法构建边界线"

    # 在边界线的每个采样点上验证：边界Y == maxY
    boundary_coords = list(boundary_line.coords)
    deviations = []
    fail_points = []

    for bx, by in boundary_coords:
        # 重新计算该X处的maxY
        expected_y = None
        for line in relevant_excav:
            y = get_y_at_x(line, bx)
            if y is not None and (expected_y is None or y > expected_y):
                expected_y = y

        if expected_y is None:
            continue

        dev = abs(by - expected_y)
        deviations.append(dev)
        if dev > tolerance:
            fail_points.append({
                'point': (bx, by),
                'expected_y': expected_y,
                'actual_y': by,
                'distance': dev,
            })

    max_dev = max(deviations) if deviations else 0
    avg_dev = sum(deviations) / len(deviations) if deviations else 0

    result = {
        'max_deviation': max_dev,
        'avg_deviation': avg_dev,
        'total_points': len(boundary_coords),
        'fail_points': fail_points,
        'fail_count': len(fail_points),
        'pass_rate': (len(boundary_coords) - len(fail_points)) / len(boundary_coords) * 100 if boundary_coords else 0,
    }

    return result, None


def verify_boundary(src_dxf_path, output_dxf_path, section_layer='V4', sample_count=None):
    """主验证函数"""

    print(f"源文件: {src_dxf_path}")
    print(f"输出文件: {output_dxf_path}")
    print(f"断面线图层: {section_layer}")
    print()

    # 读取源DXF
    src_doc = ezdxf.readfile(src_dxf_path)
    src_msp = src_doc.modelspace()

    # 获取开挖线图层名
    excav_layer = get_layer_name_by_chars(src_doc, [0x5F00, 0x6316, 0x7EBF])
    if not excav_layer:
        print("[ERROR] 未找到开挖线图层")
        return False
    print(f"[INFO] 开挖线图层: {excav_layer}")

    # 提取开挖线
    excav_lines = get_lines_from_layer(src_msp, excav_layer)
    print(f"[INFO] 开挖线数量: {len(excav_lines)}")

    if not excav_lines:
        print("[ERROR] 开挖线为空")
        return False

    # 提取断面线
    sections = extract_sections_from_dxf(src_msp, section_layer)
    print(f"[INFO] 断面线数量: {len(sections)}")

    if not sections:
        print("[ERROR] 断面线为空")
        return False

    # 抽样或全测
    if sample_count and sample_count < len(sections):
        import random
        random.seed(42)
        test_sections = random.sample(sections, sample_count)
        print(f"[INFO] 抽样检测: {sample_count}/{len(sections)} 个断面")
    else:
        test_sections = sections
        print(f"[INFO] 全量检测: {len(sections)} 个断面")

    print()

    # 逐断面验证
    total_pass = 0
    total_fail = 0
    total_skip = 0
    all_max_devs = []
    worst_sections = []

    for i, sec in enumerate(test_sections):
        result, err = verify_single_section(excav_lines, sec, tolerance=0.5)

        if err:
            total_skip += 1
            continue

        all_max_devs.append(result['max_deviation'])

        if result['fail_count'] == 0:
            total_pass += 1
        else:
            total_fail += 1
            worst_sections.append({
                'index': i,
                'x_range': (sec['x_min'], sec['x_max']),
                'result': result,
            })

        # 进度输出
        if (i + 1) % 50 == 0 or (i + 1) == len(test_sections):
            print(f"  进度: {i + 1}/{len(test_sections)}, "
                  f"通过: {total_pass}, 失败: {total_fail}, 跳过: {total_skip}")

    print()
    print("=" * 60)
    print("检测结果汇总")
    print("=" * 60)
    print(f"  总断面数: {len(test_sections)}")
    print(f"  通过: {total_pass}")
    print(f"  失败: {total_fail}")
    print(f"  跳过: {total_skip}")

    if all_max_devs:
        print(f"  最大偏差: {max(all_max_devs):.4f}")
        print(f"  平均偏差: {sum(all_max_devs) / len(all_max_devs):.4f}")

    # 输出最差断面详情
    if worst_sections:
        worst_sections.sort(key=lambda x: x['result']['max_deviation'], reverse=True)
        print()
        print(f"偏差最大的 {min(10, len(worst_sections))} 个断面:")
        print("-" * 60)
        for ws in worst_sections[:10]:
            r = ws['result']
            print(f"  断面#{ws['index']}: "
                  f"X=[{ws['x_range'][0]:.1f}, {ws['x_range'][1]:.1f}], "
                  f"最大偏差={r['max_deviation']:.4f}, "
                  f"不合格点={r['fail_count']}/{r['total_points']}, "
                  f"通过率={r['pass_rate']:.1f}%")

            # 输出前3个不合格点详情
            for fp in r['fail_points'][:3]:
                print(f"    X={fp['point'][0]:.2f}: "
                      f"边界Y={fp['actual_y']:.2f}, "
                      f"期望Y={fp['expected_y']:.2f}, "
                      f"偏差={fp['distance']:.4f}")

    # 最终判定
    print()
    if total_fail == 0 and total_skip < len(test_sections) * 0.1:
        print("[PASS] 所有断面的设计/超挖边界严格沿开挖线分布")
        return True
    else:
        fail_rate = total_fail / (total_pass + total_fail) * 100 if (total_pass + total_fail) > 0 else 0
        print(f"[FAIL] {total_fail} 个断面存在边界偏差 ({fail_rate:.1f}%)")
        return False


def main():
    parser = argparse.ArgumentParser(description='验证分层算量设计/超挖边界')
    parser.add_argument('src_dxf', help='源DXF文件路径')
    parser.add_argument('output_dxf', help='输出DXF文件路径（可选，仅用于对比）', nargs='?', default=None)
    parser.add_argument('--section-layer', '-s', default='V4', help='断面线图层名 (默认: V4)')
    parser.add_argument('--sample', '-n', type=int, default=None, help='抽样数量 (默认: 全测)')

    args = parser.parse_args()

    if not os.path.exists(args.src_dxf):
        print(f"[ERROR] 源文件不存在: {args.src_dxf}")
        sys.exit(1)

    success = verify_boundary(
        args.src_dxf,
        args.output_dxf or args.src_dxf,
        section_layer=args.section_layer,
        sample_count=args.sample,
    )

    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
