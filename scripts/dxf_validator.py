#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DXF Validator - 验证并修复DXF文件以确保ZWCAD兼容性
"""

import ezdxf
import sys
import os

def validate_and_fix(input_file):
    """验证并修复DXF文件"""

    print(f"Validating: {input_file}")

    try:
        # 读取文件
        doc = ezdxf.readfile(input_file)
        print(f"  Version: {doc.dxfversion}")
        print(f"  Layers: {len(doc.layers)}")
        print(f"  Entities: {len(list(doc.entities))}")

        # 检查必要元素
        issues = []

        # 检查HEADER
        try:
            ver = doc.header.get('$ACADVER', None)
            if ver is None:
                issues.append("Missing $ACADVER")
        except:
            issues.append("Cannot read header")

        # 检查图层表
        if len(doc.layers) == 0:
            issues.append("No layers defined")

        if issues:
            print(f"  Issues found: {issues}")
        else:
            print("  Validation OK")

        # 保存修复后的文件（ezdxf会自动修复结构）
        output_file = input_file.replace('.dxf', '_validated.dxf')
        doc.saveas(output_file)
        print(f"  Saved validated file: {output_file}")

        return True

    except Exception as e:
        print(f"  Error: {e}")
        return False

def main():
    if len(sys.argv) < 2:
        print("Usage: dxf_validator.py <input_dxf>")
        sys.exit(1)

    input_file = sys.argv[1]
    success = validate_and_fix(input_file)
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()