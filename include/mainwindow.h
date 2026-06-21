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

// 自建pcl数据处理库
#include "mypcl.h"

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
    CONCATENATE,    // 拼接
	REGISTRATION   // 配准结果
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
	MainWindow(QWidget* parent = nullptr);  // 构造函数
	~MainWindow();  // 析构函数


	// ========================== 函数声明 ===========================

#pragma region 工具栏函数实现
    // ========== 加载与保存 ==========

    /**
     * @brief 加载点云文件入口函数（支持 .pcd 和 .ply 格式）
     * @note 加载点云时清空旧点云
     */
    void loadPointCloud();

    /**
     * @brief 加载单个点云
     */
    void loadSingleCloud();

    /**
     * @brief 加载多幅点云
     */
    void loadMultiCloud();

	// ========== 保存 ==========
    /**
     * @brief 保存当前点云为文件（支持 .pcd 和 .ply 格式）
     * @brief 
     */
    void savePointCloud();

    /**
	 * @brief 保存单点云处理结果
	 */
    void saveSingleCloud();

    /**
     * @brief 保存多幅点云处理结果
     */
    void saveMultiCloud();

	// ========== 其他功能 ==========
    /**
	 * @brief 撤销
	 */
    void undoCloudOperation();

    /**
     * @brief 清空当前工程数据
     */
    void clearData();

    /**
	 * @brief 点云居中显示
	 */
    void centerView();

    /**
     * @brief 显示完整点云信息
     */
    void showCloudInfo();

    /**
     * @brief 显示完整日志
     */
    void showFullLog();

    /**
     * @brief 软件使用说明
     */
    void showIntroduction();

#pragma endregion



#pragma region 单个点云处理

    // =======预处理======= 
    /**
     * @brief 去除点云中的NaN点
     */
    void removeNaN();

    /**
     * @brief 体素降采样
     */
    void voxelDownSample();

    /**
     * @brief 均匀滤波
     */
    void uniformFilter();

    /**
     * @brief 直通滤波
     */
    void passThroughFilter();

    /**
     * @brief 统计离群点移除
     */
    void statisticalOutlierRemoval();

    /**
     * @brief 半径离群点移除
     */
    void radiusOutlierRemoval();

    /**
     * @brief MLS平滑处理点云
     */
    void mlsSmoothProcess();

    // =======特征处理======= 
    /**
     * @brief 法向量估计
     */
    void estimateNormal();

    /**
     * @brief 法线方向一致化
     */
    void orientNormalConsistent();

    /**
     * @brief 法线向视点翻转
     */
    void flipNormalToViewpoint();

    /**
     * @brief 法线混乱度评估
     */
    void evaluateNormalDisorder();
    
    // =======表面重建======= 
    /**
     * @brief 贪婪投影三角化重建
     */
    void greedyProjectionTriangulation();

    /**
     * @brief Poisson泊松重建
     */
    void poissonReconstruction();

    /**
     * @brief 重建误差评估
     */
    void evaluateReconstructionError();

    // =========其他=========
    /**
     * @brief 保存当前点云到撤销历史栈
     */
    void pushCloudToUndoStack();



#pragma endregion



#pragma region 多幅点云处理


    /**
     * @brief 点云拼接（简单直接的坐标叠加）
     */
    void concatenateClouds();

    /**
     * @brief 一键预处理所有加载的点云（去NaN、降采样、去噪）
     */
    void on_btn_preprocess_clicked();

	/**
	 * @brief 多幅点云粗配准（SAC-IA）
	 */
    void on_btn_coarse_clicked();

    /**
     * @brief 多幅点云精配准（ICP）
     */
    void on_btn_fine_clicked();


#pragma endregion

#pragma  region 可视化

    // 初始化=====================
    //void initViewer();
    void renderThreadFunc();
    void refreshViewer();

    // 单点云可视化=================
    
    void showPointCloud();

    /**
     * @brief 显示法线点云
     */
    void showNormalCloud();

    /**
     * @brief 显示mesh网格
     */
    void showMesh();

    // 多点云可视化 TODO:待修改==============

    /**
     * @brief 可视化配准结果
     */
    void on_btn_showReg_clicked();

#pragma endregion

public slots:
	void updateViewer(PointCloudPtr cloud, ViewerMode mode);      // 更新点云
	void updateViewer(PolygonMeshPtr mesh, ViewerMode mode);      // 更新网格
    void updateViewer(PointCloudPtr cloud, NormalCloudPtr normals, ViewerMode mode);

private:
    Ui::MainWindow ui;

    ViewerMode m_viewer_mode = ViewerMode::CLOUD;
	// 常驻独立窗口相关
	std::thread render_thread;
	std::atomic<bool> render_stop{ false };
	std::atomic<bool> viewer_created{ false };  // 标记窗口是否已创建
	pcl::visualization::PCLVisualizer::Ptr persistent_viewer;
    
	// 数据缓存（深拷贝后传给渲染线程）
	std::mutex data_mutex;
	PointCloudPtr pending_cloud;          // 用于点云
    NormalCloudPtr pending_normals;         // 法线
	PolygonMeshPtr pending_mesh;          // 用于网格
	ViewerMode pending_mode;              // 待显示的类型
	bool has_pending_data{ false };

    // 点云撤销历史栈
    std::stack<pcl::PointCloud<pcl::PointXYZ>::Ptr> cloudHistoryStack;
    // 最大撤销步数
    const int MAX_UNDO_STEP = 20;

    // 用来接管 cout 的输出
    std::streambuf* old_cout_buf = nullptr;
    std::streambuf* old_cerr_buf = nullptr;
    std::stringstream log_buffer;

    // =================全局数据================
	// 单点云：当前点云
	PointCloudPtr current_cloud;
	NormalCloudPtr normal_cloud;  // 法向量
	PolygonMeshPtr mesh;    // 网格（重建用）

    // 多幅点云配准专用变量
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_cloud_list; // 加载的所有点云
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_coarse_reg_clouds; // 粗配准后的点云
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_coarse_accumulated; // 粗配准拼接结果
    std::vector<Eigen::Matrix4f> m_coarse_transforms; // 粗配准变换矩阵

    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_fine_reg_clouds; // 精配准后的点云
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_fine_accumulated; // 精配准拼接结果
    std::vector<Eigen::Matrix4f> m_fine_transforms; // 精配准变换矩阵

    pcl::PointCloud<pcl::PointXYZ>::Ptr m_registration_result; // 最终配准结果

protected:
    //void showEvent(QShowEvent* event) override;
	//void resizeEvent(QResizeEvent* event) override;

};

