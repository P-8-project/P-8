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

CONFIG(debug) {
	LIBS += -L$$PWD/../../../target/debug -lp-8 -ldl
	PRE_TARGETDEPS += $$PWD/../../../target/debug/libp-8.a
} else {
	LIBS += -L$$PWD/../../../target/release -lp-8 -ldl
	PRE_TARGETDEPS += $$PWD/../../../target/release/libp-8.a
}

include(p-8-handler.pri)

unix:!isEmpty(BINDIR) {
	target.path = $$BINDIR
	INSTALLS += target
}
