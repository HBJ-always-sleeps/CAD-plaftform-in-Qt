#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""测试ezdxf HATCH颜色设置"""
import ezdxf

doc = ezdxf.new()
msp = doc.modelspace()

# Create HATCH with ACI color only
hatch1 = msp.add_hatch(dxfattribs={'layer': 'test1'})
hatch1.set_solid_fill(color=11)  # Use set_solid_fill for HATCH color
hatch1.paths.add_polyline_path([(0,0), (10,0), (10,10), (0,10)], is_closed=True)

# Create HATCH with ACI color + RGB override
hatch2 = msp.add_hatch(dxfattribs={'layer': 'test2'})
hatch2.set_solid_fill(color=31, rgb=(255, 0, 0))  # Both ACI and RGB
hatch2.paths.add_polyline_path([(20,0), (30,0), (30,10), (20,10)], is_closed=True)

doc.saveas('D:/QtCADPlatform/test_hatch_color.dxf')
print("DXF saved")

# Re-read and check colors
doc2 = ezdxf.readfile('D:/QtCADPlatform/test_hatch_color.dxf')
for h in doc2.modelspace().query('HATCH'):
    layer = h.dxf.layer
    aci = h.dxf.color
    rgb = h.rgb if hasattr(h, 'rgb') else None
    print(f"Layer: {layer}, ACI: {aci}, RGB: {rgb}")