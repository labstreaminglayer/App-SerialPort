QT += core gui widgets

CONFIG += c++11

TARGET = App-SerialPort
TEMPLATE = app

INCLUDEPATH += \
    ../boost_1_70_0 \
    ../LSL/include

LIBS += \
    -L../boost_1_70_0/lib64-msvc-14.1 \
             -lboost_thread-vc141-mt-x64-1_70 \
    -L../LSL/lib \
             -lliblsl64

FORMS += \
    mainwindow.ui

HEADERS += \
    mainwindow.h

SOURCES += \
    main.cpp \
    mainwindow.cpp
