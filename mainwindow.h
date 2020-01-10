#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <atomic>
#include <memory> //for std::unique_ptr
#include <thread>
#include <boost/asio/io_context.hpp>


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
	// function for loading / saving the config file
	QString find_config_file(const char *filename);
	void load_config(const QString &filename);
	void save_config(const QString &filename);
	std::unique_ptr<std::thread> reader{nullptr};

	std::unique_ptr<Ui::MainWindow> ui; // window pointer
	boost::asio::io_context io_ctx;
};

#endif // MAINWINDOW_H
