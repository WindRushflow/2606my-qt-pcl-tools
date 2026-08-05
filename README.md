# [2606myQtPclTools]

> 一句话描述：基于Qt + PCL的点云处理工具箱，支持单点云和多幅点云处理。

## 项目目标

### 项目技术目标
- 实现点云数据的加载、预处理、特征提取、网格重建与可视化，掌握点云处理的完整流程。
- 构建功能完善、界面直观、运行稳定的点云工具，支持多幅点云批量导入、批量处理与配准。
- 将核心点云处理逻辑封装为静态库，与 Qt 界面分离，提升后续扩展和复用能力。
- 完善用户体验：支持操作撤销、日志重定向和参数交互输入，降低使用门槛。
- 支持点云、法向量和网格模型按需保存，形成可追溯的处理闭环。

### 个人能力与长期目标
- 提升软件工程能力，重点练习模块化设计、异常处理、性能优化和界面交互设计。
- 深化工程化实践，掌握基于 CMake 的跨平台构建、模块化开发与静态库封装。
- 通过解决编译、IDE 索引、Qt/PCL 集成等问题，提高复杂环境下的调试能力。
- 为后续语义分割、场景理解等复杂点云项目准备可复用代码和经验。
- 通过项目成果展示个人技术积累，增强职业竞争力。

## 开发环境
- Qt [6.11.0] / PCL [1.15.1] / VTK [9.4]（PCL自带）
- IDE：Visual Studio 2026 预览版（版本号 17.14，工具集 v145）
- 构建系统：CMake [3.21]（生成器：Visual Studio 17 2022）
- 依赖库：mypcllib（静态库，位于 ../2606myPclLib/output/Debug/）

## 资料目录
2606myQtPclTools/        
├── build/
├── data/
├── doc/
│   ├── Designments.md
│   ├── Frame.md
│   ├── Records.md
│   ├── RoadMap.md
│   ├── functionDesign/
│   └── notes.md
├── include/
│   └── mainwindow.h
├── output/
│   ├── Debug/
│   ├── SingleCloud_20260612_222624/
│   └── lib/
├── src/
│   ├── main.cpp
│   └── mainwindow.cpp
├── ui/
│   └── mainwindow.ui
├── CMakeLists.txt
└── README.md