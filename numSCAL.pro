#-------------------------------------------------
#
# Project created by QtCreator 2014-01-02T17:45:02
#
#-------------------------------------------------

QT       += core

QT       += gui

QT       += opengl

QT       += printsupport

TARGET = numSCAL
CONFIG   += console
CONFIG   -= app_bundle
CONFIG += c++11

TEMPLATE = app

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

win32 {
    LIBS += -lopengl32 $$PWD/Glew/glew32.dll
    LIBS += -L$$PWD/Glew/ -lglew32
    LIBS += -L$$PWD/Glew/ -lglew32s
}

unix {
    LIBS += -lGLEW
}


SOURCES += main.cpp \
    pore.cpp \
    node.cpp \
    network.cpp \
    mainwindow.cpp \
    cluster.cpp \
    widget3d.cpp \
    hoshenKopelmann.cpp \
    loadData.cpp \
    element.cpp \
    solver.cpp \
    generationRegular.cpp \
    generationExtracted.cpp \
    misc.cpp \
    qcustomplot.cpp \
    outputData.cpp \
    films.cpp \
    relativePermeabilities.cpp \
    steadyTwoPhaseFlow.cpp \
    unsteadyTwoPhaseFlow.cpp \
    unsteadyTracerFlow.cpp \
    randomGenerator.cpp

HEADERS += \
    pore.h \
    node.h \
    network.h \
    tools.h \
    mainwindow.h \
    widget3d.h \
    cluster.h \
    element.h \
    qcustomplot.h \
    shader.h \
    iterator.h \
    randomGenerator.h

OTHER_FILES +=

FORMS += \
    mainwindow.ui

CONFIG += warn_off

RESOURCES += \
    resources.qrc
