#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <atomic>
#include <memory> //for std::unique_ptr
#include <thread>


#define WIN32_LEAN_AND_MEAN
#include "windows.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(QWidget *parent, const char *config_file);
	~MainWindow() noexcept override;

private slots:
	void closeEvent(QCloseEvent *ev) override;
	void toggleRecording();

private:
	QString find_config_file(const char *filename);
	void load_config(const QString &filename);
	void save_config(const QString &filename);
	std::unique_ptr<std::thread> reader{nullptr};

	std::unique_ptr<Ui::MainWindow> ui; // window pointer
	std::atomic<bool> shutdown{false};	// flag indicating whether the recording thread should quit
};

#endif // MAINWINDOW_H
