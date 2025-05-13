import codecs

def gb2312_to_unicode():
    unicode_list = []
    for high in range(0xB0, 0xD8):  # GB2312 一级汉字范围
        for low in range(0xA1, 0xFF):
            gb2312 = bytes([high, low])
            try:
                char = gb2312.decode('gb2312')
                unicode_list.append(ord(char))  # 获取 Unicode 编码
            except UnicodeDecodeError:
                continue
    return sorted(unicode_list)  # 升序排列

def merge_intervals(unicode_list):
    intervals = []
    start = unicode_list[0]
    end = unicode_list[0]

    for code in unicode_list[1:]:
        if code == end + 1:  # 如果是连续的
            end = code
        else:
            intervals.append((start, end))
            start = code
            end = code
    intervals.append((start, end))  # 添加最后一个区间
    return intervals

# 获取 GB2312 一级汉字的 Unicode 编码
unicode_list = gb2312_to_unicode()

# 合并连续的 Unicode 编码为区间
intervals = merge_intervals(unicode_list)

# 输出结果
with open("unicode_intervals.txt", "w", encoding="utf-8") as f:
    for start, end in intervals:
        if start == end:
            f.write(f"(0x{start:04X}, 0x{start:04X}),\n")
        else:
            f.write(f"(0x{start:04X}, 0x{end:04X}),\n")

print("Unicode 区间已生成：unicode_intervals.txt")