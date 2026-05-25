#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DXF Generator Helper - 使用ezdfx库生成兼容的DXF文件
从C++通过QProcess调用
"""

import ezdxf
import json
import sys
import os

def generate_dxf(input_file, output_file, entities_json):
    """从JSON数据生成DXF文件"""

    # 读取源文件作为基础
    try:
        doc = ezdxf.readfile(input_file)
    except:
        # 如果源文件无法读取，创建新文档
        doc = ezdxf.new(dxfversion='AC1032')

    # 加载实体数据
    with open(entities_json, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # 添加图层
    msp = doc.modelspace()
    for layer_info in data.get('layers', []):
        layer_name = layer_info['name']
        if layer_name not in doc.layers:
            doc.layers.new(name=layer_name,
                          color=layer_info.get('color', 7),
                          linetype=layer_info.get('linetype', 'Continuous'))

    # 添加HATCH实体
    for hatch_data in data.get('hatches', []):
        layer = hatch_data['layer']
        points = hatch_data['points']  # [(x, y), ...]

        # 创建HATCH边界
        hatch = msp.add_hatch(color=1, dxfattribs={'layer': layer})
        hatch.paths.add_polyline_path(points, is_closed=True)

    # 保存文件
    doc.saveas(output_file)
    print(f"DXF saved: {output_file}")
    return True

def main():
    if len(sys.argv) < 4:
        print("Usage: dxf_generator.py <input_dxf> <output_dxf> <entities_json>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    entities_json = sys.argv[3]

    try:
        generate_dxf(input_file, output_file, entities_json)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()