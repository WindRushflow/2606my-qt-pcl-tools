# -*- coding: utf-8 -*-
path = r"E:\sdmcfjwrjsscj\practice\VSCode-C++\PointCloud\2606myPclLib\src\registration.cpp"
with open(path, encoding="utf-8", newline="") as f:
    t = f.read()

old = "sac.setMaximumIterations(1000);"
new = "sac.setMaximumIterations(300);  // 1000->300: SAC-IA 是粗配准大头(77%), 降迭代换时间; fitness 需观察"
cnt = t.count(old)
print("1000 iterations occurrences:", cnt)
assert cnt == 1, "unexpected count"
t = t.replace(old, new)
with open(path, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("replaced OK:", "setMaximumIterations(300)" in t)
