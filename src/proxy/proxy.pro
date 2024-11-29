CONFIG += console
CONFIG -= app_bundle
QT -= gui
QT += network
TARGET = p-8-proxy
DESTDIR = ../../bin

MOC_DIR = $$OUT_PWD/_moc
OBJECTS_DIR = $$OUT_PWD/_obj

LIBS += -L$$PWD/../cpp -lp-8-cpp
PRE_TARGETDEPS += $$PWD/../cpp/libp-8-cpp.a

include($$OUT_PWD/../rust/lib.pri)
include($$OUT_PWD/../../conf.pri)
include(proxy.pri)

unix:!isEmpty(BINDIR) {
	target.path = $$BINDIR
	INSTALLS += target
}
