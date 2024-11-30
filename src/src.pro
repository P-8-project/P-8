TEMPLATE = subdirs

include($$OUT_PWD/../conf.pri)

cpp.subdir = cpp

rust.subdir = rust
rust.depends = cpp

p-8.subdir = p-8

SUBDIRS += \
	cpp \
	rust \
	p-8
