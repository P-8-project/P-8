TEMPLATE = subdirs

libp-8_proxy.subdir = libp-8-proxy
p-8_proxy.subdir = p-8-proxy
p-8_proxy.depends = libp-8_proxy
tests.subdir = tests
tests.depends = libp-8_proxy

tests.CONFIG += no_default_install

SUBDIRS += \
	libp-8_proxy \
	p-8_proxy \
	tests
