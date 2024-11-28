QMAKE_CXXFLAGS += $$(CXXFLAGS)
QMAKE_CFLAGS += $$(CFLAGS)
QMAKE_LFLAGS += $$(LDFLAGS)

SRC_DIR = $$PWD/..
CPP_DIR = $$PWD/../../cpp

INCLUDEPATH += $$SRC_DIR
INCLUDEPATH += $$CPP_DIR

DEFINES += NO_IRISNET

HEADERS += \
	$$SRC_DIR/template.h \
	$$SRC_DIR/service.h \
	$$SRC_DIR/listenport.h \
	$$SRC_DIR/condureservice.h \
	$$SRC_DIR/mongrel2service.h \
	$$SRC_DIR/m2adapterservice.h \
	$$SRC_DIR/zurlservice.h \
	$$SRC_DIR/p-8proxyservice.h \
	$$SRC_DIR/p-8handlerservice.h

SOURCES += \
	$$SRC_DIR/template.cpp \
	$$SRC_DIR/service.cpp \
	$$SRC_DIR/condureservice.cpp \
	$$SRC_DIR/mongrel2service.cpp \
	$$SRC_DIR/m2adapterservice.cpp \
	$$SRC_DIR/zurlservice.cpp \
	$$SRC_DIR/p-8proxyservice.cpp \
	$$SRC_DIR/p-8handlerservice.cpp
