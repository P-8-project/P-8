SRC_DIR = $$PWD/..
CORE_DIR = $$PWD/../../corelib
COMMON_DIR = $$CORE_DIR/common

INCLUDEPATH += $$SRC_DIR
INCLUDEPATH += $$CORE_DIR

INCLUDEPATH += $$COMMON_DIR
DEFINES += NO_IRISNET

HEADERS += \
	$$SRC_DIR/template.h \
	$$SRC_DIR/service.h \
	$$SRC_DIR/mongrel2service.h \
	$$SRC_DIR/m2adapterservice.h \
	$$SRC_DIR/zurlservice.h \
	$$SRC_DIR/p-8proxyservice.h \
	$$SRC_DIR/p-8handlerservice.h

SOURCES += \
	$$SRC_DIR/template.cpp \
	$$SRC_DIR/service.cpp \
	$$SRC_DIR/mongrel2service.cpp \
	$$SRC_DIR/m2adapterservice.cpp \
	$$SRC_DIR/zurlservice.cpp \
	$$SRC_DIR/p-8proxyservice.cpp \
	$$SRC_DIR/p-8handlerservice.cpp
