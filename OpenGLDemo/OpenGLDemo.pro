#-------------------------------------------------
#
# Project created by QtCreator 2019-05-09T23:12:01
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = OpenGLDemo
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
# 指定库
DEFINES += QT_DEPRECATED_WARNINGS

# 指定库文件路径
# (Win32) LIBS += D:/QT/.....
# (Liunx) LIBS += -L/usr/local/.....  或 LIBS += -l .lib    -L指定一个库目录 -l指定一个具体的库

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

#指定头文件和源文件   \表示换行
SOURCES += \
        main.cpp \
        widget.cpp

HEADERS += \
        widget.h

# Default rules for deployment(部署).
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    shader.qrc
