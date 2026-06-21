#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

// ui类前置声明
namespace Ui {
	class new_MainWindow;
}

class new_MainWindow : public QMainWindow
{
	Q_OBJECT  // Qt信号槽宏，必须写
public:
	explicit new_MainWindow(QWidget* parent = nullptr);
	~new_MainWindow();

private:
	Ui::new_MainWindow* ui;
};

#endif // NEW_MAINWINDOW_H