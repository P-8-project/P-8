CONFIG *= console testcase
CONFIG -= app_bundle
QT -= gui
QT *= network testlib

TESTS_DIR = $$PWD
SRC_DIR = $$PWD/..
CORE_DIR = $$PWD/../../corelib
QZMQ_DIR = $$CORE_DIR/qzmq

LIBS += -L$$SRC_DIR -lp-8-handler
PRE_TARGETDEPS += $$PWD/../libp-8-handler.a

LIBS += -L$$PWD/../../corelib -lp-8-core
PRE_TARGETDEPS += $$PWD/../../corelib/libp-8-core.a

include($$PWD/../../rust/lib.pri)
include($$PWD/../../../conf.pri)

INCLUDEPATH += $$SRC_DIR
INCLUDEPATH += $$CORE_DIR
INCLUDEPATH += $$QZMQ_DIR/src

DEFINES += NO_IRISNET
