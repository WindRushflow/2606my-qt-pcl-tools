# 项目记录
   
---
> v3

## 一、大阶段总览

### v1.0
- 开始日期：2026.4.21
- 完成日期：2026.5.13
- 有效时间：约30个时间段
- 大概讲解：

  - 阶段目标：从零搭建 Qt + PCL 开发环境，完成单一点云全流程处理与多幅点云配准两大核心场景，实现“无需编码、点击操作”的点云处理工具。

  - 阶段成果：完成项目功能构思、自用函数库搭建、VS+Qt Tools+PCL环境配置；完成单一点云读写、预处理、法向量估计、表面重建、可视化等核心功能；完成多幅点云批量加载、一键预处理、SAC-IA粗配准与ICP精配准、结果保存等功能；实现程序独立打包运行。

  - 踩坑与解决：Qt Creator配置PCL频繁报错，转用VS+Qt Tools解决；空指针导致程序崩溃，构造函数统一初始化解决；打包时依赖缺失，梳理依赖库、配置路径解决。

  - 成长与不足：掌握了Qt界面设计与PCL算法联动的完整开发思路；建立起模块化开发与防御性编程习惯；但多线程处理、算法底层原理、中文路径兼容等方面仍有明显不足，后续需重点补强。

  - 标志性产出：完成可独立运行的点云处理工具，支持单点云全流程处理与多幅点云配准；积累了自用PCL函数库与Qt+PCL结合开发的实践经验。

### v2.0
- 开始日期：2026.6.6
- 完成日期：2026.6.20
- 有效时间：约11个时间段
- 大概讲解：重构项目文件夹结构与环境配置，完成新界面搭建与工具栏功能开发。可视化方面，原计划 QVTK 嵌入因 VTK 无 Qt 支持无法实现，放弃该方案，改用常驻独立窗口，确保交互流畅。最终完成单点云全流程处理与多幅点云配准的完整闭环，配套文档体系同步完善，项目具备归档条件。因临近考试，暂停新功能开发，整理后准备复习。


---

## 二、当前大阶段记录 

### 2026.7.29

**总结：编译带 Qt 支持的 VTK 9.4.2 全流程完成，修改 CMakeLists.txt 接入新 VTK，但 CMake 4.4 无法检测 VS 2026 预览版实例。**

1. **确定方案**：用户想重新用 QVTK 嵌入方案替代现有的"常驻独立窗口"方案。AI 分析后确认 PCL 自带的 VTK（9.4）没有 Qt 支持，需要自己编译 VTK。

2. **下载 VTK 9.4.2 源码**：用户从 GitHub 下载 VTK 9.4.2 源码，解压到 `D:\VTK\VTK-9.4.2`。

3. **CMake 配置**：AI 给出 cmake 配置命令，关键参数是 `-DVTK_GROUP_ENABLE_Qt=YES` 开启所有 Qt 模块。
   ```
   cmake ..\VTK-9.4.2 ^
       -G "Visual Studio 18 2026" ^
       -A x64 ^
       -DCMAKE_PREFIX_PATH="D:\Qt\6.11.0\msvc2022_64" ^
       -DVTK_GROUP_ENABLE_Qt=YES ^
       -DCMAKE_INSTALL_PREFIX="D:\VTK\VTK-9.4.2-Qt" ^
       -DVTK_BUILD_TESTING=OFF -DBUILD_TESTING=OFF ^
       -DVTK_BUILD_EXAMPLES=OFF -DVTK_BUILD_DOCUMENTATION=OFF ^
       -DVTK_WRAP_PYTHON=OFF
   ```

4. **踩坑：ParallelDIY 模块 VS 2026 兼容问题**：编译过程中 `diy2/fmt/format.h` 使用了 `stdext::checked_array_iterator`，该符号在 VS 2026（MSVC 19.51）中被移除。AI 通过修改 VTK 源码解决了此问题，在 `format.h` 中添加 `_MSC_VER < 1940` 编译条件判断：
   ```
   修改前：#ifdef _SECURE_SCL
   修改后：#if defined(_SECURE_SCL) && _MSC_VER < 1940
   ```

5. **编译与安装**：用户执行 `cmake --build . --config Release -j8`，最终 171 个 DLL/exe 全部编译成功，无编译错误。然后执行 `cmake --install . --config Release` 安装到 `D:\VTK\VTK-9.4.2-Qt`。

6. **验证及后续**：安装目录包含：
   - Qt 相关 DLL：`vtkGUISupportQt-9.4.dll`、`vtkRenderingQt-9.4.dll`、`vtkViewsQt-9.4.dll` 等
   - 关键头文件：`QVTKOpenGLNativeWidget.h`
   - 完整 cmake 配置
   - 删除`VTK-9.4.2-build`中间编译文件

7. **修改 CMakeLists.txt**：新增以下内容以接入 VTK-Qt：
   ```cmake
   list(PREPEND CMAKE_PREFIX_PATH "D:/VTK/VTK-9.4.2-Qt")
   find_package(VTK REQUIRED COMPONENTS GUISupportQt)
   ```
   并在 `target_link_libraries` 中添加 `VTK::GUISupportQt`。

8. **踩坑：CMake 4.4 无法检测 VS 2026 预览版实例**：
   - 删除旧 build 目录后重新配置时，CMake 报错找不到 VS 2026 实例
   - 使用 VS 2022 生成器 + v145 工具集也无法解决
   - 即使在正确的"VS 2026 开发者命令提示符"中（`VSCMD_VER=18.8.2`），CMake 4.4.0-rc1 依然无法识别该实例
   - Ninja 生成器未安装
   - **待解决**：需安装 Ninja 或等待 CMake 更新支持 VS 2026 预览版


### 2026.7.30

**总结：QVTK 嵌入方案遇到 Qt 6.11 运行时崩溃和 Qt 6.5.3 编译兼容性问题，最终确认 Qt 6.5.3 + VTK 9.4.2 方案可正常创建 QVTKOpenGLNativeWidget。项目文件回退至 v2.0 独立窗口方案。**

1. **实现 QVTK 嵌入改造**：修改 `mainwindow.h/cpp`，将常驻独立窗口方案替换为 QVTKOpenGLNativeWidget 嵌入方案：
   - 删除渲染线程、mutex 锁、pending 数据缓存等线程安全机制
   - 用 vtkSmartPointer&lt;vtkRenderer&gt; + vtkGenericOpenGLRenderWindow 初始化 PCLVisualizer
   - 将 QVTK 创建延迟到 showEvent()，确保 OpenGL 上下文就绪
   - 用户回退了部分改动，项目代码恢复为 v2.0 独立窗口方案

2. **踩坑：Qt 6.11.0 + VTK 9.4.2 QVTK 运行时崩溃**：
   - 最小测试（仅 QVTKOpenGLNativeWidget + QApplication）同样崩溃
   - 错误：`QWidget: Must construct a QApplication before a QWidget`，但确认 QApplication 已成功构造
   - `QT_OPENGL=desktop` 和 `QT_OPENGL=angle` 均无效
   - 纯 QWidget 测试正常，排除 Qt / OpenGL 驱动问题
   - 结论：VTK 9.4.2 的 QVTK 模块与 Qt 6.11.0 存在兼容性问题

3. **尝试 Qt 6.5.3 LTS**：安装 Qt 6.5.3 msvc2019_64（路径 `D:\Qt_\qt6.5.3\6.5.3\msvc2019_64`）
   - VTK 9.4.2 重新编译时遇到 Qt 6.5.3 头文件中的 `stdext::checked_array_iterator` 问题
   - 创建兼容头文件 `D:\VTK\fix_msvc_stdext.h`，提供 `stdext::make_checked_array_iterator` stub
   - 用 `-DCMAKE_CXX_FLAGS="/FI\"D:/VTK/fix_msvc_stdext.h\""` 强制包含

4. **踩坑：Qt 6.5.3 cmake + VTK-Qt 配置冲突**：
   - VTK 编译时绑定了 Qt 6.5.3，但 cmake 查找 Qt 时会找到系统中安装的 Qt 6.11，导致版本不匹配
   - CMake 4.4.0-rc1 无法识别 Qt 6.11 的 `_qt_internal_should_include_targets` 命令
   - 解决：通过 `set(Qt6CoreTools_DIR ...)` 等显式指定 Qt 6.5.3 的工具链路径

5. **关键突破**：Qt 6.5.3 + VTK 9.4.2 最小测试 QVTKOpenGLNativeWidget 创建成功，无崩溃
   - 输出：`QApplication OK` → `QVTKOpenGLNativeWidget created` → `Widget visible: 1`
   - VTK 安装不完整（HDF5 模块编译失败导致 cmake --install 中断）
   - 已手动复制 85 个 DLL 到安装目录 `D:\VTK\VTK-9.4.2-Qt-653`

