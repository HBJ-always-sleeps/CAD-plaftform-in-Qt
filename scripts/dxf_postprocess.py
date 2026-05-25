#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DXF Post-Processor - 用ezdxf处理DXF输出（确保兼容性）
从JSON文件读取实体数据，追加到源DXF文件
"""

import ezdxf
import json
import sys
import os
from datetime import datetime

def process_dxf(input_dxf, output_dxf, entities_json):
    """读取源DXF，追加实体，保存新文件"""

    # 读取源文件（保留完整结构）
    print(f"[INFO] 读取源文件: {input_dxf}")
    doc = ezdxf.readfile(input_dxf)
    msp = doc.modelspace()

    # 读取实体数据
    print(f"[INFO] 读取实体数据: {entities_json}")
    with open(entities_json, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # 创建图层
    layers_created = set()
    for layer_info in data.get('layers', []):
        layer_name = layer_info['name']
        if layer_name not in doc.layers and layer_name not in layers_created:
            color = layer_info.get('color', 7)
            doc.layers.new(name=layer_name, dxfattribs={'color': color})
            layers_created.add(layer_name)
            print(f"  [LAYER] 创建图层: {layer_name}, 颜色={color}")

    # 添加HATCH实体
    hatch_count = 0
    for hatch_data in data.get('hatches', []):
        layer = hatch_data['layer']
        points = hatch_data['points']  # [[x, y], ...]
        color = hatch_data.get('color', 1)

        if len(points) >= 3:
            # 转换为ezdxf格式
            polyline_points = [(p[0], p[1]) for p in points]

            # 创建HATCH
            hatch = msp.add_hatch(
                dxfattribs={
                    'layer': layer,
                    'color': color
                }
            )
            hatch.set_pattern_fill('SOLID', scale=1.0)
            hatch.paths.add_polyline_path(polyline_points, is_closed=True)
            hatch_count += 1

    print(f"  [HATCH] 添加 {hatch_count} 个填充实体")

    # 添加LINE实体
    line_count = 0
    for line_data in data.get('lines', []):
        layer = line_data['layer']
        points = line_data['points']
        color = line_data.get('color', -1)

        if len(points) == 2:
            msp.add_line(
                (points[0][0], points[0][1]),
                (points[1][0], points[1][1]),
                dxfattribs={'layer': layer, 'color': color if color > 0 else 256}
            )
            line_count += 1
        elif len(points) > 2:
            polyline_points = [(p[0], p[1]) for p in points]
            msp.add_lwpolyline(polyline_points, dxfattribs={'layer': layer})
            line_count += 1

    print(f"  [LINE] 添加 {line_count} 个线实体")

    # 添加TEXT实体
    text_count = 0
    for text_data in data.get('texts', []):
        layer = text_data['layer']
        content = text_data['content']
        x = text_data['x']
        y = text_data['y']
        height = text_data.get('height', 3.0)
        color = text_data.get('color', -1)

        msp.add_text(
            content,
            dxfattribs={
                'layer': layer,
                'height': height,
                'color': color if color > 0 else 256
            }
        ).set_placement((x, y), align='LEFT')
        text_count += 1

    print(f"  [TEXT] 添加 {text_count} 个文本实体")

    # 保存文件
    print(f"[INFO] 保存输出文件: {output_dxf}")
    doc.saveas(output_dxf)

    # 验证
    out_size = os.path.getsize(output_dxf) / (1024*1024)
    print(f"[OK] DXF已保存: {output_dxf}")
    print(f"  文件大小: {out_size:.2f} MB")
    print(f"  总实体数: {len(list(doc.entities))}")

    return True

def main():
    if len(sys.argv) < 4:
        print("Usage: dxf_postprocess.py <input_dxf> <output_dxf> <entities_json>")
        sys.exit(1)

    input_dxf = sys.argv[1]
    output_dxf = sys.argv[2]
    entities_json = sys.argv[3]

    try:
        success = process_dxf(input_dxf, output_dxf, entities_json)
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"[ERROR] {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == '__main__':
    main()