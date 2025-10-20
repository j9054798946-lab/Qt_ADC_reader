QT += core widgets network

CONFIG += c++17

TARGET = LEDMonitor
TEMPLATE = app

SOURCES += \
    graphwidget.cpp \
    main.cpp \
    mainwindow.cpp \
    ledwidget.cpp

HEADERS += \
    graphwidget.h \
    mainwindow.h \
    ledwidget.h
