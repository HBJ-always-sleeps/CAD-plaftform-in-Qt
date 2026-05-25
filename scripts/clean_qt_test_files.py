#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qt测试文件清理脚本
只删除以QtTest_开头的测试生成文件，不删除用户提供的参考文件
"""

import os
import glob

def clean_qt_test_files(test_dir):
    """只删除QtTest_前缀的测试文件"""

    # 查找所有QtTest_开头的文件
    patterns = ['QtTest_*.dxf', 'QtTest_*.xlsx', 'QtTest_*.json']

    deleted_count = 0
    deleted_files = []

    for pattern in patterns:
        full_pattern = os.path.join(test_dir, pattern)
        for filepath in glob.glob(full_pattern):
            try:
                os.remove(filepath)
                deleted_count += 1
                deleted_files.append(os.path.basename(filepath))
            except Exception as e:
                print(f"删除失败: {filepath} - {e}")

    return deleted_count, deleted_files

if __name__ == '__main__':
    import sys

    if len(sys.argv) < 2:
        test_dir = r'D:\断面算量平台\测试文件'
    else:
        test_dir = sys.argv[1]

    print(f"清理目录: {test_dir}")
    print("只删除 QtTest_ 开头的测试文件")

    count, files = clean_qt_test_files(test_dir)

    print(f"已删除 {count} 个文件:")
    for f in files[:20]:  # 只显示前20个
        print(f"  {f}")
    if len(files) > 20:
        print(f"  ... 还有 {len(files)-20} 个文件")

    print("\n保留的参考文件（不含QtTest_前缀）未被删除")