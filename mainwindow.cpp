#include "mainwindow.hpp"
#include "ui_mainwindow.h"
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
                          "\nLSL library info:" + lsl::library_info();
        QMessageBox::about(this, "About this app", infostr);
    });
    connect(ui->linkButton, &QPushButton::clicked, this, &MainWindow::toggleRecording);


    //: At the end of the constructor, we load the supplied config file or find it
    //: in one of the default paths
    QString cfgfilepath = find_config_file(config_file);
    load_config(cfgfilepath);
}

void MainWindow::load_config(const QString &filename) {
    QSettings settings(filename, QSettings::Format::IniFormat);
    ui->comPort->setValue(settings.value("coresettings.comport", 1).toInt());
    ui->baudRate->setValue(settings.value("coresettings.baudrate", 57600).toInt());
    ui->samplingRate->setValue(settings.value("streamsettings.samplingrate", 0).toInt());
    ui->chunkSize->setValue(settings.value("streamsettings.chunksize", 32).toInt());
    ui->input_name->setText(settings.value("streamsettings.streamname", "SerialPort").toString());
    ui->dataBits->setCurrentIndex(settings.value("miscsettings.databits", 4).toInt());
    ui->parity->setCurrentIndex(settings.value("miscsettings.parity", 0).toInt());
    ui->stopBits->setCurrentIndex(settings.value("miscsettings.stopbits", 0).toInt());
    ui->readIntervalTimeout->setValue(
            settings.value("timeoutsettings.readintervaltimeout", 500).toInt());
    ui->readTotalTimeoutConstant->setValue(
            settings.value("timeoutsettings.readtotaltimeoutconstant", 50).toInt());
    ui->readTotalTimeoutMultiplier->setValue(
            settings.value("timeoutsettings.readtotaltimeoutmultiplier", 10).toInt());
}

//: Save function, same as above
void MainWindow::save_config(const QString &filename) {
    QSettings settings(filename, QSettings::Format::IniFormat);
    settings.beginGroup("coresettings");
    settings.setValue("comport", ui->comPort->value());
    settings.setValue("baudrate", ui->baudRate->value());
    settings.endGroup();
    settings.beginGroup("streamsettings");
    settings.setValue("samplingrate", ui->samplingRate->value());
    settings.setValue("chunksize", ui->chunkSize->value());
    settings.setValue("streamname", ui->input_name->text());
    settings.endGroup();
    settings.beginGroup("miscsettings");
    settings.setValue("databits", ui->dataBits->currentIndex());
    settings.setValue("parity", ui->parity->currentIndex());
    settings.setValue("stopbits", ui->stopBits->currentIndex());
    settings.endGroup();
    settings.beginGroup("timeoutsettings");
    settings.setValue("readintervaltimeout", ui->readIntervalTimeout->value());
    settings.setValue("readtotaltimeoutconstant", ui->readTotalTimeoutConstant->value());
    settings.setValue("readtotaltimeoutmultiplier", ui->readTotalTimeoutMultiplier->value());
    settings.sync();
}

void MainWindow::closeEvent(QCloseEvent *ev) {
    if (reader) {
        QMessageBox::warning(this, "Recording still running", "Can't quit while recording");
        ev->ignore();
    }
}


// background data reader thread
void read_thread(HANDLE hPort, int comPort, int baudRate, int samplingRate, int chunkSize,
                 const std::string streamName, std::atomic<bool> &shutdown) {
    try {

        // create streaminfo
        lsl::stream_info info(streamName,"Raw",1,samplingRate,lsl::cf_int16,std::string("SerialPort_") + streamName);
        // append some meta-data
        lsl::xml_element channels = info.desc().append_child("channels");
        channels.append_child("channel")
                .append_child_value("label","Channel1")
                .append_child_value("type","Raw")
                .append_child_value("unit","integer");
        info.desc().append_child("acquisition")
                .append_child("hardware")
                .append_child_value("com_port", std::to_string(comPort))
                .append_child_value("baud_rate",std::to_string(baudRate));

        // make a new outlet
        lsl::stream_outlet outlet(info,chunkSize);

        // enter transmission loop
        unsigned char byte;
        short sample;
        unsigned long bytes_read;
        while (!shutdown) {
            // get a sample
            ReadFile(hPort,&byte,1,&bytes_read,NULL); sample = byte;
            // transmit it
            if (bytes_read)
                outlet.push_sample(&sample);
        }
    }
    catch(std::exception &e) {
        // any other error
//		QMessageBox::critical(this,"Error",(std::string("Error during processing: ")+=e.what()).c_str(),QMessageBox::Ok);
    }
    CloseHandle(hPort);
}

//: ## Toggling the recording state
//: Our record button has two functions: start a recording and
//: stop it if a recording is running already.
void MainWindow::toggleRecording() {
    /*: the `std::unique_ptr` evaluates to false if it doesn't point to an object,
     * so we need to start a recording.
     * First, we load the configuration from the UI fields, set the shutdown flag
     * to false so the recording thread doesn't quit immediately and create the
     * recording thread. */
    if (!reader) {
        HANDLE hPort = NULL;
        try {

            // get the UI parameters...
            int comPort = ui->comPort->value();
            int baudRate = ui->baudRate->value();
            int samplingRate = ui->samplingRate->value();
            int chunkSize = ui->chunkSize->value();
            std::string streamName = ui->input_name->text().toStdString();
            int dataBits = ui->dataBits->currentIndex() + 4;
            int parity = ui->parity->currentIndex();
            int stopBits = ui->stopBits->currentIndex();
            int readIntervalTimeout = ui->readIntervalTimeout->value();
            int readTotalTimeoutConstant = ui->readTotalTimeoutConstant->value();
            int readTotalTimeoutMultiplier = ui->readTotalTimeoutMultiplier->value();

            // try to open the serial port
            std::string fname = "\\\\.\\COM" + std::to_string(comPort);
            hPort = CreateFileA(fname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, 0);
            if (hPort == INVALID_HANDLE_VALUE)
                throw std::runtime_error(
                        "Could not open serial port. Please make sure that you are using the right COM "
                        "port and that the device is ready.");

            // try to set up serial port parameters
            DCB dcbSerialParams = {0};
            if (!GetCommState(hPort, &dcbSerialParams))
                QMessageBox::critical(
                        this, "Error", "Could not get COM port state.", QMessageBox::Ok);
            dcbSerialParams.BaudRate = baudRate;
            dcbSerialParams.ByteSize = dataBits;
            dcbSerialParams.StopBits = stopBits;
            dcbSerialParams.Parity = parity;
            if (!SetCommState(hPort, &dcbSerialParams))
                QMessageBox::critical(this, "Error", "Could not set baud rate.", QMessageBox::Ok);

            // try to set timeouts
            COMMTIMEOUTS timeouts = {0};
            if (!GetCommTimeouts(hPort, &timeouts))
                QMessageBox::critical(
                        this, "Error", "Could not get COM port timeouts.", QMessageBox::Ok);
            timeouts.ReadIntervalTimeout = readIntervalTimeout;
            timeouts.ReadTotalTimeoutConstant = readTotalTimeoutConstant;
            timeouts.ReadTotalTimeoutMultiplier = readTotalTimeoutMultiplier;
            if (!SetCommTimeouts(hPort, &timeouts))
                QMessageBox::critical(
                        this, "Error", "Could not set COM port timeouts.", QMessageBox::Ok);
            reader = std::make_unique<std::thread>([&]() {
                read_thread(
                        hPort, comPort, baudRate, samplingRate, chunkSize, streamName, shutdown);
            });

        } catch (std::exception &e) {
            if (hPort != INVALID_HANDLE_VALUE) CloseHandle(hPort);
            QMessageBox::critical(this, "Error",
                                  QStringLiteral("Error during initialization: ") + e.what(), QMessageBox::Ok);
            return;
        }
        ui->linkButton->setText("Unlink");
    } else {
        shutdown = true;
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


//: Tell the compiler to put the default destructor in this object file
MainWindow::~MainWindow() noexcept = default;