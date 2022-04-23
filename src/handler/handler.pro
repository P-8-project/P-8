TEMPLATE = subdirs

libp-8_handler.subdir = libp-8-handler
p-8_handler.subdir = p-8-handler
p-8_handler.depends = libp-8_handler
tests.subdir = tests
tests.depends = libp-8_handler

tests.CONFIG += no_default_install

SUBDIRS += \
	libp-8_handler \
	p-8_handler \
	tests
