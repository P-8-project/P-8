TEMPLATE = subdirs

include($$OUT_PWD/../conf.pri)

rust.subdir = rust

corelib.subdir = corelib
corelib.depends = rust

m2adapter.subdir = m2adapter
m2adapter.depends = corelib

proxy.subdir = proxy
proxy.depends = corelib

handler.subdir = handler
handler.depends = corelib

runner.subdir = runner
runner.depends = corelib

p-8.subdir = p-8

SUBDIRS += \
	rust \
	corelib \
	m2adapter \
	proxy \
	handler \

SUBDIRS += \
	runner \
	p-8
