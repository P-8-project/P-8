TEMPLATE = lib
CONFIG -= app_bundle
CONFIG += staticlib
QT -= gui
QT += network
TARGET = runner
DESTDIR = ..

MOC_DIR = $$OUT_PWD/_moc
OBJECTS_DIR = $$OUT_PWD/_obj

LIBS += -L$$PWD/../../cpp -lp-8-cpp
PRE_TARGETDEPS += $$PWD/../../cpp/libp-8-cpp.a

include($$OUT_PWD/../../../conf.pri)
include(librunner.pri)
