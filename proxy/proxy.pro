TEMPLATE = subdirs

sub_libp-8_proxy.subdir = src/pro/libp-8-proxy
sub_p-8_proxy.subdir = src/pro/p-8-proxy
sub_p-8_proxy.depends = sub_libp-8_proxy
sub_tests.subdir = tests
sub_tests.depends = sub_libp-8_proxy

SUBDIRS += \
	sub_libp-8_proxy \
	sub_p-8_proxy \
	sub_tests
