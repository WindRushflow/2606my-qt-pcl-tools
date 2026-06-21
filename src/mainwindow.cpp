#include "mainwindow.h"

#include <QFileDialog>  // 文件选择框
#include <QMessageBox>  // 弹出提示框
#include <QTextStream>  // 

#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/pcl_visualizer.h>

#include <vector>
#include <pcl/point_cloud.h>

#include "mypcl.h"


// 构造函数
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

	// ========== 新增：启动常驻渲染线程 ==========
	render_stop = false;
	render_thread = std::thread(&MainWindow::renderThreadFunc, this);


    // ============初始化当前点云============
    current_cloud.reset(new PointCloud);
    normal_cloud.reset(new pcl::PointCloud<pcl::Normal>);
    mesh.reset(new pcl::PolygonMesh);
    m_registration_result.reset(new PointCloud);

    // ==================== 工具栏函数 =======================
    // 加载与保存
    connect(ui.action_load, &QAction::triggered, this, &MainWindow::loadPointCloud);
    connect(ui.action_save, &QAction::triggered, this, &MainWindow::savePointCloud);
    // 点云信息
    connect(ui.action_showInfo, &QAction::triggered, this, &MainWindow::showCloudInfo);
    // 撤销
    connect(ui.action_undo, &QAction::triggered, this, &MainWindow::undoCloudOperation);
	// 清空
    connect(ui.action_clear, &QAction::triggered, this, &MainWindow::clearData);
    // 居中显示
	connect(ui.action_centered, &QAction::triggered, this, &MainWindow::centerView);
    // 完整日志
	connect(ui.action_showLog, &QAction::triggered, this, &MainWindow::showFullLog);
    // 工具说明
	connect(ui.action_Intro, &QAction::triggered, this,&MainWindow::showIntroduction);

    // ===================== 预处理模块 =====================
    connect(ui.btn_removeNaN, &QPushButton::clicked, this, &MainWindow::removeNaN);
    connect(ui.btn_voxel, &QPushButton::clicked, this, &MainWindow::voxelDownSample);
    connect(ui.btn_uniform, &QPushButton::clicked, this, &MainWindow::uniformFilter);
    connect(ui.btn_passthrough, &QPushButton::clicked, this, &MainWindow::passThroughFilter);
    connect(ui.btn_staticsRemove, &QPushButton::clicked, this, &MainWindow::statisticalOutlierRemoval);
    connect(ui.btn_radiusRemove, &QPushButton::clicked, this, &MainWindow::radiusOutlierRemoval);
    connect(ui.btn_mls, &QPushButton::clicked, this, &MainWindow::mlsSmoothProcess);

    // ===================== 特征模块 =====================
    connect(ui.btn_NE, &QPushButton::clicked, this, &MainWindow::estimateNormal);
    connect(ui.btn_normalsAlign, &QPushButton::clicked, this, &MainWindow::orientNormalConsistent);
    connect(ui.btn_flipNormal, &QPushButton::clicked, this, &MainWindow::flipNormalToViewpoint);
    connect(ui.btn_normalsChaos, &QPushButton::clicked, this, &MainWindow::evaluateNormalDisorder);

    // ===================== 重建模块 =====================
    connect(ui.btn_greedy, &QPushButton::clicked, this, &MainWindow::greedyProjectionTriangulation);
    connect(ui.btn_poisson, &QPushButton::clicked, this, &MainWindow::poissonReconstruction);
    connect(ui.btn_reconError, &QPushButton::clicked, this, &MainWindow::evaluateReconstructionError);

    // ===================== 可视化模块 =====================
    connect(ui.btn_showCloud, &QPushButton::clicked, this, &MainWindow::showPointCloud);
    connect(ui.btn_showNormal, &QPushButton::clicked, this, &MainWindow::showNormalCloud);
    connect(ui.btn_showMesh, &QPushButton::clicked, this, &MainWindow::showMesh);

    // ========== 多幅点云配准 ==========
    // UI provides btn_preprocess, btn_coarse, btn_fine and visualization buttons
    connect(ui.btn_preprocess, &QPushButton::clicked, this, &MainWindow::on_btn_preprocess_clicked);
    connect(ui.btn_coarse, &QPushButton::clicked, this, &MainWindow::on_btn_coarse_clicked);
    connect(ui.btn_fine, &QPushButton::clicked, this, &MainWindow::on_btn_fine_clicked);

    // ====================== 日志重定向到 te_log ======================
    old_cout_buf = std::cout.rdbuf(log_buffer.rdbuf());
    old_cerr_buf = std::cerr.rdbuf(log_buffer.rdbuf());

    // 用定时器定时把日志刷到界面
    QTimer* log_timer = new QTimer(this);
    connect(log_timer, &QTimer::timeout, this, [this]() {
        if (!log_buffer.str().empty()) {
            // mainwindow.ui has a single te_log and te_Info for info
            ui.te_log->append(QString::fromStdString(log_buffer.str()));
            log_buffer.str("");
            log_buffer.clear();

            // 自动滚动到底部
            QTextCursor cursor = ui.te_log->textCursor();
            cursor.movePosition(QTextCursor::End);
            ui.te_log->setTextCursor(cursor);
        }
        });
    log_timer->start(50); // 每50ms刷新一次


	// ========== 自动加载示例点云 ===========
	QDir dir(QCoreApplication::applicationDirPath());
	dir.cdUp();	dir.cdUp();
	QString path = dir.absoluteFilePath("data/rabbit.pcd");
	QFileInfo check(path);
	if (check.exists()){
		current_cloud->clear();

		if (pcl::io::loadPCDFile(path.toStdString(), *current_cloud) == 0){
			LOG_INFO("Auto load success: " << path.toStdString());
			showPointCloud();
		}
		else{
			LOG_ERROR("Auto load failed.");
		}
	}
	else{
		LOG_ERROR("Default file not found: " << path.toStdString());
	}
}

MainWindow::~MainWindow()
{
    std::cout.rdbuf(old_cout_buf);
    std::cerr.rdbuf(old_cerr_buf);

	render_stop = true;
	if (render_thread.joinable())
		render_thread.join();
}

#pragma region 工具栏函数实现

#pragma region 打开

// =============加载=============


void MainWindow::loadPointCloud()
{    
	QWidget* currentTab = ui.Left_tab->currentWidget();

	if (currentTab == ui.tab_single){
		loadSingleCloud();
	}
	else if (currentTab == ui.tab_multi){
		loadMultiCloud();
	}

}

void MainWindow::loadSingleCloud()
{

	QString path = QFileDialog::getOpenFileName(
		this, "select cloud file", "", "(*.pcd *.ply)"
	);

	if (path.isEmpty()) {
		LOG_ERROR(path.toStdString() << "is empty.");
		return;
	}

	// 清空旧点云
	current_cloud->clear();
	cloudHistoryStack = std::stack<pcl::PointCloud<pcl::PointXYZ>::Ptr>();
	LOG_INFO("Load new cloud, undo stack cleared.");

	bool isload = false;

	// 加载 PCD
	if (path.endsWith(".pcd", Qt::CaseInsensitive))
	{
		if (pcl::io::loadPCDFile(path.toStdString(), *current_cloud) == 0)
		{
			isload = true;
		}
	}
	// 加载 PLY
	else if (path.endsWith(".ply", Qt::CaseInsensitive))
	{
		if (pcl::io::loadPLYFile(path.toStdString(), *current_cloud) == 0)
		{
			isload = true;
		}
	}

	if (!isload){
		// 黄色警告窗口，标题error，内容load error
		QMessageBox::warning(this, "error", "load error.");
		return;
	}

	QMessageBox::information(this, "success",
		QString("load success. size：%1").arg(current_cloud->size())
	);

	// 调用你自己的打印信息函数
	mypcl::printPointCloudBasicInfo(current_cloud);

}

// 加载多幅点云
void MainWindow::loadMultiCloud()
{
	// 多选文件
	QStringList paths = QFileDialog::getOpenFileNames(this, "加载多幅点云", "", "Point Cloud (*.pcd *.ply)");
	if (paths.isEmpty()) return;

	// 每次加载 → 清空历史
	m_cloud_list.clear();
	ui.te_log->clear();  // 清空日志区域以显示加载列表
	m_cloud_list.clear();

	m_coarse_reg_clouds.clear();
	m_coarse_accumulated.clear();
	m_coarse_transforms.clear();

	m_fine_reg_clouds.clear();
	m_fine_accumulated.clear();
	m_fine_transforms.clear();

	m_registration_result->clear();


	// 批量加载
	for (int i = 0; i < paths.size(); ++i){
		QString path = paths[i];
		PointCloudPtr cloud(new pcl::PointCloud<pcl::PointXYZ>);
		bool ok = false;

		if (path.endsWith(".pcd", Qt::CaseInsensitive)){
			if (pcl::io::loadPCDFile(path.toStdString(), *cloud) == 0){
				ok = true;
			}
		}
		// 加载 PLY
		else if (path.endsWith(".ply", Qt::CaseInsensitive)){
			if (pcl::io::loadPLYFile(path.toStdString(), *cloud) == 0){
				ok = true;
			}
		}

		if (ok){
			m_cloud_list.push_back(cloud);
			ui.te_log->append(QString::number(i + 1) + ": " + path);
			LOG_INFO("Loaded: " << path.toStdString());
		}
	}

	QMessageBox::information(this, "完成", QString("成功加载 %1 幅点云").arg(m_cloud_list.size()));
}
#pragma endregion

#pragma region 保存 
// =============保存=============

void MainWindow::savePointCloud()
{
    QWidget* currentTab = ui.Left_tab->currentWidget();

	if (currentTab == ui.tab_single)
	{
		saveSingleCloud();
	}
	else
	{
		saveMultiCloud();
	}
}

void MainWindow::saveSingleCloud()
{
	QString baseDir =
		QFileDialog::getExistingDirectory(
			this,
			"选择保存目录"
		);

	if (baseDir.isEmpty())
		return;

	QString timeStr =
		QDateTime::currentDateTime()
		.toString("yyyyMMdd_hhmmss");

	QString exportDir =
		baseDir + "/SingleCloud_" + timeStr;

	QDir().mkpath(exportDir);

	int saveCount = 0;

	try{
		// 原始点云
		if (current_cloud && !current_cloud->empty()){
			QString path =
				exportDir + "/cloud.pcd";

			pcl::io::savePCDFileBinary(
				path.toStdString(),
				*current_cloud
			);

			saveCount++;
		}

		// 法线点云
		if (current_cloud &&
			normal_cloud &&
			!current_cloud->empty() &&
			!normal_cloud->empty())
		{
			pcl::PointCloud<pcl::PointNormal>::Ptr cloudNormal(
				new pcl::PointCloud<pcl::PointNormal>
			);

			pcl::concatenateFields(
				*current_cloud,
				*normal_cloud,
				*cloudNormal
			);

			QString path =
				exportDir + "/cloud_normal.pcd";

			pcl::io::savePCDFileBinary(
				path.toStdString(),
				*cloudNormal
			);

			saveCount++;
		}

		// Mesh
		if (mesh && !mesh->polygons.empty())
		{
			QString path =
				exportDir + "/mesh.ply";

			pcl::io::savePLYFile(
				path.toStdString(),
				*mesh
			);

			saveCount++;
		}

		LOG_INFO("Single cloud export finished.");

		QMessageBox::information(
			this,
			"完成",
			QString("成功导出 %1 个文件\n\n%2")
			.arg(saveCount)
			.arg(exportDir)
		);
	}
	catch (...)
	{
		QMessageBox::critical(
			this,
			"错误",
			"保存失败！"
		);
	}
}

void MainWindow::saveMultiCloud()
{
	QString baseDir =
		QFileDialog::getExistingDirectory(
			this,
			"选择保存目录"
		);

	if (baseDir.isEmpty())
		return;

	QString timeStr =
		QDateTime::currentDateTime()
		.toString("yyyyMMdd_hhmmss");

	QString exportDir =
		baseDir + "/MultiCloud_" + timeStr;

	QDir().mkpath(exportDir);

	int saveCount = 0;

	try
	{
		// 原始加载点云
		for (size_t i = 0; i < m_cloud_list.size(); i++)
		{
			if (!m_cloud_list[i] ||
				m_cloud_list[i]->empty())
				continue;

			QString path =
				exportDir +
				QString("/raw_%1.pcd").arg(i);

			pcl::io::savePCDFileBinary(
				path.toStdString(),
				*m_cloud_list[i]
			);

			saveCount++;
		}

		// 粗配准最终结果
		if (!m_coarse_accumulated.empty())
		{
			QString path =
				exportDir +
				"/coarse_result.pcd";

			pcl::io::savePCDFileBinary(
				path.toStdString(),
				*m_coarse_accumulated.back()
			);

			saveCount++;
		}

		// 精配准最终结果
		if (!m_fine_accumulated.empty())
		{
			QString path =
				exportDir +
				"/fine_result.pcd";

			pcl::io::savePCDFileBinary(
				path.toStdString(),
				*m_fine_accumulated.back()
			);

			saveCount++;
		}

		// 当前最终结果
		if (m_registration_result &&
			!m_registration_result->empty())
		{
			QString path =
				exportDir +
				"/registration_result.pcd";

			pcl::io::savePCDFileBinary(
				path.toStdString(),
				*m_registration_result
			);

			saveCount++;
		}

		LOG_INFO("Multi cloud export finished.");

		QMessageBox::information(
			this,
			"完成",
			QString("成功导出 %1 个文件\n\n%2")
			.arg(saveCount)
			.arg(exportDir)
		);
	}
	catch (...)
	{
		QMessageBox::critical(
			this,
			"错误",
			"保存失败！"
		);
	}
}

#pragma endregion

#pragma region 其他工具栏

// =============撤销=============

void MainWindow::undoCloudOperation()
{
	// 栈空没法撤销
	if (cloudHistoryStack.empty())
	{
		LOG_ERROR("No operation to undo.");
		QMessageBox::warning(this, "提示", "暂无操作可撤销！");
		return;
	}

	// 取出上一版点云，覆盖当前
	current_cloud = cloudHistoryStack.top();
	cloudHistoryStack.pop();

	LOG_INFO("Undo success, stack remain size: " << cloudHistoryStack.size());

	// 撤销后自动刷新点云信息
	showCloudInfo();
}


// =============清空=============
void MainWindow::clearData()
{
	current_cloud->clear();
	normal_cloud->clear();

	mesh->polygons.clear();

	m_cloud_list.clear();

	m_coarse_reg_clouds.clear();
	m_coarse_accumulated.clear();
	m_coarse_transforms.clear();

	m_fine_reg_clouds.clear();
	m_fine_accumulated.clear();
	m_fine_transforms.clear();

	m_registration_result->clear();

	ui.te_Info->clear();

	LOG_INFO("All data cleared.");

	QMessageBox::information(
		this,
		"完成",
		"当前工程数据已清空"
	);
}


void MainWindow::showCloudInfo(){

	// 防御检查
	if (!current_cloud || current_cloud->empty()) {
		LOG_ERROR("No point cloud available.");
		QMessageBox::warning(this, "警告", "暂无点云数据！");
		return;
	}

	// ===================== 核心：捕获 cout 输出 =====================
	std::stringstream buffer;
	std::streambuf* old_buf = std::cout.rdbuf(buffer.rdbuf());

	// 调用你原来的函数（完全不改）
	mypcl::printPointCloudBasicInfo(current_cloud);

	// 恢复 cout
	std::cout.rdbuf(old_buf);

	// 抓到所有输出内容
	std::string log_text = buffer.str();

	// ===================== 同时在控制台再打一遍 =====================
	LOG_INFO("----------------------------------------");
	LOG_INFO("Printing current point cloud information...");
	std::cout << log_text << std::endl;
	LOG_INFO("----------------------------------------");

	// ===================== 显示到 UI 文本框 =====================
	ui.te_Info->clear();
	ui.te_Info->append(QString::fromStdString(log_text));

}

// 完整日志
void MainWindow::showFullLog()
{
	QMessageBox::information(
		this,
		"完整日志",
		ui.te_log->toPlainText()
	);
}

// 工具说明
void MainWindow::showIntroduction()
{
	QString text =
		"点云处理系统\n\n"
		"单点云处理：\n"
		"预处理\n"
		"法线估计\n"
		"表面重建\n\n"
		"多点云处理：\n"
		"批量预处理\n"
		"粗配准(SAC-IA)\n"
		"精配准(ICP)\n";

	QMessageBox::information(
		this,
		"工具说明",
		text
	);
}

// 居中显示
void MainWindow::centerView()   //TODO
{
	QMessageBox::information(
		this,
		"提示",
		"当前版本暂不支持"
	);
}

#pragma endregion

#pragma endregion


#pragma region 单个点云处理

// =============预处理模块=============

void MainWindow::removeNaN()
{
    pushCloudToUndoStack();
    if (!current_cloud || current_cloud->empty()) {
        QMessageBox::warning(this, "警告", "暂无点云数据！");
        return;
    }
    mypcl::removeNaN(current_cloud);
    showCloudInfo();	// 自动刷新信息
	refreshViewer();	// 自动更新界面
}

void MainWindow::voxelDownSample()
{
    pushCloudToUndoStack();
    if (!current_cloud || current_cloud->empty()) {
        QMessageBox::warning(this, "警告", "暂无点云数据！");
        return;
    }

    bool ok = false;
    double leaf = QInputDialog::getDouble(this, "参数输入",
        "请输入体素大小 (默认 0.1)：", 0.1, 0.001, 10.0, 3, &ok);

    if (!ok) {
        LOG_ERROR("Voxel downsample canceled.");
        return;
    }

    mypcl::doVoxelFilter(current_cloud, leaf);
    showCloudInfo(); // 自动刷新信息
	refreshViewer();	// 自动更新界面
}

void MainWindow::uniformFilter()
{
    pushCloudToUndoStack();
    if (!current_cloud || current_cloud->empty()) {
        QMessageBox::warning(this, "警告", "暂无点云数据！");
        return;
    }

    bool ok = false;
    double radius = QInputDialog::getDouble(this, "参数输入",
        "请输入均匀采样半径 (默认 0.1)：", 0.1, 0.001, 10.0, 3, &ok);

    if (!ok) {
        LOG_ERROR("Uniform sampling canceled.");
        return;
    }

    mypcl::doUniformSampling(current_cloud, radius);
    showCloudInfo(); // 自动刷新信息
	refreshViewer();	// 自动更新界面
}

void MainWindow::passThroughFilter()
{
    pushCloudToUndoStack();
    if (!current_cloud || current_cloud->empty()) {
        QMessageBox::warning(this, "警告", "暂无点云数据！");
        return;
    }

    QStringList fields = { "x", "y", "z" };
    QString field = QInputDialog::getItem(this, "选择轴", "请选择过滤轴：", fields, 0, false, nullptr);
    if (field.isEmpty()) return;

    bool ok1, ok2;
    float min = QInputDialog::getDouble(this, "最小值", "输入最小值：", -10, -1e6, 1e6, 3, &ok1);
    float max = QInputDialog::getDouble(this, "最大值", "输入最大值：", 10, -1e6, 1e6, 3, &ok2);

    if (!ok1 || !ok2) {
        LOG_ERROR("PassThrough canceled.");
        return;
    }

    mypcl::doPassThrough(current_cloud, field.toStdString(), min, max);
    showCloudInfo(); // 自动刷新信息
	refreshViewer();	// 自动更新界面
}

void MainWindow::statisticalOutlierRemoval()
{
    pushCloudToUndoStack();
    if (!current_cloud || current_cloud->empty()) {
        QMessageBox::warning(this, "警告", "暂无点云数据！");
        return;
    }

    bool ok1, ok2;
    int k = QInputDialog::getInt(this, "参数", "邻域点数 meanK (默认 30)：", 30, 5, 200, 1, &ok1);
    double std = QInputDialog::getDouble(this, "参数", "标准差倍数 (默认 1.0)：", 1.0, 0.1, 5.0, 1, &ok2);

    if (!ok1 || !ok2) {
        LOG_ERROR("Statistical remover canceled.");
        return;
    }

    mypcl::doStatisticalOutlierRemoval(current_cloud, k, std);
    showCloudInfo(); // 自动刷新信息
	refreshViewer();	// 自动更新界面
}

void MainWindow::radiusOutlierRemoval()
{
    pushCloudToUndoStack();
    if (!current_cloud || current_cloud->empty()) {
        QMessageBox::warning(this, "警告", "暂无点云数据！");
        return;
    }

    bool ok1, ok2;
    double r = QInputDialog::getDouble(this, "参数", "搜索半径 (默认 1.0)：", 1.0, 0.01, 10, 2, &ok1);
    int min = QInputDialog::getInt(this, "参数", "最小邻域点数 (默认 20)：", 20, 1, 200, 1, &ok2);

    if (!ok1 || !ok2) {
        LOG_ERROR("Radius remover canceled.");
        return;
    }

    mypcl::doRadiusOutlierRemoval(current_cloud, r, min);
    showCloudInfo(); // 自动刷新信息
	refreshViewer();	// 自动更新界面
}

void MainWindow::mlsSmoothProcess()
{
    pushCloudToUndoStack();
    if (!current_cloud || current_cloud->empty()) {
        QMessageBox::warning(this, "警告", "暂无点云数据！");
        return;
    }

    bool ok = false;
    double radius = QInputDialog::getDouble(this, "参数输入",
        "MLS搜索半径 (默认 0.1)：", 0.1, 0.001, 10.0, 3, &ok);

    if (!ok) {
        LOG_ERROR("MLS smooth canceled.");
        return;
    }

    // 输出临时变量
    PointCloudPtr smooth_cloud;
    NormalCloudPtr norm_cloud;

    // 调用你的函数
    mypcl::mlsSmooth(current_cloud, radius, smooth_cloud, norm_cloud);

    // 覆盖全局点云
    current_cloud = smooth_cloud;
    normal_cloud = norm_cloud;

    showCloudInfo(); // 自动刷新信息
	refreshViewer();	// 自动更新界面
}


// ================特征处理模块=================

void MainWindow::estimateNormal()
{
    pushCloudToUndoStack();
    if (!current_cloud || current_cloud->empty()) {
        QMessageBox::warning(this, "警告", "暂无点云数据！");
        return;
    }

    bool ok = false;
    double radius = QInputDialog::getDouble(this, "参数输入",
        "法向量搜索半径 (默认 0.1)：", 0.1, 0.001, 10.0, 3, &ok);

    if (!ok) {
        LOG_ERROR("Normal estimation canceled.");
        return;
    }

    mypcl::computeNormals(current_cloud, normal_cloud, radius);
}

void MainWindow::orientNormalConsistent()
{
    pushCloudToUndoStack();
    if (!current_cloud || current_cloud->empty() || !normal_cloud || normal_cloud->empty()) {
        QMessageBox::warning(this, "警告", "请先加载点云并计算法线！");
        return;
    }

    bool ok = false;
    int k = QInputDialog::getInt(this, "参数", "BFS传播邻域点数 (默认20)：", 20, 5, 100, 1, &ok);
    if (!ok) {
        LOG_ERROR("Normal align canceled.");
        return;
    }

    mypcl::alignNormalsConsistently(normal_cloud, current_cloud, k);
}

void MainWindow::flipNormalToViewpoint()
{
    if (!current_cloud || current_cloud->empty() || !normal_cloud || normal_cloud->empty()) {
        QMessageBox::warning(this, "警告", "请先加载点云并计算法线！");
        return;
    }

    mypcl::flipNormalsToViewpoint(normal_cloud, current_cloud, 0, 0, 0);
}

void MainWindow::evaluateNormalDisorder()
{
    if (!current_cloud || current_cloud->empty() || !normal_cloud || normal_cloud->empty()) {
        QMessageBox::warning(this, "警告", "请先加载点云并计算法线！");
        return;
    }
    double chaos = mypcl::evaluateNormalChaos(normal_cloud, current_cloud);
}

// =============== 表面重建 ===============

void MainWindow::greedyProjectionTriangulation()
{
    if (!current_cloud || current_cloud->empty() || !normal_cloud || normal_cloud->empty()) {
        QMessageBox::warning(this, "警告", "请先加载点云并计算法线！");
        return;
    }

    bool ok = false;
    double radius = QInputDialog::getDouble(this, "参数输入",
        "贪婪投影搜索半径 (默认 0.25)：", 0.25, 0.01, 10.0, 3, &ok);

    if (!ok) {
        LOG_ERROR("Greedy projection canceled.");
        return;
    }

    // 初始化网格
    if (!mesh) mesh.reset(new pcl::PolygonMesh);
    mypcl::greedyProjectionTriangulation(current_cloud, normal_cloud, mesh, radius);
}

void MainWindow::poissonReconstruction()
{
    if (!current_cloud || current_cloud->empty() || !normal_cloud || normal_cloud->empty()) {
        QMessageBox::warning(this, "警告", "请先加载点云并计算法线！");
        return;
    }

    bool ok = false;
    int depth = QInputDialog::getInt(this, "参数输入",
        "Poisson重建深度 (默认8，推荐8~12)：", 8, 6, 12, 1, &ok);

    if (!ok) {
        LOG_ERROR("Poisson reconstruction canceled.");
        return;
    }

    if (!mesh) mesh.reset(new pcl::PolygonMesh);
    mypcl::poissonReconstruction(current_cloud, normal_cloud, mesh, depth);
}

void MainWindow::evaluateReconstructionError()
{
    if (!current_cloud || current_cloud->empty() || !mesh || mesh->polygons.empty()) {
        LOG_ERROR("No point cloud or mesh available for error evaluation.");
        QMessageBox::warning(this, "警告", "请先完成重建！");
        return;
    }
 
    double error = mypcl::computeReconstructionError(current_cloud, mesh, "point");
}

// ======================其他======================

void MainWindow::pushCloudToUndoStack()
{
    if (!current_cloud || current_cloud->empty())
        return;

    // 深拷贝一份，存入栈
    pcl::PointCloud<pcl::PointXYZ>::Ptr temp_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    *temp_cloud = *current_cloud;

    cloudHistoryStack.push(temp_cloud);

    // 超过最大步数，弹出最旧的
    if (cloudHistoryStack.size() > MAX_UNDO_STEP)
    {
        cloudHistoryStack.pop();
    }

    LOG_INFO("Push current cloud to undo stack. Stack size: " << cloudHistoryStack.size());
}
# pragma endregion


#pragma region ==========多幅点云处理模块============

// 一键预处理（批量：去NaN + 降采样 + 去噪）

void MainWindow::on_btn_preprocess_clicked()
{
    if (m_cloud_list.empty())
    {
        QMessageBox::warning(this, "警告", "请先加载点云！");
        return;
    }

    bool ok = false;
    double leaf = QInputDialog::getDouble(this, "降采样", "体素大小", 0.01, 0, 10, 3, &ok);
    if (!ok) return;

    LOG_INFO("Start batch preprocess...");

    for (auto& cloud : m_cloud_list)
    {
        mypcl::removeNaN(cloud);
        mypcl::doVoxelFilter(cloud, leaf);
        mypcl::doStatisticalOutlierRemoval(cloud, 10, 1.0);
    }

    LOG_INFO("Batch preprocess finished!");
}

// 多幅粗配准
void MainWindow::on_btn_coarse_clicked()
{
    if (m_cloud_list.size() < 2)
    {
        QMessageBox::warning(this, "警告", "至少需要2幅点云！");
        return;
    }

    bool ok1, ok2;
    double fpfh_r = QInputDialog::getDouble(this, "FPFH半径", "FPFH半径", 
        5*mypcl::computeAveragePointDistance(m_cloud_list[0]), 0, 10, 3, &ok1);
    double sac_r = QInputDialog::getDouble(this, "SAC半径", "SAC半径", 
        10 * mypcl::computeAveragePointDistance(m_cloud_list[0]), 0, 10, 3, &ok2);
    if (!ok1 || !ok2) return;

    mypcl::multipleCoarseRegistration(
        m_cloud_list,
        m_coarse_reg_clouds,
        m_coarse_accumulated,
        m_coarse_transforms,
        fpfh_r,
        sac_r
    );

    m_registration_result = m_coarse_accumulated.back();
    
    QMessageBox::information(this, "完成", "粗配准完成！");
}

// 多幅精配准
void MainWindow::on_btn_fine_clicked()
{
    if (m_coarse_accumulated.empty())
    {
        QMessageBox::warning(this, "警告", "请先执行粗配准！");
        return;
    }

    bool ok1, ok2;
    double max_d = QInputDialog::getDouble(this, "ICP最大距离", "ICP最大距离", 1.0, 0, 10, 3, &ok1);
    double leaf = QInputDialog::getDouble(this, "体素大小", "体素大小",
        mypcl::computeAveragePointDistance(m_cloud_list[0]), 0.01, 1, 2, &ok2);
    if (!ok1 || !ok2) return;

    mypcl::multipleFineRegistration(
        m_cloud_list,
        m_fine_reg_clouds,
        m_fine_accumulated,
        m_fine_transforms,
        max_d,
        leaf
    );

    m_registration_result = m_fine_accumulated.back();

    QMessageBox::information(this, "完成", "精配准完成！");
}

// 显示配准结果
void MainWindow::on_btn_showReg_clicked()
{
    if (!m_registration_result || m_registration_result->empty())
    {
        QMessageBox::warning(this, "警告", "无配准结果！");
        return;
    }

    std::thread([this]() {
        pcl::visualization::PCLVisualizer viewer("Registration Result");
        viewer.setBackgroundColor(0, 0, 0);
        viewer.addCoordinateSystem(1.0);
        viewer.addPointCloud(m_registration_result, "result");
        viewer.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "result");

        while (!viewer.wasStopped())
            viewer.spinOnce(10);
        }).detach();
}



#pragma endregion


#pragma region ===========可视化模块============
/*
// 初始化
void MainWindow::initViewer()
{
	viewer.reset(new pcl::visualization::PCLVisualizer("viewer", false));

	viewer->setBackgroundColor(0, 0, 0);
	viewer->addCoordinateSystem(1.0);

	vtkRenderWindow* window = viewer->getRenderWindow();

	window->SetParentId((void*)ui.widget_viewer->winId());

	window->SetSize(
		ui.widget_viewer->width()*1.25,
		ui.widget_viewer->height()*1.25
	);

	viewer->resetCamera();

	window->Render();
}

void MainWindow::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);

	static bool inited = false;
	if (!inited)
	{
		initViewer();
		inited = true;
	}
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
	QMainWindow::resizeEvent(event);

	if (viewer)
	{
		vtkRenderWindow* window = viewer->getRenderWindow();

		window->SetSize(
			ui.widget_viewer->width() * 1.25,
			ui.widget_viewer->height() * 1.25
		);

		window->Render();
	}
}
*/

// 初始化

void MainWindow::renderThreadFunc()
{
	// 创建窗口（第一次启动时）
	persistent_viewer.reset(new pcl::visualization::PCLVisualizer("点云工具 - 可视化窗口"));
	persistent_viewer->setBackgroundColor(0, 0, 0);
	persistent_viewer->addCoordinateSystem(1.0);
	// 设置点云大小等默认属性（可选）
	viewer_created = true;

	while (!render_stop)
	{
		// 检查是否有待处理的数据更新
		bool needUpdate = false;
		PointCloudPtr newCloud;
		NormalCloudPtr newNormals;
		PolygonMeshPtr newMesh;
		ViewerMode newMode = ViewerMode::CLOUD;

		{
			std::lock_guard<std::mutex> lock(data_mutex);
			if (has_pending_data)
			{
				needUpdate = true;
				newCloud = pending_cloud;
				newNormals = pending_normals;
				newMesh = pending_mesh;
				newMode = pending_mode;
				has_pending_data = false;  // 清空标记
			}
		}

		if (needUpdate)
		{
			// 清除所有旧内容
			persistent_viewer->removeAllPointClouds();
			persistent_viewer->removeAllShapes();

			// 根据模式显示不同数据
			switch (newMode)
			{
			case ViewerMode::CLOUD:
				if (newCloud && !newCloud->empty())
				{
					persistent_viewer->addPointCloud(newCloud, "cloud");
					persistent_viewer->setPointCloudRenderingProperties(
						pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "cloud");
				}
				break;
			case ViewerMode::NORMAL:
				if (newCloud && !newCloud->empty() && newNormals && !newNormals->empty())
				{
					// 使用 addPointCloudNormals 直接绘制法线箭头
					// 参数：点云、法线云、箭头缩放系数（1.0）、箭头显示密度（每N个点显示一个）
					persistent_viewer->addPointCloudNormals<PointXYZ, Normal>(
						newCloud, newNormals,
						1,      // 箭头长度缩放
						0.05,   // 箭头显示密度（每20个点显示1个，0.05表示5%的点显示箭头）
						"normals"
					);
					// 可选：同时用点云颜色作为背景
					persistent_viewer->addPointCloud(newCloud, "cloud");
					persistent_viewer->setPointCloudRenderingProperties(
						pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "cloud");
				}
				else
				{
					// 如果法线为空，降级为普通点云显示
					if (newCloud && !newCloud->empty())
					{
						persistent_viewer->addPointCloud(newCloud, "cloud");
					}
					ui.te_log->append("警告：法线数据缺失，仅显示原始点云");
				}
				break;
			case ViewerMode::MESH:
				if (newMesh && !newMesh->polygons.empty())
				{
					persistent_viewer->addPolygonMesh(*newMesh, "mesh");
				}
				break;
			case ViewerMode::REGISTRATION:
				if (newCloud && !newCloud->empty())
				{
					persistent_viewer->addPointCloud(newCloud, "registration");
					persistent_viewer->setPointCloudRenderingProperties(
						pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "registration");
				}
				break;
			}

			// 重置相机以适应新内容
			persistent_viewer->resetCamera();
			// 强制窗口弹出到最前面（增加用户体验）
			persistent_viewer->getRenderWindow()->Render();
		}

		// 处理事件（非阻塞，10ms超时）
		persistent_viewer->spinOnce(10);

		// 如果用户关闭了窗口，重建它
		if (persistent_viewer->wasStopped())
		{
			persistent_viewer.reset(new pcl::visualization::PCLVisualizer("点云工具 - 可视化窗口"));
			persistent_viewer->setBackgroundColor(0, 0, 0);
			persistent_viewer->addCoordinateSystem(1.0);
			// 重建后需要重新设置viewer_created标志
			viewer_created = true;
			// 继续循环，不需要break
		}
	}

	// 线程结束前清理
	persistent_viewer.reset();
	viewer_created = false;
}

void MainWindow::updateViewer(PointCloudPtr cloud, ViewerMode mode)
{
	std::lock_guard<std::mutex> lock(data_mutex);
	// 深拷贝一份，防止主线程修改
	pending_cloud.reset(new PointCloud(*cloud));
	pending_mesh.reset();  // 清空mesh
	pending_mode = mode;
	has_pending_data = true;
}

void MainWindow::updateViewer(PolygonMeshPtr mesh, ViewerMode mode)
{
	std::lock_guard<std::mutex> lock(data_mutex);
	pending_mesh.reset(new pcl::PolygonMesh(*mesh));
	pending_cloud.reset();  // 清空cloud
	pending_mode = mode;
	has_pending_data = true;
}

void MainWindow::updateViewer(PointCloudPtr cloud, NormalCloudPtr normals, ViewerMode mode)
{
	std::lock_guard<std::mutex> lock(data_mutex);
	// 深拷贝点云
	if (cloud) {
		pending_cloud.reset(new PointCloud(*cloud));
	}
	// 深拷贝法线云
	if (normals) {
		pending_normals.reset(new NormalCloud(*normals));
	}
	pending_mesh.reset();
	pending_mode = mode;
	has_pending_data = true;
}

// 根据当前视图模式刷新可视化窗口（不改变模式）
void MainWindow::refreshViewer() {
	if (!current_cloud || current_cloud->empty()) return;

	switch (m_viewer_mode) {
		case ViewerMode::CLOUD:
			updateViewer(current_cloud, ViewerMode::CLOUD);
			break;
		case ViewerMode::NORMAL:
			if (normal_cloud && !normal_cloud->empty())
				updateViewer(current_cloud, normal_cloud, ViewerMode::NORMAL);
			else
				updateViewer(current_cloud, ViewerMode::CLOUD); // 降级
			break;
		case ViewerMode::MESH:
			if (mesh && !mesh->polygons.empty())
				updateViewer(mesh, ViewerMode::MESH);
			else
				updateViewer(current_cloud, ViewerMode::CLOUD); // 降级
			break;
		default:
			updateViewer(current_cloud, ViewerMode::CLOUD);
			break;
	}
}

// =============单点云可视化模块=============

/**
* @brief 显示xyz点云
*/
void MainWindow::showPointCloud()
{
	if (!current_cloud || current_cloud->empty()) return;
	updateViewer(current_cloud, ViewerMode::CLOUD);
	m_viewer_mode = ViewerMode::CLOUD;

	//// 创建子线程显示点云，为了不卡住Qt主窗口
	//std::thread([this]() {
	//	pcl::visualization::PCLVisualizer viewer("PointViewer");
	//	viewer.setBackgroundColor(0, 0, 0);
	//	viewer.addCoordinateSystem(1.0);
	//	viewer.addPointCloud(current_cloud, "cloud");
	//	viewer.setPointCloudRenderingProperties(
	//		pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "cloud");

	//	while (!viewer.wasStopped())
	//	{
	//		viewer.spinOnce(10);
	//		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//	}
	//	}).detach();
}

void MainWindow::showNormalCloud()
{
	if (!normal_cloud || normal_cloud->empty()) {
		ui.te_log->append("法向量点云为空，请先计算法向量");
		return;
	}
	if (!current_cloud || current_cloud->empty()) {
		ui.te_log->append("原始点云为空，无法显示法线");
		return;
	}

	ui.te_log->append(QString("正在显示法线：%1 个点").arg(normal_cloud->size()));

	// 调用新的接口，同时传入点云和法线
	updateViewer(current_cloud, normal_cloud, ViewerMode::NORMAL);
	m_viewer_mode = ViewerMode::NORMAL;


	//std::thread([this]() {
	//	pcl::visualization::PCLVisualizer viewer("Normal Cloud Viewer");
	//	viewer.setBackgroundColor(0, 0, 0);
	//	viewer.addCoordinateSystem(1.0);
	//	viewer.addPointCloud(current_cloud, "cloud");
	//	viewer.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "cloud");

	//	// 显示法线（每5个点显示一根，线长0.05）
	//	viewer.addPointCloudNormals<pcl::PointXYZ, pcl::Normal>(current_cloud, normal_cloud, 5, 0.05, "normals");
	//	viewer.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 0, 1, 0, "normals");

	//	while (!viewer.wasStopped()) {
	//		viewer.spinOnce(10);
	//		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//	}
	//	LOG_INFO("Normal viewer closed.");
	//	}).detach();
}

void MainWindow::showMesh()
{
	if (!mesh || mesh->polygons.empty()) return;
	updateViewer(mesh, ViewerMode::MESH);
	m_viewer_mode = ViewerMode::MESH;

	LOG_INFO("Opening mesh viewer...");


	//std::thread([this]() {
	//	pcl::visualization::PCLVisualizer viewer("Mesh Viewer");
	//	viewer.setBackgroundColor(0, 0, 0);
	//	viewer.addCoordinateSystem(1.0);
	//	viewer.addPolygonMesh(*mesh, "mesh");

	//	while (!viewer.wasStopped()) {
	//		viewer.spinOnce(10);
	//		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//	}
	//	LOG_INFO("Mesh viewer closed.");
	//	}).detach();
}

#pragma endregion
