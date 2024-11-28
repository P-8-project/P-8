CONFIG *= console testcase
CONFIG -= app_bundle
QT -= gui
QT *= network testlib

TESTS_DIR = $$PWD
SRC_DIR = $$PWD/..
CPP_DIR = $$PWD/../../cpp
QZMQ_DIR = $$CPP_DIR/qzmq

LIBS += -L$$SRC_DIR -lp-8-handler
PRE_TARGETDEPS += $$PWD/../libp-8-handler.a

LIBS += -L$$PWD/../../cpp -lp-8-cpp
PRE_TARGETDEPS += $$PWD/../../cpp/libp-8-cpp.a

include($$PWD/../../rust/lib.pri)
include($$PWD/../../../conf.pri)

INCLUDEPATH += $$SRC_DIR
INCLUDEPATH += $$CPP_DIR
INCLUDEPATH += $$QZMQ_DIR/src

DEFINES += NO_IRISNET
