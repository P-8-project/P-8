CONFIG += console
CONFIG -= app_bundle
QT -= gui
QT += network
TARGET = p-8
DESTDIR = ../../..

MOC_DIR = $$OUT_PWD/_moc
OBJECTS_DIR = $$OUT_PWD/_obj

LIBS += -L$$PWD/.. -lrunner
PRE_TARGETDEPS += $$PWD/../librunner.a

LIBS += -L$$PWD/../../corelib -lp-8-core
PRE_TARGETDEPS += $$PWD/../../corelib/libp-8-core.a

include($$OUT_PWD/../../../conf.pri)
include(runner.pri)

unix:!isEmpty(BINDIR) {
	target.path = $$BINDIR
	INSTALLS += target
}
