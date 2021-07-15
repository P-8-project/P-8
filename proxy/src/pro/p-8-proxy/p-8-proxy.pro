CONFIG += console
CONFIG -= app_bundle
QT -= gui
QT += network
TARGET = p-8-proxy
DESTDIR = ../../..

MOC_DIR = $$OUT_PWD/_moc
OBJECTS_DIR = $$OUT_PWD/_obj

LIBS += -L$$PWD/../.. -lp-8-proxy

include($$OUT_PWD/../../../conf.pri)
include(p-8-proxy.pri)

unix:!isEmpty(BINDIR) {
	target.path = $$BINDIR
	INSTALLS += target
}
