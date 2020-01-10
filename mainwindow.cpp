#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <fstream>
#include <lsl_cpp.h>
#include <string>
#include <vector>
#include <boost/asio.hpp>

#include "serialport.h"

MainWindow::MainWindow(QWidget *parent, const char *config_file)
	: QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);
	connect(ui->actionLoad_Configuration, &QAction::triggered, [this]() {
		load_config(QFileDialog::getOpenFileName(
			this, "Load Configuration File", "", "Configuration Files (*.cfg)"));
	});
	connect(ui->actionSave_Configuration, &QAction::triggered, [this]() {
		save_config(QFileDialog::getSaveFileName(
			this, "Save Configuration File", "", "Configuration Files (*.cfg)"));
	});
	connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
	connect(ui->actionAbout, &QAction::triggered, [this]() {
		QString infostr = QStringLiteral("LSL library version: ") +
						  QString::number(lsl::library_version()) +
						  "\nLSL library info:" + lsl::lsl_library_info();
		QMessageBox::about(this, "About this app", infostr);
	});
	connect(ui->linkButton, &QPushButton::clicked, this, &MainWindow::toggleRecording);

	QString cfgfilepath = find_config_file(config_file);
	load_config(cfgfilepath);
}


void MainWindow::load_config(const QString &filename) {
	QSettings settings(filename, QSettings::Format::IniFormat);
	ui->input_streamname->setText(settings.value("Stream/name", "Default name").toString());
	ui->input_samplingrate->setValue(settings.value("Stream/samplingrate", 0).toInt());
	ui->input_comport->setText(settings.value("serial/comport", "COM1").toString());
	ui->input_baudrate->setCurrentText(settings.value("serial/baudrate", 115200).toString());
	ui->input_databits->setValue(settings.value("serial/databits", 8).toInt());
	ui->input_parity->setCurrentIndex(settings.value("serial/parity", 0).toInt());
	ui->input_stopbits->setCurrentIndex(settings.value("serial/stopbits", 0).toInt());
}

void MainWindow::save_config(const QString &filename) {
	QSettings settings(filename, QSettings::Format::IniFormat);
	settings.beginGroup("Stream");
	settings.setValue("name", ui->input_streamname->text());
	settings.setValue("samplingrate", ui->input_samplingrate->value());
	settings.beginGroup("serial");
	settings.setValue("comport", ui->input_comport->text());
	settings.setValue("baudrate", ui->input_baudrate->currentText());
	settings.setValue("databits", ui->input_databits->value());
	settings.setValue("parity", ui->input_parity->currentIndex());
	settings.setValue("stopbits", ui->input_stopbits->currentIndex());
	settings.beginGroup("TCP");
	settings.setValue("port", ui->input_tcpport->text());
	settings.sync();
}

void MainWindow::closeEvent(QCloseEvent *ev) {
	if (reader) {
		QMessageBox::warning(this, "Recording still running", "Can't quit while recording");
		ev->ignore();
	}
}

enum class OutletType { singlechar = 0, singlestring = 1, linestring = 2 };

// background data reader thread
void recording_thread_function(
	OutletType type, int chunksize, std::unique_ptr<lsl::stream_outlet>&& outlet, AsioReader&& reader) {
	try {
		auto streaminfo{outlet->info()};
		std::string buf;
		while(true) {
			double ts = reader.readLine(buf, 2000);
			std::cout << "Read: " << buf;
			outlet->push_sample(&buf, ts);
		}


	} catch (std::exception &e) {
		// any other error
		QMessageBox::critical(nullptr, "Error",
			(std::string("Error during processing: ") += e.what()).c_str(), QMessageBox::Ok);
	}
}

void MainWindow::toggleRecording() {
	if(reader->joinable()) {
		reader->join();
		reader.reset();
	}
	if (!reader) {
		try {
			auto outlettype = static_cast<OutletType>(ui->input_outlettype->currentIndex());
			auto name = ui->input_streamname->text().toStdString();
			auto rate = ui->input_samplingrate->value();
			lsl::channel_format_t fmt{outlettype == OutletType::singlechar ? lsl::cf_int8 : lsl::cf_string};
			std::string type{outlettype == OutletType::singlechar ? "Raw" : "Markers"};
			lsl::stream_info streaminfo(name, type, 1, rate, fmt);
			io_ctx.restart();
			 auto samplingRate = ui->input_samplingrate->value();
			auto chunksize = ui->input_chunksize->value();

			auto outlet = std::make_unique<lsl::stream_outlet>(streaminfo);
			switch(ui->tabs_source->currentIndex()) {
			case 0:
			{
				// Serial port
				// get the UI parameters...
				SerialPort sp(io_ctx,
							  ui->input_comport->text().toStdString(),
							  ui->input_baudrate->currentText().toInt(),
							  ui->input_databits->value(),
							  ui->input_parity->currentIndex(),
							  ui->input_stopbits->currentIndex());
				//reader = std::make_unique<std::thread>(recording_thread_function);
				break;
			}
			case 1: {
				int tcpport = ui->input_tcpport->value();
				//recording_thread_function(outlettype, chunksize, std::move(outlet), NetPort(io_ctx, static_cast<uint16_t>(tcpport)));
				reader = std::make_unique<std::thread>(recording_thread_function, outlettype, chunksize, std::move(outlet), NetPort(io_ctx, static_cast<uint16_t>(tcpport)));

				break;
			}
			}


			// start reading
		} catch (std::exception &e) {
			QMessageBox::critical(this, "Error",
				QStringLiteral("Error during initialization: ") + e.what(),
				QMessageBox::Ok);
			return;
		}

		// done, all successful
		ui->linkButton->setText("Unlink");
	} else {
		reader->join();
		reader.reset();
		ui->linkButton->setText("Link");
	}
}

/**
 * Find a config file to load. This is (in descending order or preference):
 * - a file supplied on the command line
 * - [executablename].cfg in one the the following folders:
 *	- the current working directory
 *	- the default config folder, e.g. '~/Library/Preferences' on OS X
 *	- the executable folder
 * @param filename	Optional file name supplied e.g. as command line parameter
 * @return Path to a found config file
 */
QString MainWindow::find_config_file(const char *filename) {
	if (filename) {
		QString qfilename(filename);
		if (!QFileInfo::exists(qfilename))
			QMessageBox(QMessageBox::Warning, "Config file not found",
				QStringLiteral("The file '%1' doesn't exist").arg(qfilename), QMessageBox::Ok,
				this);
		else
			return qfilename;
	}
	QFileInfo exeInfo(QCoreApplication::applicationFilePath());
	QString defaultCfgFilename(exeInfo.completeBaseName() + ".cfg");
	QStringList cfgpaths;
	cfgpaths << QDir::currentPath()
			 << QStandardPaths::standardLocations(QStandardPaths::ConfigLocation) << exeInfo.path();
	for (auto path : cfgpaths) {
		QString cfgfilepath = path + QDir::separator() + defaultCfgFilename;
		if (QFileInfo::exists(cfgfilepath)) return cfgfilepath;
	}
	QMessageBox(QMessageBox::Warning, "No config file not found",
		QStringLiteral("No default config file could be found"), QMessageBox::Ok, this);
	return "";
}


MainWindow::~MainWindow() noexcept = default;
