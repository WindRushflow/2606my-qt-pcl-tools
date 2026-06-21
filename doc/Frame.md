# 代码框架设计

> **目标**：理清代码分层，明确“功能按钮”对应“哪个文件的哪个函数”。

---

## 一、代码文件架构
- include/ 头文件
  - mainwindow.h 窗口类
- src/ 源代码
  - main.cpp  程序入口
  - mainwindow.cpp  窗口类函数的实现
- ui/ 界面设计
  - mainwindow.ui 设计界面
mainwindow.ui/.h/.cpp 一一对应

## 二、分文件讲解

### mainwindow.h

#### 程序准备
- 包含头文件
- 设置别名

#### 窗口类 MainWindow
1. public 变量
- 构造函数、析构函数
- 分模块函数
  - 工具栏模块
  - 单点云处理模块
    - 预处理
    - 特征处理
    - 表面重建
    - 其他
  - 多幅点云处理模块
    - 一键预处理
    - 拼接
    - 配准
  - 可视化模块
    - 初始化
    - 单点云可视化
    - 多点云可视化
  - 其他模块

2. private 变量
- 窗口变量ui
- 可视化相关变量
- “撤销”功能变量
- “日志重定向”功能变量
- 单点云和多点云和核心数据变量

3. public slots
按理说应该放响应信号的函数

### mainwindow.cpp

#### 1. 头文件包含
- `mainwindow.h`
- Qt 工具类（QFileDialog、QMessageBox、QTextStream 等）
- PCL 核心头文件（point_types、pcd_io、visualizer）
- 自建库 `mypcl.h`

---

#### 2. 构造函数 MainWindow::MainWindow(QWidget* parent)
- 加载 UI：`ui.setupUi(this)`
- 启动常驻渲染线程：`render_thread = std::thread(&MainWindow::renderThreadFunc, this)`
- 初始化核心数据：`current_cloud`、`normal_cloud`、`mesh`、`m_registration_result`
- 绑定信号槽（connect）：
  - 工具栏（加载/保存/撤销/清空/信息/日志/说明）
  - 预处理（去NaN/体素/均匀/直通/统计/半径/MLS）
  - 特征处理（法向量估计/一致化/翻转/混乱度）
  - 表面重建（贪婪投影/泊松/误差评估）
  - 可视化（显示点云/法线/Mesh）
  - 多幅配准（预处理/粗配准/精配准）
- 日志重定向：接管 `std::cout` 和 `std::cerr`，用 QTimer 定时刷新到 `te_log`
- 自动加载示例点云：`data/rabbit.pcd`

---

#### 3. 析构函数 MainWindow::~MainWindow()
- 恢复 `std::cout` / `std::cerr` 重定向
- 停止渲染线程：`render_stop = true` + `render_thread.join()`

---

#### 4. 工具栏模块（#pragma region 工具栏函数实现）

##### 4.1 加载子模块（#pragma region 打开）
- `loadPointCloud()`：入口，根据当前 tab 分发：
- `loadSingleCloud()`：单选文件对话框 → 清空旧数据 → 加载 PCD/PLY → 打印点云信息
- `loadMultiCloud()`：多选文件对话框 → 清空配准容器 → 批量加载 → 日志输出

##### 4.2 保存子模块（#pragma region 保存）
- `savePointCloud()`：入口，根据当前 tab 分发
- `saveSingleCloud()`：选择目录 → 创建时间戳文件夹 → 保存原始点云/法线合并/Mesh
- `saveMultiCloud()`：选择目录 → 保存原始多幅点云 → 粗配准结果 → 精配准结果 → 最终配准结果

##### 4.3 其他工具栏功能
- `undoCloudOperation()`：从撤销栈弹出并恢复点云 → 自动刷新信息
- `clearData()`：清空所有点云/法线/Mesh/配准容器 → 清空信息栏
- `showCloudInfo()`：捕获 `mypcl::printPointCloudBasicInfo` 输出 → 显示到 `te_Info`
- `showFullLog()`：弹窗显示完整日志
- `showIntroduction()`：弹窗显示工具说明
- `centerView()`：暂未实现（TODO）

---

#### 5. 单点云处理模块（#pragma region 单个点云处理）

##### 5.1 预处理子模块
- `removeNaN()`：入栈 → 调用 `mypcl::removeNaN` → 刷新信息
- `voxelDownSample()`：入栈 → 弹窗输入体素大小 → 调用 `mypcl::doVoxelFilter` → 刷新信息
- `uniformFilter()`：入栈 → 弹窗输入半径 → 调用 `mypcl::doUniformSampling` → 刷新信息
- `passThroughFilter()`：入栈 → 弹窗选择轴/最小值/最大值 → 调用 `mypcl::doPassThrough` → 刷新信息
- `statisticalOutlierRemoval()`：入栈 → 弹窗输入 k 和 std → 调用 `mypcl::doStatisticalOutlierRemoval` → 刷新信息
- `radiusOutlierRemoval()`：入栈 → 弹窗输入半径和最小点数 → 调用 `mypcl::doRadiusOutlierRemoval` → 刷新信息
- `mlsSmoothProcess()`：入栈 → 弹窗输入半径 → 调用 `mypcl::mlsSmooth` → 覆盖 `current_cloud` 和 `normal_cloud` → 刷新信息

##### 5.2 特征处理子模块
- `estimateNormal()`：入栈 → 弹窗输入搜索半径 → 调用 `mypcl::computeNormals` → 刷新信息
- `orientNormalConsistent()`：入栈 → 弹窗输入邻域点数 → 调用 `mypcl::alignNormalsConsistently` → 刷新信息
- `flipNormalToViewpoint()`：调用 `mypcl::flipNormalsToViewpoint`（视点 0,0,0）
- `evaluateNormalDisorder()`：调用 `mypcl::evaluateNormalChaos`

##### 5.3 表面重建子模块
- `greedyProjectionTriangulation()`：弹窗输入搜索半径 → 调用 `mypcl::greedyProjectionTriangulation`
- `poissonReconstruction()`：弹窗输入深度 → 调用 `mypcl::poissonReconstruction`
- `evaluateReconstructionError()`：调用 `mypcl::computeReconstructionError`

##### 5.4 其他
- `pushCloudToUndoStack()`：深拷贝 `current_cloud` → 入栈 → 超过 20 步则弹出最旧

---

#### 6. 多幅点云处理模块（#pragma region 多幅点云处理模块）
- `on_btn_preprocess_clicked()`：弹窗输入体素大小 → 批量对 `m_cloud_list` 执行去NaN + 降采样 + 统计去噪
- `on_btn_coarse_clicked()`：弹窗输入 FPFH 半径和 SAC 半径 → 调用 `mypcl::multipleCoarseRegistration` → 更新 `m_registration_result`
- `on_btn_fine_clicked()`：检查粗配准是否完成 → 弹窗输入 ICP 最大距离和体素大小 → 调用 `mypcl::multipleFineRegistration` → 更新 `m_registration_result`

---

#### 7. 可视化模块（#pragma region 可视化模块）

##### 7.1 初始化与渲染线程
- `renderThreadFunc()`：创建常驻 `PCLVisualizer` 窗口 → 循环检查 `has_pending_data` → 有则清空并重新添加内容 → 调用 `spinOnce(10)` → 检测窗口是否被关闭，若关闭则重建
- `updateViewer(PointCloudPtr, ViewerMode)`：加锁 → 深拷贝点云 → 设置 `pending_mode` → 置 `has_pending_data = true`
- `updateViewer(PolygonMeshPtr, ViewerMode)`：加锁 → 深拷贝 Mesh → 设置模式 → 置标志
- `updateViewer(PointCloudPtr, NormalCloudPtr, ViewerMode)`：加锁 → 深拷贝点云和法线 → 设置模式 → 置标志

##### 7.2 视图刷新
- `refreshViewer()`：根据 `m_viewer_mode` 调用对应的 `updateViewer` 重载，不改变当前模式

##### 7.3 单点云可视化
- `showPointCloud()`：调用 `updateViewer(current_cloud, CLOUD)` → 设置 `m_viewer_mode = CLOUD`
- `showNormalCloud()`：检查 `normal_cloud` 非空 → 调用 `updateViewer(current_cloud, normal_cloud, NORMAL)` → 设置模式
- `showMesh()`：调用 `updateViewer(mesh, MESH)` → 设置模式

##### 7.4 多点云可视化
- `on_btn_showReg_clicked()`：检查 `m_registration_result` 非空 → 新建线程弹出独立 PCLVisualizer 窗口显示配准结果


### main.cpp
入口

## 三、数据流向
单个点云处理界面：加载新点云时自动清空旧数据，确保每次处理都是针对当前加载的点云。
`PointCloudPtr current_cloud;`
`NormalCloudPtr normal_cloud;`  
`PolygonMeshPtr mesh;`    

多幅点云处理
```C++
std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_cloud_list; // 加载的所有点云
std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_coarse_reg_clouds; // 粗配准后的点云
std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_coarse_accumulated; // 粗配准拼接结果
std::vector<Eigen::Matrix4f> m_coarse_transforms; // 粗配准变换矩阵

std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_fine_reg_clouds; // 精配准后的点云
std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_fine_accumulated; // 精配准拼接结果
std::vector<Eigen::Matrix4f> m_fine_transforms; // 精配准变换矩阵

pcl::PointCloud<pcl::PointXYZ>::Ptr m_registration_result; // 最终配准结果
```
