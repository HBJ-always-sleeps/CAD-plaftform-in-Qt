#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""验证DXF HATCH边界完整性"""
import os
import ezdxf

out_dir = r'D:\断面算量平台\测试文件'
dxf_files = [(f, os.path.getmtime(os.path.join(out_dir, f))) for f in os.listdir(out_dir) if f.endswith('.dxf') and '20260522' in f]
dxf_files.sort(key=lambda x: x[1], reverse=True)

if dxf_files:
    dxf_path = os.path.join(out_dir, dxf_files[0][0])
    print(f"检查: {dxf_files[0][0]}")

    doc = ezdxf.readfile(dxf_path)
    msp = doc.modelspace()

    # 验证HATCH边界
    valid_count = 0
    invalid_count = 0
    for h in msp.query('HATCH'):
        layer = h.dxf.layer
        if '设计' not in layer and '超挖' not in layer:
            continue

        pts = []
        for path in h.paths:
            if hasattr(path, 'vertices'):
                pts = list(path.vertices)
                break

        if len(pts) >= 3:
            valid_count += 1
        else:
            invalid_count += 1
            print(f"  无效HATCH: {layer}, pts={len(pts)}")

    print(f"\nHATCH验证:")
    print(f"  有效(>=3点): {valid_count}")
    print(f"  无效(<3点): {invalid_count}")

    # 检查颜色设置
    color_layers = {}
    for h in msp.query('HATCH'):
        layer = h.dxf.layer
        color = h.dxf.color
        # 检查是否有RGB true color
        rgb = h.rgb if hasattr(h, 'rgb') else None
        if '设计' in layer or '超挖' in layer or '-4m' in layer or '0m' in layer:
            key = f"{layer}_ACI{color}"
            if rgb:
                key += f"_RGB{rgb}"
            color_layers[layer] = color_layers.get(layer, [])
            color_layers[layer].append((color, rgb))

    print(f"\nHATCH颜色设置:")
    for layer, colors in sorted(color_layers.items())[:10]:
        print(f"  {layer}: ACI={colors[0][0]}, RGB={colors[0][1]}")