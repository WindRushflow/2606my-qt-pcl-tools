#pragma once

// Qt主窗口基类
#include <QtWidgets/QMainWindow>
// UI界面自动生成文件
#include "ui_mainwindow.h"

// Qt工具类
#include <QTimer>        // 定时器
#include <QFileDialog>   // 文件弹窗
#include <QInputDialog>  // 输入弹窗
#include <QDir>          // 文件夹操作
#include <QDateTime>     // 时间戳
#include <pcl/visualization/pcl_visualizer.h>

// C++标准容器
#include <stack>          // 栈，用于撤销功能

// C++标准IO
#include <iostream>       // 控制台打印
#include <sstream>        // 字符串格式化
#include <mutex>
#include <atomic>

// C++多线程
#include <thread>         // 异步处理耗时点云运算

// QtConcurrent 多线程（粗配准后台执行，UI 不卡死）
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

// 自建pcl数据处理库
#include "mypcl.h"

// 线程安全日志缓冲（库线程写日志 / 主线程定时刷新共用）
#include "threadsafelogbuf.h"

// 别名
// PCL点云基础类型别名
using PointXYZ = pcl::PointXYZ;
using PointCloud = pcl::PointCloud<PointXYZ>;
using PointCloudPtr = PointCloud::Ptr;
// PCL法线相关类型别名
using Normal = pcl::Normal;
using NormalCloud = pcl::PointCloud<pcl::Normal>;
using NormalCloudPtr = NormalCloud::Ptr;
// PCL网格模型
using PolygonMeshPtr = pcl::PolygonMesh::Ptr;
// PCL FPFH特征描述子
using FPFHSignature33 = pcl::FPFHSignature33;
using FPFHCloudPtr = pcl::PointCloud<FPFHSignature33>::Ptr;
// Eigen变换矩阵
using Matrix4f = Eigen::Matrix4f;
// PCL点索引容器
using PointIndicesPtr = pcl::PointIndices::Ptr;


// Viewer显示状态变量管理
enum class ViewerMode
{
    CLOUD,         // 点云
    NORMAL,        // 法线
    MESH,          // 网格
    CONCATENATE,   // 拼接
    REGISTRATION   // 配准结果
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);  // 构造函数
    ~MainWindow();                          // 析构函数

#pragma region 工具栏函数实现

    void loadPointCloud();               // 加载点云文件入口，根据当前Tab分发
    void loadSingleCloud();              // 加载单幅点云（单选文件对话框）
    void loadMultiCloud();               // 加载多幅点云（多选文件，批量导入）

    void savePointCloud();               // 保存点云入口，根据当前Tab分发
    void saveSingleCloud();              // 保存单点云（原始+法线+Mesh，自动创建时间戳文件夹）
    void saveMultiCloud();               // 保存多幅点云（原始+粗配准+精配准结果）

    void undoCloudOperation();           // 撤销上一次点云操作
    void clearData();                    // 清空所有点云数据
    void centerView();                   // 居中显示（TODO: 待实现）
    void showCloudInfo();                // 显示点云基本信息到信息栏
    void showFullLog();                  // 弹窗显示完整运行日志
    void showIntroduction();             // 弹窗显示软件使用说明

#pragma endregion

#pragma region 单个点云处理

    void removeNaN();                    // 去除点云中的 NaN 点
    void voxelDownSample();              // 体素降采样
    void uniformFilter();                // 均匀滤波
    void passThroughFilter();            // 直通滤波（指定轴和范围）
    void statisticalOutlierRemoval();    // 统计离群点移除
    void radiusOutlierRemoval();         // 半径离群点移除
    void mlsSmoothProcess();             // MLS 平滑处理

    void estimateNormal();               // 法向量估计
    void orientNormalConsistent();       // 法线方向一致化
    void flipNormalToViewpoint();        // 法线向视点翻转
    void evaluateNormalDisorder();       // 法线混乱度评估

    void greedyProjectionTriangulation(); // 贪婪投影三角化重建
    void poissonReconstruction();        // Poisson 泊松重建
    void evaluateReconstructionError();  // 重建误差评估

    void pushCloudToUndoStack();         // 当前点云入撤销栈

#pragma endregion

#pragma region 多幅点云处理

    void concatenateClouds();            // 点云拼接（坐标直接叠加）
    void on_btn_preprocess_clicked();    // 一键预处理：去NaN + 降采样 + 去噪
    void on_btn_coarse_clicked();        // SAC-IA 粗配准
    void on_btn_fine_clicked();          // ICP 精配准
    void onCoarseFinished();             // 粗配准后台执行完成回调（主线程）
    void onPreprocessFinished();         // 批量预处理后台执行完成回调（主线程）
    void onFineFinished();               // 精配准后台执行完成回调（主线程）

#pragma endregion

#pragma region 可视化

    void renderThreadFunc();             // 常驻渲染线程函数
    void refreshViewer();                // 按当前模式刷新可视化窗口
    void showPointCloud();               // 显示原始点云
    void showNormalCloud();              // 显示法向量点云
    void showMesh();                     // 显示 Mesh 网格模型
    void on_btn_showReg_clicked();       // 显示配准结果

#pragma endregion

public slots:
    void updateViewer(PointCloudPtr cloud, ViewerMode mode);                 // 更新点云显示
    void updateViewer(PolygonMeshPtr mesh, ViewerMode mode);                 // 更新网格显示
    void updateViewer(PointCloudPtr cloud, NormalCloudPtr normals, ViewerMode mode); // 更新点云+法线显示

private:
    Ui::MainWindow ui;

    ViewerMode m_viewer_mode = ViewerMode::CLOUD;
    // 常驻独立窗口相关
    std::thread render_thread;
    std::atomic<bool> render_stop{ false };
    std::atomic<bool> viewer_created{ false };  // 标记窗口是否已创建
    std::atomic<bool> center_view_requested{ false };  // 居中显示请求标志（渲染线程消费）
    pcl::visualization::PCLVisualizer::Ptr persistent_viewer;

    // 数据缓存（深拷贝后传给渲染线程）
    std::mutex data_mutex;
    PointCloudPtr pending_cloud;          // 用于点云
    NormalCloudPtr pending_normals;       // 法线
    PolygonMeshPtr pending_mesh;          // 用于网格
    ViewerMode pending_mode;              // 待显示的类型
    bool has_pending_data{ false };

    // 点云撤销历史栈
    std::stack<pcl::PointCloud<pcl::PointXYZ>::Ptr> cloudHistoryStack;
    const int MAX_UNDO_STEP = 20;         // 最大撤销步数

    // 用来接管 cout 的输出
    std::streambuf* old_cout_buf = nullptr;
    std::streambuf* old_cerr_buf = nullptr;
    ThreadSafeLogBuf log_buffer;   // 线程安全：工作线程写、主线程定时读

    // ==== 全局数据 ====
    PointCloudPtr current_cloud;           // 当前点云
    NormalCloudPtr normal_cloud;           // 法向量
    PolygonMeshPtr mesh;                   // 网格（重建用）

    // 多幅点云配准专用变量
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_cloud_list;           // 加载的所有点云
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_coarse_reg_clouds;    // 粗配准后的点云
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_coarse_accumulated;   // 粗配准拼接结果
    std::vector<Eigen::Matrix4f> m_coarse_transforms;                        // 粗配准变换矩阵

    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_fine_reg_clouds;      // 精配准后的点云
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_fine_accumulated;     // 精配准拼接结果
    std::vector<Eigen::Matrix4f> m_fine_transforms;                          // 精配准变换矩阵

    pcl::PointCloud<pcl::PointXYZ>::Ptr m_registration_result;               // 最终配准结果
    PointCloudPtr m_concat_result;                     // 点云拼接结果（坐标直接叠加）

    // 后台任务多线程（QtConcurrent）：预处理 / 粗配准 / 精配准
    // 同一时刻只允许一个后台任务（三者共享 m_cloud_list / m_coarse_* / m_fine_* 数据）
    QFutureWatcher<void>* m_preprocess_watcher = nullptr; // 预处理 future 监听器
    QFutureWatcher<void>* m_coarse_watcher = nullptr;     // 粗配准 future 监听器
    QFutureWatcher<void>* m_fine_watcher = nullptr;       // 精配准 future 监听器
    std::atomic<bool> m_busy_running{ false };            // 任一后台任务正在执行
    void setBusyUI(bool busy);                        // 任务期间禁用共享数据相关控件

protected:
    //void showEvent(QShowEvent* event) override;
    //void resizeEvent(QResizeEvent* event) override;

};
