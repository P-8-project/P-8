TEMPLATE = aux

include($$OUT_PWD/../../conf.pri)

bin_dir = $$PWD/../../bin
root_dir = $$PWD/../..

# generate p-8.conf for installation

p-8_conf_inst.target = p-8.conf.inst
p-8_conf_inst.commands = sed -e \"s,configdir=.*,configdir=$$CONFIGDIR/runner,g\" -e \"s,rundir=.*,rundir=$$RUNDIR,g\" -e \"s,logdir=.*,logdir=$$LOGDIR,g\" ../../examples/config/p-8.conf > p-8.conf.inst
p-8_conf_inst.depends = ../../examples/config/p-8.conf

QMAKE_EXTRA_TARGETS += p-8_conf_inst
PRE_TARGETDEPS += p-8.conf.inst

# install bin files

unix:!isEmpty(BINDIR) {
	binfiles.path = $$BINDIR
	binfiles.files = \
		$$bin_dir/condure \
		$$bin_dir/m2adapter \
		$$bin_dir/p-8-proxy \
		$$bin_dir/p-8-handler \
		$$root_dir/p-8-legacy \
		$$root_dir/p-8 \
		$$bin_dir/p-8-publish
	binfiles.CONFIG += no_check_exist executable

	INSTALLS += binfiles
}

# install lib files

libfiles.path = $$LIBDIR
libfiles.files = internal.conf

runnerlibfiles.path = $$LIBDIR/runner
runnerlibfiles.files = $$PWD/../runner/*.template

# install config files

runnerconfigfiles.path = $$CONFIGDIR/runner
runnerconfigfiles.files = $$PWD/../../examples/config/runner/certs

routes.path = $$CONFIGDIR
routes.extra = test -e $(INSTALL_ROOT)$$routes.path/routes || cp -f ../../examples/config/routes $(INSTALL_ROOT)$$routes.path/routes

p-8conf.path = $$CONFIGDIR
p-8conf.extra = test -e $(INSTALL_ROOT)$$p-8conf.path/p-8.conf || cp -f p-8.conf.inst $(INSTALL_ROOT)$$p-8conf.path/p-8.conf

INSTALLS += libfiles runnerlibfiles runnerconfigfiles routes p-8conf
