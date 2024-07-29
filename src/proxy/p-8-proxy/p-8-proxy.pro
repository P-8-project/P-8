CONFIG += console
CONFIG -= app_bundle
QT -= gui
QT += network
TARGET = p-8-proxy
DESTDIR = ../../../bin

MOC_DIR = $$OUT_PWD/_moc
OBJECTS_DIR = $$OUT_PWD/_obj

LIBS += -L$$PWD/.. -lp-8-proxy
PRE_TARGETDEPS += $$PWD/../libp-8-proxy.a

LIBS += -L$$PWD/../../corelib -lp-8-core
PRE_TARGETDEPS += $$PWD/../../corelib/libp-8-core.a

include($$OUT_PWD/../../rust/lib.pri)
include($$OUT_PWD/../../../conf.pri)
include(p-8-proxy.pri)

unix:!isEmpty(BINDIR) {
	target.path = $$BINDIR
	INSTALLS += target
}
