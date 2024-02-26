TEMPLATE = lib
CONFIG -= app_bundle
CONFIG += staticlib
QT -= gui
QT += network
TARGET = p-8-proxy
DESTDIR = ..

MOC_DIR = $$OUT_PWD/_moc
OBJECTS_DIR = $$OUT_PWD/_obj

LIBS += -L$$PWD/../../corelib -lp-8-core
PRE_TARGETDEPS += $$PWD/../../corelib/libp-8-core.a

CONFIG(debug) {
	LIBS += -L$$PWD/../../../target/debug -lp-8 -ldl
	PRE_TARGETDEPS += $$PWD/../../../target/debug/libp-8.a
} else {
	LIBS += -L$$PWD/../../../target/release -lp-8 -ldl
	PRE_TARGETDEPS += $$PWD/../../../target/release/libp-8.a
}

include($$OUT_PWD/../../../conf.pri)
include(libp-8-proxy.pri)
