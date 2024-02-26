CONFIG *= console testcase
CONFIG -= app_bundle
QT -= gui
QT *= network testlib

TESTS_DIR = $$PWD
SRC_DIR = $$PWD/..
CORE_DIR = $$PWD/../../corelib
QZMQ_DIR = $$CORE_DIR/qzmq
COMMON_DIR = $$CORE_DIR/common

LIBS += -L$$SRC_DIR -lp-8-handler
PRE_TARGETDEPS += $$PWD/../libp-8-handler.a

LIBS += -L$$PWD/../../corelib -lp-8-core
PRE_TARGETDEPS += $$PWD/../../corelib/libp-8-core.a

include($$PWD/../../../conf.pri)

CONFIG(debug, debug|release) {
	LIBS += -L$$PWD/../../../target/debug -lp-8 -ldl
	PRE_TARGETDEPS += $$PWD/../../../target/debug/libp-8.a
} else {
	LIBS += -L$$PWD/../../../target/release -lp-8 -ldl
	PRE_TARGETDEPS += $$PWD/../../../target/release/libp-8.a
}

INCLUDEPATH += $$SRC_DIR
INCLUDEPATH += $$CORE_DIR
INCLUDEPATH += $$QZMQ_DIR/src

INCLUDEPATH += $$COMMON_DIR
DEFINES += NO_IRISNET
