# -*- coding: utf-8 -*-
path = r"E:\sdmcfjwrjsscj\practice\VSCode-C++\PointCloud\2606myPclLib\src\registration.cpp"
raw = open(path, "rb").read()
print("CRLF count:", raw.count(b"\r\n"), "| lone LF:", raw.count(b"\n") - raw.count(b"\r\n"))
print("file size:", len(raw))
# 打印 65-68 行的原始字节（按 \r\n 和 \n 都拆）
import re
lines = re.split(b"\r\n|\n", raw)
for i in range(64, 69):
    print(i + 1, repr(lines[i][:130]))
