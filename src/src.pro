TEMPLATE = subdirs

include($$OUT_PWD/../conf.pri)

rust.subdir = rust

cpp.subdir = cpp
cpp.depends = rust

m2adapter.subdir = m2adapter
m2adapter.depends = cpp

proxy.subdir = proxy
proxy.depends = cpp

handler.subdir = handler
handler.depends = cpp

runner.subdir = runner
runner.depends = cpp

p-8.subdir = p-8

SUBDIRS += \
	rust \
	cpp \
	m2adapter \
	proxy \
	handler \

SUBDIRS += \
	runner \
	p-8
