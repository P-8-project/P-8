TEMPLATE = aux

include(conf.pri)

root_dir = $$PWD/..
bin_dir = $$root_dir/bin

RELEASE = $$(RELEASE)

!isEmpty(RELEASE) {
	target_dir = $$root_dir/target/release
} else {
	target_dir = $$root_dir/target/debug
}

# copy bin files

connmgr_bin.target = $$bin_dir/p-8-connmgr
connmgr_bin.depends = $$target_dir/p-8-connmgr
connmgr_bin.commands = mkdir -p $$bin_dir && cp -a $$target_dir/p-8-connmgr $$bin_dir/p-8-connmgr && ln -sf p-8-connmgr $$bin_dir/p-8-condure

m2adapter_bin.target = $$bin_dir/m2adapter
m2adapter_bin.depends = $$target_dir/m2adapter
m2adapter_bin.commands = mkdir -p $$bin_dir && cp -a $$target_dir/m2adapter $$bin_dir/m2adapter

proxy_bin.target = $$bin_dir/p-8-proxy
proxy_bin.depends = $$target_dir/p-8-proxy
proxy_bin.commands = mkdir -p $$bin_dir && cp -a $$target_dir/p-8-proxy $$bin_dir/p-8-proxy

handler_bin.target = $$bin_dir/p-8-handler
handler_bin.depends = $$target_dir/p-8-handler
handler_bin.commands = mkdir -p $$bin_dir && cp -a $$target_dir/p-8-handler $$bin_dir/p-8-handler

runner_bin.target = $$root_dir/p-8
runner_bin.depends = $$target_dir/p-8
runner_bin.commands = cp -a $$target_dir/p-8 $$root_dir/p-8

publish_bin.target = $$bin_dir/p-8-publish
publish_bin.depends = $$target_dir/p-8-publish
publish_bin.commands = mkdir -p $$bin_dir && cp -a $$target_dir/p-8-publish $$bin_dir/p-8-publish

QMAKE_EXTRA_TARGETS += \
	connmgr_bin \
	m2adapter_bin \
	proxy_bin \
	handler_bin \
	runner_bin \
	publish_bin

PRE_TARGETDEPS += \
	$$bin_dir/p-8-connmgr \
	$$bin_dir/m2adapter \
	$$bin_dir/p-8-proxy \
	$$bin_dir/p-8-handler \
	$$root_dir/p-8 \
	$$bin_dir/p-8-publish

# generate p-8.conf for installation

p-8_conf_inst.target = p-8.conf.inst
p-8_conf_inst.commands = sed -e \"s,configdir=.*,configdir=$$CONFIGDIR/runner,g\" -e \"s,rundir=.*,rundir=$$RUNDIR,g\" -e \"s,logdir=.*,logdir=$$LOGDIR,g\" ../examples/config/p-8.conf > p-8.conf.inst
p-8_conf_inst.depends = ../examples/config/p-8.conf conf.pri

QMAKE_EXTRA_TARGETS += p-8_conf_inst
PRE_TARGETDEPS += p-8.conf.inst

# install bin files

unix:!isEmpty(BINDIR) {
	binfiles.path = $$BINDIR
	binfiles.files = \
		$$bin_dir/p-8-connmgr \
		$$bin_dir/m2adapter \
		$$bin_dir/p-8-proxy \
		$$bin_dir/p-8-handler \
		$$root_dir/p-8 \
		$$bin_dir/p-8-publish
	binfiles.CONFIG += no_check_exist executable

	symlinks.path = $$BINDIR
	symlinks.extra = ln -sf p-8-connmgr $(INSTALL_ROOT)$$symlinks.path/p-8-condure

	INSTALLS += binfiles symlinks
}

# install lib files

libfiles.path = $$LIBDIR
libfiles.files = $$PWD/../src/internal.conf

runnerlibfiles.path = $$LIBDIR/runner
runnerlibfiles.files = $$PWD/../src/runner/*.template

# install config files

runnerconfigfiles.path = $$CONFIGDIR/runner
runnerconfigfiles.files = $$PWD/../examples/config/runner/certs

routes.path = $$CONFIGDIR
routes.extra = test -e $(INSTALL_ROOT)$$routes.path/routes || cp -f ../examples/config/routes $(INSTALL_ROOT)$$routes.path/routes

p-8conf.path = $$CONFIGDIR
p-8conf.extra = test -e $(INSTALL_ROOT)$$p-8conf.path/p-8.conf || cp -f p-8.conf.inst $(INSTALL_ROOT)$$p-8conf.path/p-8.conf

INSTALLS += libfiles runnerlibfiles runnerconfigfiles routes p-8conf
