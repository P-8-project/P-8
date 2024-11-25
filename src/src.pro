TEMPLATE = subdirs

include($$OUT_PWD/../conf.pri)

cpp.subdir = cpp

rust.subdir = rust
rust.depends = cpp

cpp_tests.subdir = cpp/tests
cpp_tests.depends = cpp rust
cpp_tests.CONFIG += no_default_install

p-8.subdir = p-8

SUBDIRS += \
	cpp \
	rust \
	cpp_tests \
	p-8
