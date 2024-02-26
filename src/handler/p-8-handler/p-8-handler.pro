CONFIG += console
CONFIG -= app_bundle
QT -= gui
QT += network
DESTDIR = ../../../bin

MOC_DIR = $$OUT_PWD/_moc
OBJECTS_DIR = $$OUT_PWD/_obj

LIBS += -L$$PWD/.. -lp-8-handler
PRE_TARGETDEPS += $$PWD/../libp-8-handler.a

LIBS += -L$$PWD/../../corelib -lp-8-core
PRE_TARGETDEPS += $$PWD/../../corelib/libp-8-core.a

include($$OUT_PWD/../../../conf.pri)
include($$OUT_PWD/../../rust/lib.pri)
include(p-8-handler.pri)

unix:!isEmpty(BINDIR) {
	target.path = $$BINDIR
	INSTALLS += target
}
