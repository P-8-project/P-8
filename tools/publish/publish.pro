CONFIG += console
CONFIG -= app_bundle
QT -= gui
QT += network
TARGET = p-8-publish
DESTDIR = ../../bin

MOC_DIR = $$OUT_PWD/_moc
OBJECTS_DIR = $$OUT_PWD/_obj

include($$OUT_PWD/../../conf.pri)

CORE_DIR = $$PWD/../../src/corelib
QZMQ_DIR = $$CORE_DIR/qzmq
COMMON_DIR = $$CORE_DIR/common

INCLUDEPATH += $$CORE_DIR
INCLUDEPATH += $$QZMQ_DIR/src

INCLUDEPATH += $$COMMON_DIR
DEFINES += NO_IRISNET

LIBS += -L$$PWD/../../src/corelib -lp-8-core
PRE_TARGETDEPS += $$PWD/../../src/corelib/libp-8-core.a

SOURCES += main.cpp

unix:!isEmpty(BINDIR) {
	target.path = $$BINDIR
	INSTALLS += target
}
