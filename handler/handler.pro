TEMPLATE = subdirs

sub_libp-8_handler.subdir = src/pro/libp-8-handler
sub_p-8_handler.subdir = src/pro/p-8-handler
sub_p-8_handler.depends = sub_libp-8_handler
sub_tests.subdir = tests
sub_tests.depends = sub_libp-8_handler

sub_tests.CONFIG += no_default_install

SUBDIRS += \
	sub_libp-8_handler \
	sub_p-8_handler \
	sub_tests
