#include "new_mainwindow.h"
#include "ui_new_mainwindow.h" // AUTOUIC自动生成，不用手动建文件

new_MainWindow::new_MainWindow(QWidget* parent)
	: QMainWindow(parent), ui(new Ui::new_MainWindow)
{
	ui->setupUi(this); // 绑定ui/new_mainwindow.ui界面
}

new_MainWindow::~new_MainWindow()
{
	delete ui;
}