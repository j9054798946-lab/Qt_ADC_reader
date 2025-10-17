QT += core widgets network

CONFIG += c++17

TARGET = LEDMonitor
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    ledwidget.cpp

HEADERS += \
    mainwindow.h \
    ledwidget.h
