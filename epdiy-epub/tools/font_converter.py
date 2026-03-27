#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import argparse
from pathlib import Path

def font_to_c_array(font_file_path, array_name="xiaozhi_font"):
    font_path = Path(font_file_path)
    
    if not font_path.exists():
        raise FileNotFoundError(f"字体文件不存在: {font_file_path}")
    
    # 输出文件路径：与字体文件同目录，命名为 {array_name}.c
    output_path = font_path.parent / f"{array_name}.c"
    
    # 读取字体文件
    with open(font_path, 'rb') as f:
        font_data = f.read()
    
    font_size = len(font_data)
    
    c_content = f" __attribute__((section(\".font_data\"))) const unsigned char {array_name}[{font_size}] = {{\n"
    
    # 将字体数据转换为C数组格式，每行12个字节
    bytes_per_line = 12
    for i in range(0, font_size, bytes_per_line):
        line_data = font_data[i:i + bytes_per_line]
        hex_values = ', '.join(f'0x{b:02X}' for b in line_data)
        
        if i + bytes_per_line < font_size:
            c_content += f"\t{hex_values},\n"
        else:
            c_content += f"\t{hex_values}\n"
    
    c_content += "};\n\n\n"
    c_content += f"const int {array_name}_size = sizeof({array_name});\n"
    
    # 写入C文件
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(c_content)
    
    print(f"字体转换完成!")
    print(f"输入文件: {font_path}")
    print(f"输出文件: {output_path}")
    print(f"数组名称: {array_name}")
    print(f"文件大小: {font_size} bytes")
    
    return output_path

def main():
    parser = argparse.ArgumentParser(description='字体文件转换工具')
    parser.add_argument('font_file', help='字体文件路径')
    parser.add_argument('-n', '--name', default='xiaozhi_font', help='C数组名称 (默认: xiaozhi_font)')
    
    args = parser.parse_args()
    
    try:
        font_to_c_array(args.font_file, args.name)
        return 0
    except Exception as e:
        print(f"错误: {e}", file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())
