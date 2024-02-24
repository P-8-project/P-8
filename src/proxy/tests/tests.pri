CONFIG *= console testcase
CONFIG -= app_bundle
QT -= gui
QT *= network testlib

TESTS_DIR = $$PWD
SRC_DIR = $$PWD/..
CORE_DIR = $$PWD/../../corelib
QZMQ_DIR = $$CORE_DIR/qzmq
COMMON_DIR = $$CORE_DIR/common

LIBS += -L$$SRC_DIR -lp-8-proxy
PRE_TARGETDEPS += $$PWD/../libp-8-proxy.a

LIBS += -L$$PWD/../../corelib -lp-8-core
PRE_TARGETDEPS += $$PWD/../../corelib/libp-8-core.a

CONFIG(release) {
	LIBS += -L$$PWD/../../../target/release -lp-8 -ldl
} else {
	LIBS += -L$$PWD/../../../target/debug -lp-8 -ldl
}

include($$PWD/../../../conf.pri)

INCLUDEPATH += $$SRC_DIR
INCLUDEPATH += $$CORE_DIR
INCLUDEPATH += $$QZMQ_DIR/src

INCLUDEPATH += $$COMMON_DIR
DEFINES += NO_IRISNET
